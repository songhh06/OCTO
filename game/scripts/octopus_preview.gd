extends Node3D
## Visual prototype only. Procedural animation, NOT the C++ rod/adhesion solver.
const ARM_COUNT := 8
const CUPS_PER_ARM := 16
const SEGMENTS := 40
const SIDES := 14
var arms: Array[MeshInstance3D] = []
var cups: Array[Array] = []
var camera: Camera3D
var animal: Node3D
var time := 0.0
var yaw := 0.32
var pitch := 0.57
var distance := 7.3
var dragging := false
var paused := false
var auto_orbit := false
var frame := 0
var capture_mode := false
var test_mode := false
var skin: StandardMaterial3D
var count_label: Label
var motion_label: Label

func material(color: Color, roughness: float = 0.45, metallic: float = 0.0) -> StandardMaterial3D:
    var mat := StandardMaterial3D.new()
    mat.albedo_color = color
    mat.roughness = roughness
    mat.metallic = metallic
    return mat

func mesh_node(mesh: Mesh, mat: Material, parent: Node = self) -> MeshInstance3D:
    var node := MeshInstance3D.new()
    node.mesh = mesh
    node.material_override = mat
    parent.add_child(node)
    return node

func ellipsoid(pos: Vector3, scale_xyz: Vector3, mat: Material, parent: Node = animal) -> MeshInstance3D:
    var sphere := SphereMesh.new()
    sphere.radius = 1.0
    sphere.height = 2.0
    sphere.radial_segments = 40
    sphere.rings = 24
    var node := mesh_node(sphere, mat, parent)
    node.position = pos
    node.scale = scale_xyz
    return node

func _ready() -> void:
    DisplayServer.window_set_title("OCTOPUS | Visual prototype | Godot 4.6.2")
    test_mode = "--preview-test" in OS.get_cmdline_user_args()
    capture_mode = test_mode or "--preview-capture" in OS.get_cmdline_user_args()
    animal = Node3D.new()
    animal.name = "VisualOctopus_Exactly8Arms"
    add_child(animal)
    build_stage()
    build_animal()
    build_ui()
    camera = Camera3D.new()
    camera.fov = 43.0
    camera.near = 0.05
    add_child(camera)
    camera.make_current()
    update_camera()
    print("PREVIEW_READY arms=%d suckers=%d visual_only=true" % [arms.size(), ARM_COUNT * CUPS_PER_ARM])
    assert(arms.size() == 8)
    for row in cups:
        assert(row.size() == 16)

func build_stage() -> void:
    var environment := Environment.new()
    environment.background_mode = Environment.BG_COLOR
    environment.background_color = Color("091a29")
    environment.ambient_light_source = Environment.AMBIENT_SOURCE_COLOR
    environment.ambient_light_color = Color("a9cbd9")
    environment.ambient_light_energy = 0.32
    environment.tonemap_mode = Environment.TONE_MAPPER_FILMIC
    var world := WorldEnvironment.new()
    world.environment = environment
    add_child(world)
    var key := DirectionalLight3D.new()
    key.rotation_degrees = Vector3(-38, -35, 0)
    key.light_color = Color("ffdabf")
    key.light_energy = 0.85
    key.shadow_enabled = true
    add_child(key)
    var fill := OmniLight3D.new()
    fill.position = Vector3(-4, 3, 2)
    fill.light_color = Color("73cbe3")
    fill.light_energy = 0.55
    fill.omni_range = 12
    add_child(fill)
    var rim := OmniLight3D.new()
    rim.position = Vector3(2, 4, -3)
    rim.light_color = Color("ffa176")
    rim.light_energy = 0.9
    rim.omni_range = 10
    add_child(rim)
    var floor_mesh := PlaneMesh.new()
    floor_mesh.size = Vector2(200, 200)
    mesh_node(floor_mesh, material(Color("102937"))).position.y = -0.77
    var disk := CylinderMesh.new()
    disk.top_radius = 3.22
    disk.bottom_radius = 3.22
    disk.height = 0.14
    disk.radial_segments = 128
    mesh_node(disk, material(Color("173845"), 0.65)).position.y = -0.66
    var ring_mat := material(Color("397781"), 0.4, 0.3)
    for radius in [1.0, 2.0, 3.12]:
        var ring := TorusMesh.new()
        ring.inner_radius = radius - 0.008
        ring.outer_radius = radius + 0.008
        ring.rings = 96
        ring.ring_segments = 6
        mesh_node(ring, ring_mat).position.y = -0.581

