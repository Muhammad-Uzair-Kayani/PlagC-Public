#pragma once
#include <PlagC/Renderer/Renderer.h>
#include <array>
#include <memory>
namespace PlagC {
class JobSystem;
enum class MotionType { Static, Dynamic, Kinematic };
enum class ColliderType { Box, Sphere, Capsule, Cylinder, ConvexHull, Compound, Mesh, Heightfield };
enum class CollisionLayer : std::uint8_t { StaticWorld, Dynamic, Character, Vehicle, Projectile, Sensor, Debris, Ragdoll, Count };
struct PhysicsMaterial { float friction=.5f, restitution=0; };
struct RigidBodyComponent {
    MotionType motion=MotionType::Dynamic;
    CollisionLayer layer=CollisionLayer::Dynamic;
    float mass=1, gravityScale=1, linearDamping=.05f, angularDamping=.05f;
    bool allowSleep=true, continuous=false, sensor=false;
    // Equal nonzero groups do not self-collide (e.g. linked ragdoll parts).
    std::uint32_t collisionGroup=0;
};
struct ColliderChild {
    ColliderType type=ColliderType::Box;
    Vec3 dimensions{.5f,.5f,.5f};
    Transform transform;
};
// Immutable shared geometry. Mesh indices are triangle triplets; heightfields are square grids.
struct ColliderGeometry {
    std::vector<Vec3> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<ColliderChild> children;
    std::vector<float> heights;
    unsigned samples=0;
    Vec3 offset{},spacing{1,1,1};
};
struct ColliderComponent {
    ColliderType type=ColliderType::Box;
    // Box: half extents. Sphere: x=radius. Capsule/cylinder: x=radius, y=half-height
    // (capsule height excludes the two hemispheres). Transform scale is baked at creation.
    Vec3 dimensions{.5f,.5f,.5f};
    PhysicsMaterial material;
    std::shared_ptr<const ColliderGeometry> geometry;
    Vec3 centerOfMassOffset{};
};
struct PhysicsComponent { RigidBodyComponent body; ColliderComponent collider; };
struct PhysicsBodyHandle { std::uint64_t value=0;explicit operator bool() const {return value!=0;}bool operator==(const PhysicsBodyHandle&)const=default; };
struct PhysicsBodyDesc { PhysicsComponent component; Transform transform; };
void ValidatePhysicsBody(const PhysicsBodyDesc&);
struct CollisionMatrix {
    static constexpr unsigned Count=static_cast<unsigned>(CollisionLayer::Count);
    std::array<std::array<bool,Count>,Count> pairs{};
    CollisionMatrix();
    void Set(CollisionLayer,CollisionLayer,bool);
    bool Get(CollisionLayer,CollisionLayer) const;
};
enum class PhysicsScheduler { Native, PlagC };
struct PhysicsConfig {
    unsigned maxBodies=16384,maxBodyPairs=65536,maxContacts=16384,workers=2,maxCatchUpSteps=4;
    double fixedHz=60;
    Vec3 gravity{0,-9.81f,0};
    CollisionMatrix collisions;
    PhysicsScheduler scheduler=PhysicsScheduler::Native;
    JobSystem* sharedJobs=nullptr; // Required for PlagC scheduler; must outlive the physics world.
};
struct PhysicsBodyState { Transform transform;Vec3 velocity;MotionType motion;bool active=false;Vec3 angularVelocity{}; };
struct PhysicsPoseUpdate { PhysicsBodyHandle body;Transform transform;bool active=false; };
struct RayHit { PhysicsBodyHandle body;Vec3 position,normal;float fraction=0; };
struct ConstraintHandle { std::uint64_t value=0;explicit operator bool()const{return value!=0;}bool operator==(const ConstraintHandle&)const=default; };
enum class ConstraintType { Distance, Point, Hinge, Fixed };
struct ConstraintDesc {
    ConstraintType type=ConstraintType::Point;
    PhysicsBodyHandle a,b; // Both valid; use a static anchor body for a world attachment.
    Vec3 anchorA{},anchorB{},axis{0,1,0}; // World space.
    float minDistance=0,maxDistance=1,minAngleDegrees=-90,maxAngleDegrees=90;
    bool motor=false;float motorSpeed=0,maxMotorTorque=100; // radians/sec, N*m
};
enum class TriggerPhase { Enter,Stay,Exit };
struct TriggerEvent {PhysicsBodyHandle sensor,other;TriggerPhase phase;};
struct CharacterHandle { std::uint64_t value=0;explicit operator bool()const{return value!=0;}bool operator==(const CharacterHandle&)const=default; };
struct CharacterDesc {Vec3 position{0,2,0};float radius=.35f,halfHeight=.55f,mass=70,maxSlopeDegrees=45,stepHeight=.4f;};
struct CharacterState {Vec3 position,velocity;bool grounded=false;};
struct PhysicsStats {
    unsigned bodies=0,statics=0,dynamics=0,kinematics=0,active=0,sleeping=0,workers=0;
    std::size_t publishedPoses=0,raycasts=0,shapeCasts=0,overlaps=0,contacts=0,constraints=0,sensors=0,characters=0,triggerEvents=0;
    double updateMs=0,queriesMs=0,eventsMs=0,charactersMs=0;
    std::uint64_t jobsSubmitted=0,jobsExecuted=0;
};
}
