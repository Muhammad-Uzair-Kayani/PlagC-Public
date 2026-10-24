#pragma once
#include <PlagC/Physics/PhysicsSystem.h>
#include <functional>
#include <string_view>
namespace PhysicsLabScenes {
inline constexpr unsigned Count=14;
inline constexpr std::string_view Names[Count]={"Shape Gallery","Material Ramps","CCD Range","Pyramid","Island Comparison","Sleeping City","Static World","Queries","Trigger Zones","Character Course","Constraints","District Streaming","Physics LOD","Ragdolls"};
struct Options {unsigned scene=0,bodies=1000,queries=1000,triangles=100000,seed=42;bool alternate=false,individual=false;};
struct Visual {PlagC::Entity entity;PlagC::ColliderComponent collider;unsigned colour;bool dynamic;};
struct QueryMark {PlagC::Vec3 start,end;bool hit=false;};
class Scene {
public:
    Scene(const Options&,const PlagC::PhysicsConfig&);
    PlagC::World world;
    PlagC::PhysicsSystem physics;
    std::vector<Visual> visuals;
    std::vector<QueryMark> queryMarks;
    std::vector<PlagC::Entity> dynamics;
    double queryMs=0,streamMs=0,lodMs=0;
    std::size_t enters=0,stays=0,exits=0;
    void Advance(double dt);
    void Query(unsigned count);
    void Fire(PlagC::Vec3 position,PlagC::Vec3 direction);
    void SetCharacterMovement(PlagC::Vec3 velocity,bool jump);
    void SetFocus(PlagC::Vec3 position){focus_=position;}
    void Stream();
    const Options& Settings()const{return options_;}
private:
    Options options_;
    double time_=0,streamClock_=0;
    PlagC::Vec3 focus_{},characterMovement_{};
    bool jump_=false;
    std::vector<std::pair<PlagC::CharacterHandle,PlagC::Entity>> characters_;
    struct Mover {PlagC::Entity entity;PlagC::Transform initial;};std::vector<Mover> movers_;
    struct LodBody {PlagC::Entity entity;PlagC::PhysicsComponent full;int level=0;PlagC::Vec3 velocity{};std::size_t visual=0;PlagC::Vec3 angularVelocity{};};std::vector<LodBody> lod_;
    std::vector<PlagC::Entity> district_;
    std::vector<std::pair<PlagC::Entity,PlagC::Vec3>> velocities_;
    struct Joint {PlagC::Entity a,b;PlagC::ConstraintDesc desc;};std::vector<Joint> joints_;
    PlagC::Entity Add(const PlagC::PhysicsComponent&,const PlagC::Transform&,unsigned colour=0);
    void Floor();void Build();void Fixed(double dt);void UpdateLOD();
};
}
