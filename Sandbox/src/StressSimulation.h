#pragma once
#include <PlagC/Physics/PhysicsSystem.h>
#include <array>
#include <string_view>

namespace Stress {
inline constexpr std::array<std::string_view,6> DistrictNames={
    "PHYSICS YARD","COLLAPSE WORKS","INDUSTRIAL HALL","CONSTRAINT FACTORY","DYNAMIC CITY","TERRAIN RANGE"};
inline constexpr std::array<unsigned,9> Stages={1000,1500,2000,3000,4000,5000,6500,8000,10000};
struct Settings { unsigned entities=1000,seed=42; bool progressive=true,activity=true; };
struct Visual { PlagC::Entity entity;PlagC::ColliderComponent shape;unsigned colour=0;bool dynamic=false,sensor=false; };
struct Metrics {
    unsigned target=1000,requestedActive=1000,feasibleActive=0,stage=1;
    std::uint64_t recycled=0,spawned=0,disturbances=0,triggerEvents=0,zoneOverlaps=0;
    double nextIncrease=30,simulationSeconds=0,gameplayMs=0,queryMs=0;
    bool activityMet=false;
};
class Simulation {
public:
    Simulation(const Settings&,const PlagC::PhysicsConfig&);
    PlagC::World world;
    PlagC::PhysicsSystem physics;
    std::vector<Visual> visuals;
    PlagC::Entity camera;
    Metrics metrics;
    std::uint64_t visualRevision=0;
    void Advance(double dt);
    void SetInput(PlagC::Vec3 horizontal,bool jump,PlagC::Vec3 look);
    void SetTarget(unsigned entities);
    void SetProgressive(bool enabled){settings_.progressive=enabled;growthClock_=0;}
    bool Progressive()const{return settings_.progressive;}
    void ResetPlayer();
    void Grab();
    void Drop(){held_={};}
    void ThrowOrPush();
    void Fire();
    PlagC::Entity Held()const{return held_;}
    PlagC::CharacterState Player()const{return physics.ReadCharacter(player_);}
    PlagC::Vec3 Eye()const;
    unsigned District()const;
    unsigned Target()const{return metrics.target;}
    std::optional<PlagC::RayHit> Aim(float distance=4);
private:
    Settings settings_;
    PlagC::CharacterHandle player_;
    PlagC::Entity held_;
    float heldMass_=1;
    PlagC::Vec3 previousPlayer_{};
    PlagC::Vec3 input_{},look_{0,0,-1};bool jump_=false;
    struct Prop {PlagC::Entity entity;unsigned visual,region;float mass;};
    struct Mover {PlagC::Entity entity;PlagC::Transform base;unsigned kind,region;};
    struct Joint {PlagC::Entity a,b;PlagC::ConstraintDesc desc;};
    std::vector<Prop> props_;
    std::vector<Mover> movers_;
    std::vector<Joint> pendingJoints_;
    std::vector<PlagC::Entity> sensors_,newBobs_;
    unsigned serial_=0,recycleCursor_=0,disturbRegion_=0,infrastructure_=0;
    double physicsStep_=1./60;
    double growthClock_=0,spawnClock_=0,activityClock_=0,zoneClock_=0,lastShot_=-1;
    PlagC::Entity Add(const char*,const PlagC::PhysicsComponent&,const PlagC::Transform&,unsigned);
    PlagC::Entity Box(const char*,PlagC::Vec3 position,PlagC::Vec3 size,unsigned colour,
                      PlagC::MotionType motion=PlagC::MotionType::Static,PlagC::Vec3 rotation={});
    void BuildFacility();
    void AddInfrastructure(unsigned tier);
    void AddProp();
    void CommitJoints();
    void Fixed(double dt);
    void Recycle(unsigned count);
    Prop* Recyclable();
    void Disturb();
    void Hold();
    void UpdateCamera();
};
}
