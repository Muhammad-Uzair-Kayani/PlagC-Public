#pragma once
#include <PlagC/Physics/PhysicsTypes.h>
#include <memory>
#include <optional>
#include <span>
namespace PlagC {
// Main-thread API. Jolt's worker pool is internal; calls never expose JPH types.
class PhysicsWorld {
public:
    explicit PhysicsWorld(const PhysicsConfig& = {});
    ~PhysicsWorld();
    PhysicsWorld(const PhysicsWorld&)=delete;
    PhysicsWorld& operator=(const PhysicsWorld&)=delete;
    std::vector<PhysicsBodyHandle> CreateBatch(std::span<const PhysicsBodyDesc>);
    PhysicsBodyHandle CreateBody(const PhysicsBodyDesc&);
    void DestroyBody(PhysicsBodyHandle);
    PhysicsBodyState Read(PhysicsBodyHandle) const;
    void SetVelocity(PhysicsBodyHandle,Vec3);
    void ApplyImpulse(PhysicsBodyHandle,Vec3);
    void ApplyForce(PhysicsBodyHandle,Vec3);
    void Wake(PhysicsBodyHandle);
    void Teleport(PhysicsBodyHandle,const Transform&);
    void MoveKinematic(PhysicsBodyHandle,const Transform&);
    void Step(); // Exactly one configured fixed step, never a render-frame delta.
    std::span<const PhysicsPoseUpdate> ChangedBodies() const; // Valid until next mutation/step.
    Transform Interpolated(PhysicsBodyHandle,double alpha) const;
    std::optional<RayHit> Raycast(Vec3 origin,Vec3 displacement,CollisionLayer queryLayer=CollisionLayer::Projectile);
    PhysicsStats Statistics() const;
    double StepSeconds() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
