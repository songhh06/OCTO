#pragma once
#include "octo/dynamics.hpp"
#include "octo/json.hpp"
#include <filesystem>

namespace octo::io {
struct Scene {
    dynamics::System world;
    std::string scene_sha256, profile_sha256;
};
Scene load_scene(const std::filesystem::path &path);
struct Intent {
    std::uint64_t tick = 0;
    int arm = 0;
    std::array<double, 7> activation{};
    bool suction = false;
};
std::vector<Intent> load_inputs(const json::Value &document);
void apply(dynamics::System &world, const Intent &intent);
std::string build_id();
json::Value record(Scene scene, const std::vector<Intent> &inputs, unsigned ticks);
dynamics::System replay(const Scene &scene, const json::Value &document);
} // namespace octo::io
