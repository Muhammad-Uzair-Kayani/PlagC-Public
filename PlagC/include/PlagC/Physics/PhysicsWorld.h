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
    void DestroyBatch(std::span<const PhysicsBodyHandle>); // Validate before removal.
    void SetAngularVelocity(PhysicsBodyHandle,Vec3);
    std::optional<RayHit> ShapeCast(const ColliderComponent&,const Transform&,Vec3 displacement,CollisionLayer=CollisionLayer::Projectile);
    std::vector<PhysicsBodyHandle> Overlap(const ColliderComponent&,const Transform&,CollisionLayer=CollisionLayer::Projectile);
    ConstraintHandle CreateConstraint(const ConstraintDesc&);
    void DestroyConstraint(ConstraintHandle);
    CharacterHandle CreateCharacter(const CharacterDesc&);
    void DestroyCharacter(CharacterHandle);
    void SetCharacterInput(CharacterHandle,Vec3 horizontalVelocity,bool jump=false);
    CharacterState ReadCharacter(CharacterHandle) const;
    std::span<const TriggerEvent> TriggerEvents() const; // Main-thread delivery after Step.

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
    MeshData BuildColliderMesh(const ColliderComponent&) const; // Diagnostic geometry, generated at load time.
    PhysicsStats Statistics() const;
    double StepSeconds() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
