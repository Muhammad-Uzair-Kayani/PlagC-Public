#pragma once
#include <PlagC/World/World.h>
namespace PlagC {
// Bind resolves and validates selection once. Prepare returns owned contiguous data
// for jobs; Commit is all-or-nothing and rejects intervening writes/topology changes.
// No EnTT pointers/references escape. Join all jobs before Commit/Bind/destruction.
class TransformSystem {
public:
    TransformSystem();
    ~TransformSystem();
    TransformSystem(const TransformSystem&)=delete;
    TransformSystem& operator=(const TransformSystem&)=delete;
    void Bind(World&,std::span<const Entity>);
    bool IsBindingValid() const;
    std::span<Transform> Prepare();
    void Commit();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
