#pragma once
#include <PlagC/Renderer/Renderer.h>
#include <array>
namespace PlagC {
enum class MotionType { Static, Dynamic, Kinematic };
enum class ColliderType { Box, Sphere, Capsule, Cylinder };
enum class CollisionLayer : std::uint8_t { StaticWorld, Dynamic, Character, Vehicle, Projectile, Sensor, Debris, Ragdoll, Count };
struct PhysicsMaterial { float friction=.5f, restitution=0; };
struct RigidBodyComponent {
    MotionType motion=MotionType::Dynamic;
    CollisionLayer layer=CollisionLayer::Dynamic;
    float mass=1, gravityScale=1, linearDamping=.05f, angularDamping=.05f;
    bool allowSleep=true, continuous=false;
};
struct ColliderComponent {
    ColliderType type=ColliderType::Box;
    // Box: half extents. Sphere: x=radius. Capsule/cylinder: x=radius, y=half-height
    // (capsule height excludes the two hemispheres). Transform scale is baked at creation.
    Vec3 dimensions{.5f,.5f,.5f};
    PhysicsMaterial material;
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
struct PhysicsConfig {
    unsigned maxBodies=16384,maxBodyPairs=65536,maxContacts=16384,workers=2,maxCatchUpSteps=4;
    double fixedHz=60;
    Vec3 gravity{0,-9.81f,0};
    CollisionMatrix collisions;
};
struct PhysicsBodyState { Transform transform;Vec3 velocity;MotionType motion;bool active=false; };
struct PhysicsPoseUpdate { PhysicsBodyHandle body;Transform transform;bool active=false; };
struct RayHit { PhysicsBodyHandle body;Vec3 position,normal;float fraction=0; };
struct PhysicsStats {
    unsigned bodies=0,statics=0,dynamics=0,kinematics=0,active=0,sleeping=0,workers=0;
    std::size_t publishedPoses=0,raycasts=0;
    double updateMs=0,queriesMs=0;
};
}
