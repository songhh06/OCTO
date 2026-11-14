#include "octo/world.hpp"
#include <charconv>
#include <iostream>
#include <stdexcept>
#include <string_view>

static std::uint64_t number(std::string_view value) {
    std::uint64_t result{};
    auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), result);
    if (error != std::errc{} || end != value.data() + value.size())
        throw std::invalid_argument("expected unsigned integer");
    return result;
}
int main(int argc, char **argv) {
    try {
        std::uint64_t ticks = 1, seed = 42;
        for (int i = 1; i < argc; ++i) {
            const std::string_view key{argv[i]};
            if ((key != "--ticks" && key != "--seed") || i + 1 >= argc)
                throw std::invalid_argument("usage: octo_cli [--ticks N] [--seed N]");
            auto value = number(argv[++i]);
            if (key == "--ticks")
                ticks = value;
            else
                seed = value;
        }
        if (ticks > 10'000'000)
            throw std::invalid_argument("P0 tick limit is 10000000");
        octo::World world(seed);
        for (std::uint64_t i = 0; i < ticks; ++i)
            world.step();
        const auto state = world.snapshot();
        std::cout << "{\"snapshot_version\":" << state.version << ",\"tick\":" << state.tick
                  << ",\"seed\":" << state.seed << ",\"dynamic_bodies\":0}\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 2;
    }
}