func build_animal() -> void:
    skin = material(Color.WHITE, 0.37)
    skin.vertex_color_use_as_albedo = true
    var mantle_mat := material(Color("c76852"), 0.38)
    var head_mat := material(Color("da8064"), 0.4)
    ellipsoid(Vector3(0, 1.32, -0.15), Vector3(0.62, 0.87, 0.62), mantle_mat)
    ellipsoid(Vector3(0, 0.73, 0.14), Vector3(0.65, 0.40, 0.58), head_mat)
    # Small procedural chromatophore marks, no external textures.
    var spot_mat := material(Color("9f4b42"), 0.6)
    for i in range(46):
        var angle := float(i) * 2.39996
        var v := -0.8 + 1.65 * float(i) / 46.0
        var radius := sqrt(1.0 - v * v)
        var point := Vector3(0.625 * radius * cos(angle), 1.32 + 0.875 * v, -0.15 + 0.625 * radius * sin(angle))
        ellipsoid(point, Vector3.ONE * (0.018 + 0.007 * sin(i * 1.7)), spot_mat)
    var eye_mat := material(Color("e9bd69"), 0.24)
    var pupil_mat := material(Color("10202a"), 0.18)
    var white_mat := material(Color("fff4d8"), 0.1)
    for sign_x in [-1.0, 1.0]:
        ellipsoid(Vector3(sign_x * 0.49, 0.94, 0.44), Vector3(0.19, 0.19, 0.16), head_mat)
        ellipsoid(Vector3(sign_x * 0.53, 0.95, 0.54), Vector3(0.135, 0.125, 0.075), eye_mat)
        ellipsoid(Vector3(sign_x * 0.53, 0.95, 0.605), Vector3(0.105, 0.035, 0.025), pupil_mat)
        ellipsoid(Vector3(sign_x * 0.50, 0.99, 0.618), Vector3.ONE * 0.021, white_mat)
    var cup_mat := material(Color("f5c8ab"), 0.46)
    var cup_inner := material(Color("b87165"), 0.6)
    var ring := TorusMesh.new()
    ring.inner_radius = 0.50
    ring.outer_radius = 1.0
    ring.rings = 16
    ring.ring_segments = 8
    var hollow := CylinderMesh.new()
    hollow.top_radius = 0.50
    hollow.bottom_radius = 0.42
    hollow.height = 0.12
    hollow.radial_segments = 16
    for arm_id in range(ARM_COUNT):
        var arm := MeshInstance3D.new()
        arm.name = "Arm_%d" % (arm_id + 1)
        arm.material_override = skin
        animal.add_child(arm)
        arms.append(arm)
        var row: Array = []
        for cup_id in range(CUPS_PER_ARM):
            var cup := Node3D.new()
            cup.name = "Sucker_%d_%02d" % [arm_id + 1, cup_id + 1]
            animal.add_child(cup)
            mesh_node(ring, cup_mat, cup)
            var inner := mesh_node(hollow, cup_inner, cup)
            inner.position.y = -0.13
            row.append(cup)
        cups.append(row)
    update_arms()

func centerline(arm: int, u: float) -> Vector3:
    var angle := TAU * float(arm) / ARM_COUNT + PI / 8.0
    var radial := Vector3(sin(angle), 0, cos(angle))
    var lateral := Vector3(cos(angle), 0, -sin(angle))
    var phase := time * 1.2 + float(arm) * 0.83
    var tip := maxf((u - 0.55) / 0.45, 0.0)
    var reach := 0.44 + (2.17 + 0.14 * sin(float(arm) * 2.1)) * u - 0.48 * tip * tip
    var sway := 0.20 * sin(phase - u * 4.5) * u * u
    var height := 0.63 - 0.49 * sin(u * PI * 0.83) + (0.6 + 0.30 * sin(phase)) * tip * tip
    return radial * reach + lateral * sway + Vector3.UP * height

func arm_frame(arm: int, u: float) -> Basis:
    var tangent := (centerline(arm, u + 0.001) - centerline(arm, u - 0.001)).normalized()
    var side := tangent.cross(Vector3.UP).normalized()
    var up := side.cross(tangent).normalized()
    return Basis(side, up, -tangent)

func arm_radius(u: float) -> float:
    return 0.205 * pow(1.0 - u, 1.05) + 0.012

