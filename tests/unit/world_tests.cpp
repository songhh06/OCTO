#include "octo/world.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>

static void require(bool ok) {
    if (!ok)
        throw std::runtime_error("contract failed");
}
template <class E, class F> static void rejects(F function) {
    try {
        function();
    } catch (const E &) {
        return;
    }
    throw std::runtime_error("expected rejection");
}
int main() {
    try {
        octo::World world(123);
        const auto start = world.snapshot();
        require(start.tick == 0 && start.seed == 123);
        world.step();
        require(world.snapshot().tick == 1);
        const auto before = world.snapshot();
        for (double dt :
             {0.0, -octo::fixed_dt, 1.0 / 60.0, std::numeric_limits<double>::quiet_NaN(),
              std::numeric_limits<double>::infinity()})
            rejects<std::invalid_argument>([&] { world.step(dt); });
        require(world.snapshot() == before);
        world.restore(start);
        for (int i = 0; i < 1200; ++i)
            world.step();
        const auto expected = world.snapshot();
        world.restore(start);
        for (int i = 0; i < 1200; ++i)
            world.step();
        require(world.snapshot() == expected);
        auto incompatible = expected;
        incompatible.version = 2;
        rejects<std::invalid_argument>([&] { world.restore(incompatible); });
        require(world.snapshot() == expected);
        auto exhausted = expected;
        exhausted.tick = std::numeric_limits<std::uint64_t>::max();
        world.restore(exhausted);
        rejects<std::overflow_error>([&] { world.step(); });
        require(world.snapshot() == exhausted);
        require(octo::World::gravity()[1] == -9.81);
        std::cout
            << "P0 world: tick, dt rejection, atomic restore, determinism, overflow, SI passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
