extends "res://scripts/physics_lab.gd"
## Interactive inspection of recorded native states; no live solver claim.
const CASES := ["bend", "extend", "light", "heavy", "eight"]
const TITLES := ["Arm bending", "Arm extension", "Pull a light box (0.5 kg)", "Pull a heavy box (8 kg)", "Eight arms / 128 cups"]
const NOTES := ["One longitudinal muscle channel activated.\nWatch the arm curve away from its start.", "Transverse muscle channel activated.\nWatch length and radius change together.", "Four longitudinal channels contract.\nBoth the torso and the box can move.", "Same command, heavier box.\nCompare the numerical displacements.", "Eight native rods with geometric cups.\nSmall motion is expected in this fixture.\nCup rims are not rendered in this viewer."]
var scrub: HSlider
var details: Label
var speed := 1.0
var checking := false
var case_id := 0
var test_busy := false

func get_trace_path() -> String:
    var selected: int = int(get_tree().get_meta("demo_case", 0))
    return "res://data/demo/%s.json" % CASES[selected]

func _ready() -> void:
    super._ready()
    if frames.is_empty():
        return
    case_id = int(get_tree().get_meta("demo_case", 0))
    checking = "--demo-test" in OS.get_cmdline_user_args()
    DisplayServer.window_set_title("OCTOPUS | P2 native results demo | RECORDED PLAYBACK")
    distance = 7.8
    var layer := CanvasLayer.new()
    add_child(layer)
    var panel := PanelContainer.new()
    panel.position = Vector2(920, 25)
    panel.custom_minimum_size = Vector2(335, 0)
    layer.add_child(panel)
    var column := VBoxContainer.new()
    column.add_theme_constant_override("separation", 9)
    panel.add_child(column)
    column.add_child(ui_label("P2  /  NATIVE RESULTS", 20, Color("b3e4e7")))
    column.add_child(ui_label("RECORDED PLAYBACK\nNot live muscle control. BIO unvalidated.", 13, Color("edb494")))
    for i in range(CASES.size()):
        var button := Button.new()
        button.text = ("[Viewing] " if i == case_id else "") + TITLES[i]
        button.focus_mode = Control.FOCUS_NONE
        button.pressed.connect(select_case.bind(i))
        column.add_child(button)
    column.add_child(ui_label(NOTES[case_id], 13, Color("d5e3e5")))
    var row := HBoxContainer.new()
    column.add_child(row)
    for title in ["Pause / Play", "Restart", "0.25x / 1x"]:
        var button := Button.new()
        button.text = title
        button.focus_mode = Control.FOCUS_NONE
        row.add_child(button)
        if title == "Pause / Play":
            button.pressed.connect(func(): paused = not paused)
        elif title == "Restart":
            button.pressed.connect(func(): playback_seconds = 0.0; last_frame = 0)
        else:
            button.pressed.connect(func(): speed = 0.25 if speed == 1.0 else 1.0)
    scrub = HSlider.new()
    scrub.min_value = 0
    scrub.max_value = 100
    scrub.step = 0.1
    scrub.value_changed.connect(seek)
    column.add_child(scrub)
    details = ui_label("", 13, Color("b3e4e7"))
    column.add_child(details)
    column.add_child(ui_label("Drag: orbit   /   Wheel: zoom\nSpace: pause   /   P: screenshot\n1/2/3: camera view", 13, Color("adc4ce")))
    motion_label.text = "Recorded C++ states | No pose animation drives the solver | BIO unvalidated"
    for label in find_children("*", "Button", true, false):
        if label.text == "3  Suckers":
            label.text = "3  Underside"
    if case_id != 4:
        var marker := SphereMesh.new()
        marker.radius = 0.045
        marker.height = 0.09
        var dot := mesh_node(marker, material(Color("ffd65f"), 0.3))
        dot.position = read_vec(frames[0].arms[0].nodes.back()) * DRAW_SCALE + read_quat(frames[0].body.q) * Vector3(0, 0.63, 0)
        column.add_child(ui_label("Gold dot: initial arm tip.\nBody shell is illustrative.", 13, Color("e6c374")))
    print("P2_DEMO_READY case=%s arms=%d frames=%d" % [CASES[case_id], frames[0].arms.size(), frames.size()])

func update_camera() -> void:
    super.update_camera()
    camera.position += camera.global_basis.x * 1.45

func select_case(index: int) -> void:
    get_tree().set_meta("demo_case", index)
    get_tree().reload_current_scene()

func seek(percent: float) -> void:
    var duration: float = (frames.back().tick - frames[0].tick) * recording.fixed_dt
    playback_seconds = minf(percent * duration / 100.0, maxf(duration - recording.fixed_dt, 0.0))
    last_frame = 0
    update_arms()

func _process(delta: float) -> void:
    super._process(delta * speed)
    if scrub == null or frames.is_empty():
        return
    var duration: float = (frames.back().tick - frames[0].tick) * recording.fixed_dt
    var percent := fmod(playback_seconds, duration) / duration
    scrub.set_value_no_signal(percent * 100)
    var sample: Dictionary = frames[last_frame]
    var body_delta := read_vec(sample.body.p) - read_vec(frames[0].body.p)
    var box_delta := Vector3.ZERO
    if not sample.rigids.is_empty():
        box_delta = read_vec(sample.rigids[0].p) - read_vec(frames[0].rigids[0].p)
    var first_nodes: Array = frames[0].arms[0].nodes
    var nodes: Array = sample.arms[0].nodes
    var tip_delta := read_vec(nodes.back()) - read_vec(first_nodes.back())
    details.text = "%s   %.2f / %.2f s   %.2fx\nTorso displacement: %.3f mm\nBox displacement: %.3f mm\nArm tip displacement: %.3f mm" % ["PAUSED" if paused else "PLAYING", percent * duration, duration, speed, body_delta.length() * 1000, box_delta.length() * 1000, tip_delta.length() * 1000]
    if checking and frame >= 90 and not test_busy:
        test_busy = true
        paused = true
        seek(90)
        assert(playback_seconds > 0 and last_frame > 0)
        await get_tree().process_frame
        await capture("p2_demo_%s" % CASES[case_id])
        print("P2_DEMO_CASE_OK %s seek_pause_camera=true" % CASES[case_id])
        if case_id < CASES.size() - 1:
            select_case(case_id + 1)
        else:
            print("P2_DEMO_ALL_OK cases=5")
            get_tree().quit(0)