func update_arms() -> void:
    for arm_id in range(ARM_COUNT):
        var vertices := PackedVector3Array()
        var normals := PackedVector3Array()
        var colors := PackedColorArray()
        var indices := PackedInt32Array()
        for j in range(SEGMENTS + 1):
            var u := float(j) / SEGMENTS
            var center := centerline(arm_id, u)
            var basis := arm_frame(arm_id, u)
            for k in range(SIDES + 1):
                var theta := TAU * float(k) / SIDES
                var normal := basis.y * cos(theta) + basis.x * sin(theta)
                vertices.append(center + normal * arm_radius(u))
                normals.append(normal)
                var belly := smoothstep(-0.2, 0.65, -cos(theta))
                var tint := Color("ca6b55").lerp(Color("efb595"), belly)
                tint = tint.darkened(0.035 * (1.0 + sin(u * 96.0 + arm_id)))
                colors.append(tint)
        for j in range(SEGMENTS):
            for k in range(SIDES):
                var a := j * (SIDES + 1) + k
                var b := a + SIDES + 1
                indices.append_array(PackedInt32Array([a, b, a + 1, a + 1, b, b + 1]))
        var arrays := []
        arrays.resize(Mesh.ARRAY_MAX)
        arrays[Mesh.ARRAY_VERTEX] = vertices
        arrays[Mesh.ARRAY_NORMAL] = normals
        arrays[Mesh.ARRAY_COLOR] = colors
        arrays[Mesh.ARRAY_INDEX] = indices
        var tube := ArrayMesh.new()
        tube.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
        arms[arm_id].mesh = tube
        for cup_id in range(CUPS_PER_ARM):
            var u := 0.13 + float(cup_id / 2) * 0.105
            var basis := arm_frame(arm_id, u)
            var radius := arm_radius(u)
            var side_sign := -1.0 if cup_id % 2 == 0 else 1.0
            var normal := (-basis.y + basis.x * side_sign * 0.5).normalized()
            var cup: Node3D = cups[arm_id][cup_id]
            var x := basis.x
            var z := x.cross(normal).normalized()
            x = normal.cross(z).normalized()
            var size := radius * 0.48
            cup.transform = Transform3D(Basis(x, normal, z).scaled(Vector3.ONE * size), centerline(arm_id, u) + normal * radius)

func ui_label(text: String, font_size: int, color: Color) -> Label:
    var label := Label.new()
    label.text = text
    label.add_theme_font_size_override("font_size", font_size)
    label.add_theme_color_override("font_color", color)
    return label

func build_ui() -> void:
    var canvas := CanvasLayer.new()
    add_child(canvas)
    var margin := MarginContainer.new()
    margin.set_anchors_and_offsets_preset(Control.PRESET_FULL_RECT)
    for edge in ["left", "right", "top", "bottom"]:
        margin.add_theme_constant_override("margin_" + edge, 25)
    margin.mouse_filter = Control.MOUSE_FILTER_IGNORE
    canvas.add_child(margin)
    var box := VBoxContainer.new()
    box.mouse_filter = Control.MOUSE_FILTER_IGNORE
    margin.add_child(box)
    box.add_child(ui_label("FIELD NOTES     /     001", 13, Color("72c0c9")))
    box.add_child(ui_label("O C T O P U S", 30, Color("fff0db")))
    box.add_child(ui_label("VISUAL PROTOTYPE  /  NOT A PHYSICS SIMULATION", 12, Color("edb494")))
    var spacer := Control.new()
    spacer.size_flags_vertical = Control.SIZE_EXPAND_FILL
    spacer.mouse_filter = Control.MOUSE_FILTER_IGNORE
    box.add_child(spacer)
    var controls := HBoxContainer.new()
    controls.add_theme_constant_override("separation", 10)
    box.add_child(controls)
    var views := ["1  Perspective", "2  Eight-arm view", "3  Suckers"]
    for i in range(3):
        var button := Button.new()
        button.text = views[i]
        button.focus_mode = Control.FOCUS_NONE
        button.custom_minimum_size = Vector2(150, 36)
        button.pressed.connect(set_view.bind(i))
        controls.add_child(button)
    count_label = ui_label("08 ARMS     /     128 SUCKERS     /     PROCEDURAL MOTION", 13, Color("9fd3d5"))
    box.add_child(count_label)
    motion_label = ui_label("Drag: orbit   |   Wheel: zoom   |   Space: pause   |   A: auto orbit   |   R: reset   |   P: screenshot", 13, Color("adc4ce"))
    box.add_child(motion_label)

func set_view(view: int) -> void:
    auto_orbit = false
    distance = 7.3
    yaw = 0.32
    if view == 0:
        pitch = 0.57
    elif view == 1:
        pitch = 1.42
        yaw = 0.0
    else:
        pitch = -0.16
        distance = 6.1
        yaw = 0.2
    update_camera()

