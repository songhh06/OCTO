#pragma once
#include <array>
#include <cstdint>

namespace octo {
// SI, right handed, +Y up. P0 is an empty world: no simulated bodies yet.
inline constexpr double fixed_dt = 1.0 / 120.0;
inline constexpr std::uint32_t snapshot_version = 1;
struct Snapshot {
    std::uint32_t version = snapshot_version;
    std::uint64_t tick = 0;
    std::uint64_t seed = 42;
    bool operator==(const Snapshot &) const = default;
};
class World {
  public:
    explicit World(std::uint64_t seed = 42) : state_{snapshot_version, 0, seed} {}
    void step(double dt = fixed_dt);
    [[nodiscard]] Snapshot snapshot() const noexcept { return state_; }
    void restore(const Snapshot &snapshot);
    [[nodiscard]] static constexpr std::array<double, 3> gravity() { return {0, -9.81, 0}; }

  private:
    Snapshot state_;
};
} // namespace octo
