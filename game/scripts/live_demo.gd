extends "res://scripts/octopus_preview.gd"
## Live stdio RPC: all body/arm/box states are integrated in one native C++ world.
var child: Dictionary = {}
var pipe: FileAccess
var errors: FileAccess
var buffer := PackedByteArray()
var previous: Dictionary = {}
var current: Dictionary = {}
var latest: Dictionary = {}
var rigids: Array[MeshInstance3D] = []
var target := Vector3(1.45, 0, 0.8)
var goal_marker: MeshInstance3D
var grabbing := false
var in_flight := false
var stopped := false
var reset_pending := false
var received_at := 0
var response_ms := 16.0
var sent_at := 0
var info: Label
var live_test := false
var restart_test := false
var restart_killed := false
var captured := false
var input_checked := false
var max_motion := 0.0
var max_contacts := 0
var last_receive := 0
var test_start := 0
const DRAW_SCALE := 2.3

func _ready() -> void:
    Engine.max_fps = 60
    super._ready()
    capture_mode = false
    DisplayServer.window_set_title("OCTOPUS | LIVE native physics | Move the box")
    live_test = "--live-test" in OS.get_cmdline_user_args()
    restart_test = "--live-restart-test" in OS.get_cmdline_user_args()
    distance = 8.8
    yaw = 0.6
    pitch = 0.85
    for label in find_children("*", "Label", true, false):
        if label.text.begins_with("VISUAL PROTOTYPE"):
            label.text = "LIVE C++ WORLD / BOX CONTACT / BIO UNVALIDATED"
        elif label.text == "O C T O P U S":
            label.text = "L I V E   C O N T A C T"
    for button in find_children("*", "Button", true, false):
        if button.text == "3  Suckers": button.text = "3  Underside"
    motion_label.text = "WASD / arrows: move box | Q/E: lower/raise | Drag box: move | Right drag: orbit | Space: pause | R: reset"
    var canvas := CanvasLayer.new()
    add_child(canvas)
    var panel := PanelContainer.new()
    panel.position = Vector2(910, 25)
    panel.custom_minimum_size = Vector2(345, 0)
    canvas.add_child(panel)
    var column := VBoxContainer.new()
    column.add_theme_constant_override("separation", 12)
    panel.add_child(column)
    column.add_child(ui_label("LIVE INPUT -> NATIVE SOLVER", 18, Color("9fd3d5")))
    column.add_child(ui_label("Move the blue box into an arm.\nGold ghost cube = your requested target.\nBlue solid cube = actual physical box.", 13, Color("adc4ce")))
    info = ui_label("Starting native solver...", 14, Color("fff0db"))
    info.autowrap_mode = TextServer.AUTOWRAP_WORD_SMART
    info.custom_minimum_size.x = 320
    column.add_child(info)
    for title in ["Pause / Resume", "Reset world"]:
        var button := Button.new()
        button.text = title
        button.focus_mode = Control.FOCUS_NONE
        if title == "Reset world": button.pressed.connect(reset_world)
        else: button.pressed.connect(func(): paused = not paused)
        column.add_child(button)
    column.add_child(ui_label("8 physical arms / 12 elements each\nZero-gravity contact test with fluid drag.\nTorso shell is illustrative.\nRigid torso comparison model.\nVolumetric FEM is NOT complete.\nPhysics may run slower than wall time.\nNo replay file is used.", 13, Color("edb494")))
    var wire := BoxMesh.new()
    wire.size = Vector3.ONE * 0.32 * DRAW_SCALE
    var mat := material(Color(1,0.8,0.2,0.13))
    mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
    mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
    goal_marker = mesh_node(wire, mat)
    test_start = Time.get_ticks_msec()
    start_native()

func start_native() -> void:
    # A dead native process cannot receive a reset command. Replace its pipes
    # and discard any partial reply before accepting a new world snapshot.
    if pipe != null:
        pipe.close()
    if errors != null:
        errors.close()
    pipe = null
    errors = null
    child = {}
    buffer = PackedByteArray()
    previous = {}
    current = {}
    latest = {}
    in_flight = false
    sent_at = 0
    received_at = 0
    var executable := ProjectSettings.globalize_path("res://../build/release/octo_live.exe")
    child = OS.execute_with_pipe(executable, PackedStringArray(["--diagnostics", ProjectSettings.globalize_path("res://../work")]), false)
    if child.is_empty():
        fail("Cannot launch native solver. Run tools/build-live-demo.ps1.")
        return
    pipe = child.stdio
    errors = child.stderr
    stopped = false
    last_receive = Time.get_ticks_msec()
    print("LIVE_GODOT_PROCESS_STARTED pid=%s" % child.pid)

