#include "octo/scene.hpp"
#include <iostream>
#include <map>

int main(int argc, char **argv) {
    try {
        std::map<std::string, std::string> args;
        for (int j = 1; j < argc; ++j) {
            std::string key = argv[j];
            if (key == "--validate-scene") {
                args[key] = "true";
                continue;
            }
            if (j + 1 >= argc)
                throw std::invalid_argument("expected --option value");
            if (key != "--scene" && key != "--inputs" && key != "--record" && key != "--replay" &&
                key != "--ticks" && key != "--checkpoint")
                throw std::invalid_argument("unknown replay CLI option");
            args[key] = argv[++j];
        }
        auto scene = octo::io::load_scene(octo::json::path_utf8(args.at("--scene")));
        if (args.contains("--validate-scene")) {
            std::cout << "scene valid SHA256=" << scene.scene_sha256 << '\n';
            return 0;
        }
        octo::json::Value document;
        if (args.contains("--record")) {
            auto inputs =
                octo::io::load_inputs(octo::json::load(octo::json::path_utf8(args.at("--inputs"))));
            auto count = octo::json::Parser(args.contains("--ticks") ? args.at("--ticks") : "1200")
                             .parse()
                             .u64();
            if (count > 72000)
                throw std::invalid_argument("tick limit");
            document = octo::io::record(scene, inputs, static_cast<unsigned>(count));
            octo::json::save(octo::json::path_utf8(args.at("--record")), document);
        } else
            document = octo::json::load(octo::json::path_utf8(args.at("--replay")));
        auto final = octo::io::replay(scene, document);
        if (args.contains("--checkpoint")) {
            std::ofstream output(octo::json::path_utf8(args.at("--checkpoint")));
            if (!output)
                throw std::runtime_error("checkpoint output failed");
            final.save(output);
        }
        std::cout << "verified tick=" << final.state.tick << " state_fnv1a64=" << final.state_hash()
                  << " build=" << octo::io::build_id() << '\n';
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 2;
    }
}
