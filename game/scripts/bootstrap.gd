extends Node3D

func _ready() -> void:
    print("P0_SCENE_READY: empty 3D scene; native bridge not yet verified")
    if "--bridge-smoke" in OS.get_cmdline_user_args():
        if not ClassDB.class_exists("OctopusWorld3D"):
            push_error("P0_BRIDGE_MISSING: verified native extension is required")
            get_tree().quit(2)
            return
        var world = ClassDB.instantiate("OctopusWorld3D")
        add_child(world)
        var before: Dictionary = world.get_render_snapshot()
        world.advance_fixed_tick()
        var after: Dictionary = world.get_render_snapshot()
        if before["tick"] != 0 or after["tick"] != 1:
            push_error("P0_BRIDGE_TICK_FAILED")
            get_tree().quit(3)
            return
        print("P0_BRIDGE_ROUNDTRIP_OK: tick=1")
        get_tree().quit(0)
