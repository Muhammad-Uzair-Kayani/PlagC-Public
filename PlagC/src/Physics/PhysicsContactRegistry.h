#pragma once

#include <cstdint>
#include <vector>

namespace PlagC {

struct PhysicsContactPair {
    std::uint64_t a = 0;
    std::uint64_t b = 0;
    bool operator==(const PhysicsContactPair&) const = default;
};

// Process-local contact registry shared by Jolt callback adapters and PhysicsSystem.
// Body handles are globally unique, so independent PhysicsSystem instances can filter safely.
class PhysicsContactRegistry final {
public:
    static void Add(PhysicsContactPair) noexcept;
    static void Remove(PhysicsContactPair) noexcept;
    static std::vector<PhysicsContactPair> Snapshot();
};

} // namespace PlagC
