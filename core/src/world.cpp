#include "octo/world.hpp"
#include <cmath>
#include <limits>
#include <stdexcept>

namespace octo {
void World::step(double dt) {
    if (!std::isfinite(dt) || dt != fixed_dt)
        throw std::invalid_argument("dt must equal fixed 1/120 seconds");
    if (state_.tick == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("tick exhausted");
    ++state_.tick;
}
void World::restore(const Snapshot &snapshot) {
    if (snapshot.version != snapshot_version)
        throw std::invalid_argument("unsupported snapshot version");
    state_ = snapshot;
}
} // namespace octo
