#include "pgcpch.h"
#include "StressSimulation.h"
using namespace PlagC;
namespace Stress {
namespace {
using Clock=std::chrono::steady_clock;
double Ms(Clock::time_point start){return std::chrono::duration<double,std::milli>(Clock::now()-start).count();}
Vec3 Sum(Vec3 a,Vec3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
Vec3 Mul(Vec3 v,float s){return {v.x*s,v.y*s,v.z*s};}
float Length(Vec3 v){return std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);}
Vec3 Center(unsigned region){return {region%2?42.f:-42.f,0,30.f-60.f*float(region/2)};}
PhysicsConfig GameConfig(PhysicsConfig config){config.collisions.Set(CollisionLayer::Projectile,CollisionLayer::Sensor,false);return config;}
PhysicsComponent Body(MotionType motion){PhysicsComponent p;p.body.motion=motion;p.body.layer=motion==MotionType::Static?CollisionLayer::StaticWorld:CollisionLayer::Dynamic;return p;}
Transform At(Vec3 p,Vec3 scale={1,1,1},Vec3 rotation={}){return {p,rotation,scale};}
}
Simulation::Simulation(const Settings& settings,const PhysicsConfig& config):physics(world,GameConfig(config)),settings_(settings){
    physicsStep_=1/config.fixedHz;metrics.target=std::clamp(settings.entities,1000u,10000u);
    camera=world.Create("FPS camera");world.SetCamera(camera,{{0,1,0},70,.07f,500});world.SetActiveCamera(camera);
    BuildFacility();ResetPlayer();SetTarget(metrics.target);UpdateCamera();
}
Entity Simulation::Add(const char* name,const PhysicsComponent& body,const Transform& transform,unsigned colour){
    auto e=world.Create(name);world.SetTransform(e,transform);world.SetPhysics(e,body);
    visuals.push_back({e,body.collider,colour,body.body.motion==MotionType::Dynamic,body.body.sensor});return e;
}
Entity Simulation::Box(const char* name,Vec3 position,Vec3 size,unsigned colour,MotionType motion,Vec3 rotation){
    return Add(name,Body(motion),At(position,size,rotation),colour);
}
void Simulation::BuildFacility(){
    Box("Facility foundation",{0,-.6f,-35},{200,1,220},0);
    Box("Central avenue",{0,-.02f,-35},{12,.15f,210},1);
    for(float z:{0.f,-60.f})Box("Cross avenue",{0,-.01f,z},{196,.15f,8},1);
    for(unsigned region=0;region<6;++region){auto c=Center(region);
        Box("District pad",Sum(c,{0,-.02f,0}),{70,.15f,48},2+region);
        for(float x:{-32.f,32.f})for(float z:{-21.f,21.f})Box("District beacon",Sum(c,{x,2,z}),{.7f,4,.7f},8);
        auto sensor=Body(MotionType::Static);sensor.body.layer=CollisionLayer::Sensor;sensor.body.sensor=true;
        sensor.collider.dimensions={3,2,3};sensors_.push_back(Add("District sensor",sensor,At(Sum(c,{0,2,17})),10));
    }
    for(float x:{-98.f,98.f})Box("Perimeter wall",{x,2,-35},{2,4,218},0);
    for(float z:{-143.f,73.f})Box("Perimeter wall",{0,2,z},{196,4,2},0);
    // Warehouse with a broad entrance facing the central avenue.
    auto c=Center(2);
    Box("Warehouse roof",Sum(c,{0,11,0}),{62,.5f,40},8);
    Box("Warehouse west wall",Sum(c,{-31,5.5f,0}),{.5f,11,40},0);
    for(float z:{-20.f,20.f})Box("Warehouse end wall",Sum(c,{0,5.5f,z}),{62,11,.5f},0);
    for(float z:{-15.f,15.f})Box("Warehouse entrance",Sum(c,{31,5.5f,z}),{.5f,11,10},8);
    for(unsigned i=0;i<10;++i)Box("Walkable stairs",{-12, .15f*float(i+1),-22-float(i)}, {4,.3f*float(i+1),1},3);
    Box("Upper walkway",{-12,3,-37},{4,.5f,12},8);
    Box("Yard ramp",{-25,2,27},{9,.6f,13},8,MotionType::Static,{18,0,0});
    Box("Factory chute",{45,5,19},{12,.6f,16},8,MotionType::Static,{22,0,0});
    Box("Factory tower",{55,8,22},{1,16,1},8);
    Box("Factory tower",{35,8,22},{1,16,1},8);
    Box("Factory gantry",{45,16,22},{22,1,2},8);
    // Buildings are open three-sided rooms, leaving roads between them.
    c=Center(4);
    for(unsigned room=0;room<6;++room){float x=c.x+float(room%3)*20-20,z=c.z+float(room/3)*24-12;
        Box("City room back",{x,3,z-7},{14,6,.5f},6);
        Box("City room side",{x-7,3,z},{.5f,6,14},6);
        Box("City room side",{x+7,3,z},{.5f,6,14},6);
        Box("City lintel",{x,6,z+7},{14,1,.5f},8);
    }
    // Native heightfield keeps the open area visibly different from the paved districts.
    auto terrain=Body(MotionType::Static);terrain.collider.type=ColliderType::Heightfield;
    auto geometry=std::make_shared<ColliderGeometry>();geometry->samples=32;geometry->spacing={1.5f,1,1.5f};geometry->offset={19,0,-113};
    for(unsigned z=0;z<32;++z)for(unsigned x=0;x<32;++x){float edge=std::sin(float(x)*3.141593f/31)*std::sin(float(z)*3.141593f/31);geometry->heights.push_back(3*edge*edge);}
    terrain.collider.geometry=geometry;Add("Terrain hills",terrain,{},7);
    auto sun=world.Create("Exterior sunlight");LightComponent light;light.light.direction={.4f,-1,.3f};light.light.power=3;world.SetLight(sun,light);
    for(unsigned i=0;i<6;++i){auto e=world.Create("Warehouse point light");auto pos=Sum(Center(2),{float(i%3)*18-18,8,float(i/3)*20-10});world.SetTransform(e,At(pos));
        LightComponent l;l.light.type=LightType::Point;l.light.power=35;l.light.radius=24;l.light.colour={1,.75f,.4f};world.SetLight(e,l);}
    auto pivot=Box("Seesaw support",{-35,.6f,42},{1,1.2f,2},8);
    auto beamBody=Body(MotionType::Dynamic);beamBody.body.mass=30;
    auto beam=Add("Seesaw beam",beamBody,At({-35,1.4f,42},{10,.4f,2}),4);
    ConstraintDesc hinge;hinge.type=ConstraintType::Hinge;hinge.axis={0,0,1};hinge.anchorA=hinge.anchorB={-35,1.4f,42};hinge.minAngleDegrees=-25;hinge.maxAngleDegrees=25;
    pendingJoints_.push_back({pivot,beam,hinge});
    AddInfrastructure(1);
}
void Simulation::AddInfrastructure(unsigned tier){
    while(infrastructure_<tier){unsigned level=infrastructure_++;
        for(unsigned i=0;i<6;++i){auto c=Center(2);Vec3 pos=Sum(c,{float(i%3)*17-17,1.3f,float(i/3)*16-8});pos.y+=float(level)*2;
            auto t=At(pos,i==5?Vec3{.4f,5,5}:Vec3{5,.6f,4});if(i==5)t.position.y+=2;auto e=Box("Industrial machinery",t.position,t.scale,3,MotionType::Kinematic);
            movers_.push_back({e,t,i==5?4u:(i+level)%4,2});
        }
        // A two-ended hinged bridge, wide enough for the capsule to walk across.
        float z=-19-float(level)*8;Entity previous=Box("Bridge anchor",{23,2,z},{1,4,4},8);Vec3 last{23,2,z};
        for(unsigned i=0;i<10;++i){Vec3 pos{25+float(i)*2,2,z};auto p=Body(MotionType::Dynamic);p.body.mass=12;p.body.collisionGroup=100+level;
            auto e=Add("Bridge plank",p,At(pos,{1.95f,.35f,4}),8);ConstraintDesc d;d.type=ConstraintType::Hinge;d.axis={0,0,1};d.anchorA=d.anchorB={(last.x+pos.x)/2,2,z};d.minAngleDegrees=-20;d.maxAngleDegrees=20;pendingJoints_.push_back({previous,e,d});previous=e;last=pos;}
        auto end=Box("Bridge anchor",{45,2,z},{1,4,4},8);ConstraintDesc d;d.type=ConstraintType::Point;d.anchorA=d.anchorB={44,2,z};pendingJoints_.push_back({previous,end,d});
        for(float x:{21.f,47.f})Box("Bridge access ramp",{x,1,z},{5,.4f,4},8,MotionType::Static,{0,0,x<30?25.f:-25.f});
        // Independent chains and heavy pendulum bobs.
        for(unsigned chain=0;chain<3;++chain){float x=25+float(chain)*10,z2=-42-float(level)*5;auto a=Box("Chain anchor",{x,12,z2},{1,1,1},8);
            for(unsigned link=0;link<5;++link){auto p=Body(MotionType::Dynamic);p.body.mass=link==4?15:2;p.body.collisionGroup=200+level*3+chain;
                Vec3 pos{x,10.5f-float(link)*1.5f,z2};auto b=Add("Hanging chain",p,At(pos,link==4?Vec3{2,2,2}:Vec3{.4f,1.2f,.4f}),4);
                ConstraintDesc joint;joint.type=ConstraintType::Point;joint.anchorA=joint.anchorB=Sum(pos,{0,.75f,0});pendingJoints_.push_back({a,b,joint});if(link==4)newBobs_.push_back(b);a=b;}
        }
    }
}
void Simulation::CommitJoints(){physics.Synchronize();for(auto& j:pendingJoints_)physics.CreateConstraint(j.a,j.b,j.desc);pendingJoints_.clear();for(auto bob:newBobs_)physics.ApplyImpulse(bob,{45,0,0});newBobs_.clear();}
void Simulation::AddProp(){
    unsigned id=serial_++,region=id%6,local=id/6;auto c=Center(region);
    // Repeating modules are towers, domino lanes and loose cargo, separated by walkways.
    unsigned module=local/24,slot=local%24;float x=c.x-25+float(module%6)*10,z=c.z-16+float((module/6)%4)*10;
    auto p=Body(MotionType::Dynamic);unsigned shape=(id+settings_.seed)%8;
    p.collider.type=shape<4?ColliderType::Box:shape<6?ColliderType::Sphere:shape==6?ColliderType::Capsule:ColliderType::Cylinder;
    p.collider.dimensions={.4f,.4f,.4f};p.body.mass=2+float(id%5);p.collider.material.friction=.6f;p.collider.material.restitution=shape==4?.4f:.1f;
    Vec3 position{x+float(slot%3)*1.4f,2+float(slot/9)*1.2f+float(module/24)*3,z+float((slot/3)%3)*1.4f};
    if(region==0&&shape<4){position={x+float(slot%12)*.65f,1.3f+float(slot/12)*3,z};p.collider.dimensions={.15f,.9f,.5f};}
    if(region==1)position.y+=6;
    if(region==5)position.y+=5;
    auto e=Add(region==0?"Yard domino/cargo":region==1?"Collapse tower":"District cargo",p,At(position),9+id%3);
    props_.push_back({e,unsigned(visuals.size()-1),region,p.body.mass});++metrics.spawned;
}
void Simulation::SetTarget(unsigned target){
    if(target<metrics.target)Drop();
    metrics.target=std::clamp(target,1000u,10000u);
    unsigned tier=metrics.target>=6500?3:metrics.target>=2500?2:1;
    AddInfrastructure(tier);
    while(world.Size()>metrics.target&&!props_.empty()){world.Destroy(props_.back().entity);props_.pop_back();}
    std::erase_if(visuals,[&](const auto& v){return !world.Alive(v.entity);});
    // Rebuild indices only on structural edits, never per physics tick.
    std::unordered_map<std::uint64_t,unsigned> indices;for(unsigned i=0;i<visuals.size();++i)indices[visuals[i].entity.value]=i;
    for(auto& p:props_)p.visual=indices.at(p.entity.value);
    while(world.Size()<metrics.target)AddProp();
    CommitJoints();metrics.stage=1;for(unsigned i=0;i<Stages.size();++i)if(metrics.target>=Stages[i])metrics.stage=i+1;
    growthClock_=0;metrics.nextIncrease=metrics.target<10000?30:0;++visualRevision;
}
void Simulation::ResetPlayer(){Drop();if(player_)physics.DestroyCharacter(player_);CharacterDesc d;d.position={0,2,58};player_=physics.CreateCharacter(d);previousPlayer_=d.position;input_={};jump_=false;UpdateCamera();}
Vec3 Simulation::Eye()const{return Sum(Player().position,{0,.65f,0});}
void Simulation::UpdateCamera(){auto position=Player().position;float a=float(physics.Statistics().alpha);auto eye=Sum(Sum(Mul(previousPlayer_,1-a),Mul(position,a)),{0,.65f,0});world.SetTransform(camera,At(eye));world.SetCamera(camera,{Sum(eye,look_),70,.07f,500});}
unsigned Simulation::District()const{auto pos=Player().position;unsigned row=pos.z>0?0:pos.z>-60?1:2;return row*2+(pos.x>0?1:0);}
void Simulation::SetInput(Vec3 horizontal,bool jump,Vec3 look){input_=horizontal;jump_|=jump;float length=Length(look);if(length>.001f)look_=Mul(look,1/length);}
std::optional<RayHit> Simulation::Aim(float distance){auto start=Clock::now();auto hit=physics.Raycast(Eye(),Mul(look_,distance));metrics.queryMs+=Ms(start);return hit;}
void Simulation::Grab(){auto hit=Aim();if(!hit)return;auto e=physics.EntityFor(hit->body);if(!e)return;auto record=world.Read(e);if(!record.physics||record.physics->body.motion!=MotionType::Dynamic)return;held_=e;heldMass_=record.physics->body.mass;}
void Simulation::Hold(){
    if(!held_)return;
    if(!world.Alive(held_)){Drop();return;}auto state=physics.Read(held_);auto target=Sum(Eye(),Mul(look_,2.5f));
    Vec3 error{target.x-state.transform.position.x,target.y-state.transform.position.y,target.z-state.transform.position.z};if(Length(error)>8){Drop();return;}
    Vec3 force{error.x*45-state.velocity.x*12,error.y*45-state.velocity.y*12+9.81f,error.z*45-state.velocity.z*12};
    float length=Length(force);if(length>100)force=Mul(force,100/length);physics.ApplyForce(held_,Mul(force,heldMass_));
}
void Simulation::ThrowOrPush(){if(held_){physics.ApplyImpulse(held_,Mul(look_,heldMass_*14));Drop();return;}auto hit=Aim(6);if(!hit)return;auto e=physics.EntityFor(hit->body);if(e&&physics.Read(e).motion==MotionType::Dynamic)physics.ApplyImpulse(e,Mul(look_,20));}
Simulation::Prop* Simulation::Recyclable(){
    if(props_.empty())return nullptr;
    auto player=Player().position;
    for(std::size_t i=0;i<props_.size();++i){auto& p=props_[recycleCursor_++%props_.size()];if(p.entity==held_)continue;
        auto pos=world.ReadTransform(p.entity).position;float dx=pos.x-player.x,dz=pos.z-player.z;if(dx*dx+dz*dz>144)return &p;}
    return nullptr;
}
void Simulation::Recycle(unsigned count){
    for(unsigned i=0;i<count;++i){auto* prop=Recyclable();if(!prop)break;unsigned n=unsigned(metrics.recycled++);Vec3 center=Center((n/24)%6);
        Vec3 pos=Sum(center,{float(n%6)*1.5f-4,17+float((n/6)%4)*1.5f,float((n/24)%5)*2-4});auto t=world.ReadTransform(prop->entity);t.position=pos;t.rotationDegrees={};physics.Teleport(prop->entity,t);physics.SetVelocity(prop->entity,{0,-2,0});physics.SetAngularVelocity(prop->entity,{});
    }
}
void Simulation::Fire(){
    if(metrics.simulationSeconds-lastShot_<.15)return;
    auto* prop=Recyclable();if(!prop)return;lastShot_=metrics.simulationSeconds;
    auto p=Body(MotionType::Dynamic);p.body.mass=5;p.body.layer=CollisionLayer::Projectile;p.body.continuous=true;p.collider.type=ColliderType::Sphere;p.collider.dimensions={.2f,.2f,.2f};
    world.SetPhysics(prop->entity,p);physics.Synchronize();auto t=At(Sum(Eye(),Mul(look_,1.2f)));physics.Teleport(prop->entity,t);physics.SetVelocity(prop->entity,Mul(look_,65));physics.SetAngularVelocity(prop->entity,{});
    prop->mass=5;visuals[prop->visual].shape=p.collider;visuals[prop->visual].colour=11;++metrics.recycled;++visualRevision;
}
void Simulation::Disturb(){
    unsigned activated=0;const unsigned deficit=metrics.feasibleActive>physics.Statistics().physics.active?metrics.feasibleActive-physics.Statistics().physics.active:0;
    for(unsigned regionCount=0;regionCount<3&&activated<deficit;++regionCount){unsigned region=disturbRegion_++%6;
        for(auto& p:props_)if(p.region==region&&p.entity!=held_){auto state=physics.Read(p.entity);if(state.active)continue;physics.ApplyImpulse(p.entity,{float(int(p.entity.value%3)-1)*p.mass,p.mass*10,0});++activated;}
    }
    if(activated)++metrics.disturbances;
}
void Simulation::Fixed(double dt){
    auto start=Clock::now();previousPlayer_=Player().position;metrics.simulationSeconds+=dt;
    for(auto& m:movers_){auto t=m.base;float phase=float(metrics.simulationSeconds*.8)+float(m.entity.value%7);
        if(m.kind==0)t.position.y+=2+std::sin(phase)*2;else if(m.kind==1)t.position.x+=std::sin(phase)*4;else if(m.kind==2)t.rotationDegrees.y=float(metrics.simulationSeconds*45);else if(m.kind==3)t.position.z+=std::sin(phase)*4;else t.rotationDegrees.y=std::sin(phase)*70;
        world.SetTransform(m.entity,t);
    }
    physics.SetCharacterInput(player_,input_,jump_);jump_=false;Hold();metrics.gameplayMs+=Ms(start);
}
void Simulation::Advance(double dt){
    metrics.gameplayMs=metrics.queryMs=0;physics.Advance(dt,[this](double fixed){Fixed(fixed);});
    auto stats=physics.Statistics();
    for(auto event:physics.TriggerEvents()){static_cast<void>(event);++metrics.triggerEvents;}
    metrics.requestedActive=metrics.target>=10000?2000:metrics.target>=5000?1500:1000;
    metrics.feasibleActive=std::min(metrics.requestedActive,stats.physics.dynamics+stats.physics.kinematics);metrics.activityMet=stats.physics.active>=metrics.feasibleActive;
    // Use elapsed simulation time rather than render dt so pausing really stops growth/spawns.
    double elapsed=metrics.simulationSeconds-spawnClock_;
    if(elapsed>=2){Recycle(24);spawnClock_=metrics.simulationSeconds;}
    if(settings_.activity&&metrics.simulationSeconds-activityClock_>=1){if(!metrics.activityMet)Disturb();activityClock_=metrics.simulationSeconds;}
    if(metrics.simulationSeconds-zoneClock_>=.25){auto start=Clock::now();ColliderComponent sphere;sphere.type=ColliderType::Sphere;sphere.dimensions={.5f,.5f,.5f};auto hits=physics.Overlap(sphere,At(Player().position),CollisionLayer::Character);
        for(auto h:hits){auto e=physics.EntityFor(h);if(std::find(sensors_.begin(),sensors_.end(),e)!=sensors_.end())++metrics.zoneOverlaps;}metrics.queryMs+=Ms(start);zoneClock_=metrics.simulationSeconds;}
    if(settings_.progressive){growthClock_+=double(stats.steps)*physicsStep_;metrics.nextIncrease=metrics.target<10000?std::max(0.,30-growthClock_):0;if(growthClock_>=30){for(auto target:Stages)if(target>metrics.target){SetTarget(target);break;}growthClock_=0;}}
    metrics.activityMet=physics.Statistics().physics.active>=metrics.feasibleActive;
    if(Player().position.y<-20)ResetPlayer();
    UpdateCamera();
}
}
