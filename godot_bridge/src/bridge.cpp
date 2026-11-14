// Pending build: requires a provenance-verified godot-cpp checkout.
#include "octo/world.hpp"
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/dictionary.hpp>

namespace godot {
class OctopusWorld3D : public Node3D {
    GDCLASS(OctopusWorld3D, Node3D)
    octo::World world_;

  protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("advance_fixed_tick"), &OctopusWorld3D::advance_fixed_tick);
        ClassDB::bind_method(D_METHOD("get_render_snapshot"), &OctopusWorld3D::get_render_snapshot);
    }

  public:
    void advance_fixed_tick() { world_.step(); }
    Dictionary get_render_snapshot() const {
        const auto state = world_.snapshot();
        Dictionary result;
        result["tick"] = static_cast<int64_t>(state.tick);
        result["seed"] = static_cast<int64_t>(state.seed);
        result["snapshot_version"] = state.version;
        result["dynamic_bodies"] = 0;
        return result;
    }
};
void initialize(ModuleInitializationLevel level) {
    if (level == MODULE_INITIALIZATION_LEVEL_SCENE)
        ClassDB::register_class<OctopusWorld3D>();
}
void terminate(ModuleInitializationLevel) {}
} // namespace godot
extern "C" {
GDExtensionBool GDE_EXPORT octo_library_init(GDExtensionInterfaceGetProcAddress get_proc_address,
                                             GDExtensionClassLibraryPtr library,
                                             GDExtensionInitialization *initialization) {
    godot::GDExtensionBinding::InitObject init(get_proc_address, library, initialization);
    init.register_initializer(godot::initialize);
    init.register_terminator(godot::terminate);
    init.set_minimum_library_initialization_level(godot::MODULE_INITIALIZATION_LEVEL_SCENE);
    return init.init();
}
}
