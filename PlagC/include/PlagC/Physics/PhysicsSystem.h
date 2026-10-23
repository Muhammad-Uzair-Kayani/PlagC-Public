#pragma once
#include <PlagC/World/World.h>
#include <PlagC/Physics/PhysicsWorld.h>
#include <PlagC/Physics/FixedStep.h>
namespace PlagC {
struct PhysicsFrameStats {
    unsigned steps=0;
    std::size_t ecsWrites=0;
    double alpha=0,droppedSeconds=0,structureMs=0,inputMs=0,stepMs=0,outputMs=0,presentationMs=0;
    PhysicsStats physics;
};
// One bridge per World. Main-thread owned; Jolt workers finish before ECS writes.
// World may die first; further operations then throw instead of dereferencing it.
class PhysicsSystem {
public:
    explicit PhysicsSystem(World&,const PhysicsConfig& = {});
    ~PhysicsSystem();
    PhysicsSystem(const PhysicsSystem&)=delete;
    PhysicsSystem& operator=(const PhysicsSystem&)=delete;
    void Advance(double frameSeconds);
    void Teleport(Entity,const Transform&);
    void ApplyImpulse(Entity,Vec3);
    void ApplyForce(Entity,Vec3); // Applied to the next fixed step; call from fixed-rate gameplay.
    void SetVelocity(Entity,Vec3);
    std::optional<RayHit> Raycast(Vec3 origin,Vec3 displacement,CollisionLayer=CollisionLayer::Projectile);
    Entity EntityFor(PhysicsBodyHandle) const;
    PhysicsBodyState Read(Entity) const;
    // Renderer-only interpolated poses. Never commit these into World.
    std::span<const TransformUpdate> Presentation() const;
    PhysicsFrameStats Statistics() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