func update_camera() -> void:
    var target := Vector3(0, 0.72, 0)
    camera.position = target + Vector3(sin(yaw) * cos(pitch), sin(pitch), cos(yaw) * cos(pitch)) * distance
    camera.look_at(target, Vector3.UP)

func _unhandled_input(event: InputEvent) -> void:
    if event is InputEventMouseButton:
        if event.button_index in [MOUSE_BUTTON_LEFT, MOUSE_BUTTON_RIGHT]:
            dragging = event.pressed
        if event.pressed and event.button_index == MOUSE_BUTTON_WHEEL_UP:
            distance = maxf(3.5, distance - 0.35)
        if event.pressed and event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
            distance = minf(12.0, distance + 0.35)
    if event is InputEventMouseMotion and dragging:
        yaw -= event.relative.x * 0.007
        pitch = clampf(pitch + event.relative.y * 0.005, -0.16, 1.45)
        auto_orbit = false
    if event is InputEventKey and event.pressed and not event.echo:
        match event.keycode:
            KEY_1: set_view(0)
            KEY_2: set_view(1)
            KEY_3: set_view(2)
            KEY_SPACE: paused = not paused
            KEY_A: auto_orbit = not auto_orbit
            KEY_R: set_view(0)
            KEY_P: capture("octopus_manual")
    update_camera()

func capture(filename: String) -> void:
    await RenderingServer.frame_post_draw
    var folder := ProjectSettings.globalize_path("res://../docs/preview")
    DirAccess.make_dir_recursive_absolute(folder)
    var path := folder.path_join(filename + ".png")
    var error := get_viewport().get_texture().get_image().save_png(path)
    print("PREVIEW_CAPTURE path=%s result=%d" % [path, error])

func _process(delta: float) -> void:
    frame += 1
    if not paused:
        time += minf(delta, 0.05)
        update_arms()
        animal.position.y = 0.035 * sin(time * 1.1)
    if auto_orbit:
        yaw += delta * 0.22
    update_camera()
    if is_instance_valid(count_label):
        count_label.text = "08 ARMS     /     128 SUCKERS     /     " + ("PAUSED" if paused else "PROCEDURAL MOTION")
    if capture_mode:
        if frame == 70:
            capture("octopus_perspective")
        elif frame == 80:
            set_view(1)
        elif frame == 110:
            capture("octopus_eight_arms")
        elif frame == 120:
            set_view(2)
        elif frame == 150:
            capture("octopus_suckers")
        elif frame == 160:
            set_view(0)
            capture_mode = false
            print("PREVIEW_CAPTURE_COMPLETE arms=8 suckers=128 animation_time=%f" % time)
            if test_mode:
                check_interaction_paths()
                get_tree().quit(0)

func check_interaction_paths() -> void:
    var initial_yaw := yaw
    var press := InputEventMouseButton.new()
    press.button_index = MOUSE_BUTTON_LEFT
    press.pressed = true
    _unhandled_input(press)
    var move := InputEventMouseMotion.new()
    move.relative = Vector2(80, 25)
    _unhandled_input(move)
    assert(not is_equal_approx(yaw, initial_yaw))
    press.pressed = false
    _unhandled_input(press)
    var old_distance := distance
    var wheel := InputEventMouseButton.new()
    wheel.button_index = MOUSE_BUTTON_WHEEL_UP
    wheel.pressed = true
    _unhandled_input(wheel)
    assert(distance < old_distance)
    var space := InputEventKey.new()
    space.keycode = KEY_SPACE
    space.pressed = true
    _unhandled_input(space)
    assert(paused)
    _unhandled_input(space)
    assert(not paused)
    var t0 := time
    var old_tip := centerline(0, 0.90)
    time += 1.0
    assert(centerline(0, 0.90).distance_to(old_tip) > 0.01)
    time = t0
    var cup_total := 0
    for row in cups:
        cup_total += row.size()
    var report := {"engine": Engine.get_version_info().string, "visual_only": true,
        "arms": arms.size(), "suckers": cup_total, "drag_orbit": true,
        "wheel_zoom": true, "pause_toggle": true, "animated_geometry": true,
        "os_mouse_injection_tested": false, "physics_solver_tested": false}
    var path := ProjectSettings.globalize_path("res://../docs/preview/validation.json")
    var file := FileAccess.open(path, FileAccess.WRITE)
    file.store_string(JSON.stringify(report, "  "))
    print("PREVIEW_INPUT_CHECK_OK ", JSON.stringify(report))
