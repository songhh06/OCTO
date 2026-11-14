extends "res://scripts/octopus_preview.gd"
## Actual C++ solver snapshots; offline playback, never a live GDExtension claim.
var recording: Dictionary = {}
var frames: Array = []
var playback_seconds := 0.0
var last_frame := 0
var lab_capture := false
var rigids: Array[MeshInstance3D] = []
const DRAW_SCALE := 2.3

func _ready() -> void:
    super._ready()
    DisplayServer.window_set_title("OCTOPUS | Native C++ physics trace | OFFLINE playback")
    capture_mode = false
    lab_capture = "--lab-capture" in OS.get_cmdline_user_args()
    var path := get_trace_path()
    var file := FileAccess.open(path, FileAccess.READ)
    if file == null:
        push_error("PHYSICS_TRACE_MISSING: run tools/physics-lab.ps1 first")
        get_tree().quit(2)
        return
    var parsed = JSON.parse_string(file.get_as_text())
    if not parsed is Dictionary or parsed.get("source") != "native-cxx20-cosserat" or not parsed.get("success", false):
        push_error("Invalid or failed native physics recording")
        get_tree().quit(3)
        return
    recording = parsed
    frames = recording.frames
    if frames.size() < 2:
        get_tree().quit(4)
        return
    for child in find_children("*", "Label", true, false):
        if child.text.begins_with("VISUAL PROTOTYPE"):
            child.text = "NATIVE C++ PHYSICS TRACE  /  OFFLINE PLAYBACK  /  BIO UNVALIDATED"
        elif child.text == "O C T O P U S":
            child.text = "P H Y S I C S   L A B"
    motion_label.text = "Drag: orbit | Wheel: zoom | 1/2/3: view | Space: pause | R: reset camera | P: screenshot"
    for body in frames[0].rigids:
        var box := BoxMesh.new()
        box.size = read_vec(body.half_extents) * DRAW_SCALE * 2.0
        rigids.append(mesh_node(box, material(Color("6599b1"), 0.35)))
    yaw = 0.8
    pitch = 0.48
    update_camera()
    update_arms()
    print("PHYSICS_LAB_READY frames=%d arms=%d source=native-cxx20 offline=true" % [frames.size(), frames[0].arms.size()])

func get_trace_path() -> String:
    return "res://data/physics_trace.json"

func read_vec(values: Array) -> Vector3:
    return Vector3(values[0], values[1], values[2])

func read_quat(values: Array, start: int = 0) -> Quaternion:
    return Quaternion(values[start], values[start + 1], values[start + 2], values[start + 3]).normalized()

func update_arms() -> void:
    if frames.is_empty():
        super.update_arms()
        return
    var dt: float = recording.fixed_dt
    var duration: float = float(frames.back().tick - frames[0].tick) * dt
    var desired_tick: float = frames[0].tick + fmod(playback_seconds, maxf(duration, dt)) / dt
    while last_frame + 1 < frames.size() - 1 and frames[last_frame + 1].tick <= desired_tick:
        last_frame += 1
    if desired_tick < frames[last_frame].tick:
        last_frame = 0
    var a: Dictionary = frames[last_frame]
    var b: Dictionary = frames[mini(last_frame + 1, frames.size() - 1)]
    var alpha := clampf((desired_tick - a.tick) / maxf(float(b.tick - a.tick), 1.0), 0.0, 1.0)
    var body_p := read_vec(a.body.p).lerp(read_vec(b.body.p), alpha)
    var body_q := read_quat(a.body.q).slerp(read_quat(b.body.q), alpha)
    animal.position = body_p * DRAW_SCALE
    animal.quaternion = body_q
    for arm_id in range(ARM_COUNT):
        var shown: bool = arm_id < a.arms.size()
        arms[arm_id].visible = shown
        for cup in cups[arm_id]:
            # Native trace currently has no rim geometry: hide decorative cups.
            cup.visible = false
        if not shown:
            continue
        var nodes_a: Array = a.arms[arm_id].nodes
        var nodes_b: Array = b.arms[arm_id].nodes
        var vertices := PackedVector3Array()
        var normals := PackedVector3Array()
        var colors := PackedColorArray()
        var indices := PackedInt32Array()
        for j in range(nodes_a.size()):
            var pos := read_vec(nodes_a[j]).lerp(read_vec(nodes_b[j]), alpha)
            pos = body_q.inverse() * (pos - body_p) * DRAW_SCALE + Vector3(0, 0.63, 0)
            var rotation := body_q.inverse() * read_quat(nodes_a[j], 3).slerp(read_quat(nodes_b[j], 3), alpha)
            var radius: float = lerpf(nodes_a[j][7], nodes_b[j][7], alpha) * DRAW_SCALE
            for k in range(SIDES + 1):
                var theta := TAU * float(k) / SIDES
                var normal := rotation * Vector3(cos(theta), sin(theta), 0)
                vertices.append(pos + normal * radius)
                normals.append(normal)
                colors.append(Color("ce7e65").lerp(Color("ecc8a7"), smoothstep(0.2, 0.8, -sin(theta))))
        for j in range(nodes_a.size() - 1):
            for k in range(SIDES):
                var v := j * (SIDES + 1) + k
                var w := v + SIDES + 1
                indices.append_array(PackedInt32Array([v, v + 1, w, v + 1, w + 1, w]))
        var arrays := []
        arrays.resize(Mesh.ARRAY_MAX)
        arrays[Mesh.ARRAY_VERTEX] = vertices
        arrays[Mesh.ARRAY_NORMAL] = normals
        arrays[Mesh.ARRAY_COLOR] = colors
        arrays[Mesh.ARRAY_INDEX] = indices
        var tube := ArrayMesh.new()
        tube.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
        arms[arm_id].mesh = tube
    for j in range(rigids.size()):
        var pos := read_vec(a.rigids[j].p).lerp(read_vec(b.rigids[j].p), alpha)
        # Native coordinate positions plus the same artistic display offset.
        rigids[j].position = pos * DRAW_SCALE + body_q * Vector3(0, 0.63, 0)
        rigids[j].quaternion = read_quat(a.rigids[j].q).slerp(read_quat(b.rigids[j].q), alpha)
    count_label.text = "%s | %d ARM(S) | tick %.0f | OFFLINE %s" % [recording.scene, a.arms.size(), desired_tick, "PAUSED" if paused else "PLAYBACK"]

func _process(delta: float) -> void:
    if frames.is_empty():
        return
    frame += 1
    if not paused:
        playback_seconds += delta
    if auto_orbit:
        yaw += delta * 0.22
    update_arms()
    update_camera()
    if lab_capture and frame == 90:
        playback_seconds = (frames.back().tick - frames[0].tick) * recording.fixed_dt * 0.90
        update_arms()
        await capture("native_physics_lab")
        print("PHYSICS_LAB_CAPTURE_OK")
        get_tree().quit(0)