func read_vec(values: Array) -> Vector3:
    return Vector3(values[0], values[1], values[2])

func read_quat(values: Array, start: int = 0) -> Quaternion:
    return Quaternion(values[start], values[start+1], values[start+2], values[start+3]).normalized()

func send(command: Dictionary) -> void:
    if pipe == null or in_flight: return
    pipe.store_string(JSON.stringify(command) + "\n")
    pipe.flush()
    in_flight = true
    sent_at = Time.get_ticks_msec()

func reset_world() -> void:
    if in_flight:
        reset_pending = true
        return
    stopped = false
    last_receive = Time.get_ticks_msec()
    reset_pending = false
    target = Vector3(1.45,0,.8)
    max_motion = 0
    max_contacts = 0
    if child.is_empty() or not child.has("pid") or not OS.is_process_running(child.pid):
        # A terminated child cannot process reset; launch a fresh world.
        start_native()
    else:
        send({"op":"reset"})

func fail(message: String) -> void:
    stopped = true
    info.text = "STOPPED: " + message + "\nR / Reset world to restart. Check work/ for diagnostics."
    push_error(message)
    if live_test or restart_test: get_tree().quit(2)

func poll_native() -> void:
    if pipe == null or (stopped and not in_flight): return
    if child.has("pid") and not OS.is_process_running(child.pid):
        in_flight = false
        if restart_test and restart_killed:
            reset_world()
        else:
            fail("Native process exited; use R / Reset world to relaunch it.")
        return
    var available := pipe.get_length()
    if available > 0:
        buffer.append_array(pipe.get_buffer(available))
        if buffer.size() > 2000000:
            fail("Native message exceeds limit")
            return
        var newline := buffer.find(10)
        while newline >= 0:
            var message = JSON.parse_string(buffer.slice(0,newline).get_string_from_utf8())
            buffer = buffer.slice(newline+1)
            if not message is Dictionary or message.get("protocol",0) != 1 or not message.get("live",false) or not message.get("ok",false):
                in_flight = false
                fail("Native solve/protocol failed: " + str(message.get("error", "Invalid response") if message is Dictionary else message))
                return
            previous = current if not current.is_empty() else message.snapshot
            current = message.snapshot
            latest = message
            if int(current.tick) % 60 == 0: print("LIVE_TICK %d contacts=%d motion=%.6f" % [current.tick,message.contacts,message.arm_motion])
            response_ms = maxf(Time.get_ticks_msec()-sent_at,1) if sent_at > 0 else 16.0
            received_at = Time.get_ticks_msec()
            last_receive = received_at
            in_flight = false
            max_motion = maxf(max_motion, float(message.arm_motion))
            max_contacts = maxi(max_contacts,int(message.contacts))
            if rigids.is_empty():
                var box := BoxMesh.new()
                box.size = read_vec(current.rigids[0].half_extents)*DRAW_SCALE*2
                rigids.append(mesh_node(box,material(Color("409bd1"),.3)))
                print("LIVE_GODOT_READY arms=%d tick=%d" % [current.arms.size(),current.tick])
            newline = buffer.find(10)
    if Time.get_ticks_msec()-last_receive > 10000:
        fail("No response from native process for 10 seconds")

func update_arms() -> void:
    if current.is_empty():
        for arm in arms: arm.visible = false
        for list in cups:
            for cup in list: cup.visible = false
        return
    var a := previous
    var b := current
    var alpha := clampf(float(Time.get_ticks_msec()-received_at)/maxf(response_ms,8),0,1)
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

func update_camera() -> void:
    super.update_camera()
    camera.position += camera.global_basis.x * 1.5

func _unhandled_input(event: InputEvent) -> void:
    if event is InputEventKey and event.pressed and not event.echo:
        if event.keycode == KEY_R:
            reset_world()
            return
    if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT:
        if event.pressed and not current.is_empty():
            var origin := camera.project_ray_origin(event.position)
            var direction := camera.project_ray_normal(event.position)
            var box_pos := rigids[0].global_position
            var hit = AABB(box_pos-Vector3.ONE*.48,Vector3.ONE*.96).intersects_ray(origin,direction)
            grabbing = hit != null
        else: grabbing = false
        return
    if event is InputEventMouseMotion and grabbing:
        var origin := camera.project_ray_origin(event.position)
        var direction := camera.project_ray_normal(event.position)
        var hit = Plane(Vector3.UP,0.63).intersects_ray(origin,direction)
        if hit != null:
            target.x = hit.x / DRAW_SCALE
            target.z = hit.z / DRAW_SCALE
        return
    super._unhandled_input(event)

func check_controls() -> void:
    var key := InputEventKey.new()
    key.keycode = KEY_SPACE
    key.pressed = true
    _unhandled_input(key)
    assert(paused)
    _unhandled_input(key)
    assert(not paused)
    var click := InputEventMouseButton.new()
    click.button_index = MOUSE_BUTTON_LEFT
    click.pressed = true
    click.position = camera.unproject_position(rigids[0].position)
    _unhandled_input(click)
    assert(grabbing)
    var before := target
    var motion := InputEventMouseMotion.new()
    motion.position = camera.unproject_position(rigids[0].position + Vector3(.4,0,0))
    _unhandled_input(motion)
    assert(target.distance_to(before) > .05)
    click.pressed = false
    _unhandled_input(click)
    assert(not grabbing)
    target = Vector3(1.45,0,.8)
    input_checked = true
    print("LIVE_CONTROLS_OK drag_target_and_space_pause")

func _process(delta: float) -> void:
    poll_native()
    if reset_pending and not in_flight:
        reset_world()
        return
    if stopped or current.is_empty(): return
    if restart_test:
        if not restart_killed and int(current.tick) >= 2 and not in_flight:
            # Regression: kill the native process to exercise automatic relaunch.
            restart_killed = true
            OS.kill(child.pid)
            return
        if restart_killed:
            if int(current.tick) == 0 and not in_flight:
                print("LIVE_GODOT_RESTART_OK")
                get_tree().quit(0)
            return
    var x := float(Input.is_physical_key_pressed(KEY_D) or Input.is_physical_key_pressed(KEY_RIGHT))-float(Input.is_physical_key_pressed(KEY_A) or Input.is_physical_key_pressed(KEY_LEFT))
    var z := float(Input.is_physical_key_pressed(KEY_S) or Input.is_physical_key_pressed(KEY_DOWN))-float(Input.is_physical_key_pressed(KEY_W) or Input.is_physical_key_pressed(KEY_UP))
    var y := float(Input.is_physical_key_pressed(KEY_E))-float(Input.is_physical_key_pressed(KEY_Q))
    target += Vector3(x,y,z)*delta*.45
    target = target.clamp(Vector3(-1.6,-.15,-1.6),Vector3(1.6,.7,1.6))
    if live_test: target = Vector3(1.45-1.9*minf(float(current.tick)/300,1),0,.8)
    if not in_flight and not paused: send({"op":"step","target":[target.x,target.y,target.z]})
    update_arms()
    update_camera()
    if live_test and not input_checked and int(current.tick) >= 1:
        check_controls()
    goal_marker.position = target*DRAW_SCALE+Vector3(0,.63,0)
    info.text = "%s  tick %d / %.2f simulated s\nSolver %.1f ms / response %.1f ms\nContacts: %d (peak %d)\nArm displacement: %.1f mm\nPenetration: %.4f mm\nTarget: %.2f, %.2f, %.2f m" % ["PAUSED" if paused else "LIVE",current.tick,current.tick/120.0,latest.solver_ms,response_ms,latest.contacts,max_contacts,max_motion*1000,latest.penetration*1000,target.x,target.y,target.z]
    count_label.text = "LIVE C++ / 8 PHYSICAL ARMS / ASYNCHRONOUS INPUT / NO RECORDED TRACE"
    if live_test and current.tick >= 360 and not captured:
        captured = true
        paused = true
        assert(max_contacts>0 and max_motion>.005)
        await get_tree().process_frame
        await capture("live_box_contact")
        print("LIVE_GODOT_CONTACT_OK tick=%d contacts=%d motion_m=%.6f" % [current.tick,max_contacts,max_motion])
        reset_world()
    if live_test and captured and int(current.tick) == 0 and not in_flight:
        print("LIVE_GODOT_RESET_OK")
        get_tree().quit(0)

func _exit_tree() -> void:
    if pipe != null and child.has("pid") and OS.is_process_running(child.pid):
        pipe.store_string("{\"op\":\"quit\"}\n")
        pipe.flush()
        pipe.close()
    if child.has("pid") and OS.is_process_running(child.pid):
        OS.kill(child.pid)
