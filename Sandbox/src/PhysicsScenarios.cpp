#include "pgcpch.h"
#include "PhysicsScenarios.h"
using namespace PlagC;
namespace PhysicsLabScenes {
namespace {
using Clock=std::chrono::steady_clock;
double Ms(Clock::time_point t){return std::chrono::duration<double,std::milli>(Clock::now()-t).count();}
PhysicsComponent Static(Vec3 half={.5f,.5f,.5f}){PhysicsComponent p;p.body.motion=MotionType::Static;p.body.layer=CollisionLayer::StaticWorld;p.collider.dimensions=half;return p;}
Transform At(float x,float y,float z){return {{x,y,z},{},{1,1,1}};}
std::shared_ptr<ColliderGeometry> Terrain(unsigned side,bool heightfield){
 auto g=std::make_shared<ColliderGeometry>();g->samples=side;g->offset={-40,0,-40};g->spacing={80.f/(side-1),1,80.f/(side-1)};
 for(unsigned z=0;z<side;++z)for(unsigned x=0;x<side;++x){float px=-40+x*g->spacing.x,pz=-40+z*g->spacing.z;float y=.4f*std::sin(px*.15f)*std::cos(pz*.15f);g->heights.push_back(y);g->vertices.push_back({px,y,pz});}
 if(!heightfield)for(unsigned z=0;z+1<side;++z)for(unsigned x=0;x+1<side;++x){auto a=z*side+x,b=a+side;g->indices.insert(g->indices.end(),{a,b,a+1,a+1,b,b+1});}
 return g;
}
}
Scene::Scene(const Options& o,const PhysicsConfig& config):physics(world,config),options_(o){if(o.scene>=Count||o.bodies>16000||o.queries>10000||o.triangles>500000)throw std::invalid_argument("physics scene limits");Build();physics.Advance(0);for(auto [e,v]:velocities_)physics.SetVelocity(e,v);for(auto& j:joints_)physics.CreateConstraint(j.a,j.b,j.desc);velocities_.clear();joints_.clear();}
Entity Scene::Add(const PhysicsComponent& p,const Transform& t,unsigned colour){auto e=world.Create("Physics Lab body");world.SetTransform(e,t);world.SetPhysics(e,p);visuals.push_back({e,p.collider,colour,p.body.motion==MotionType::Dynamic});if(p.body.motion==MotionType::Dynamic)dynamics.push_back(e);if(options_.individual)physics.Synchronize();return e;}
void Scene::Floor(){Add(Static({60,.5f,60}),At(0,-.5f,0),3);}
void Scene::Build(){
 const unsigned count=options_.bodies;PhysicsComponent p;
 if(options_.scene!=6)Floor();
 switch(options_.scene){
 case 0:{
  for(unsigned i=0;i<6;++i){p={};p.collider.type=ColliderType(i);auto g=std::make_shared<ColliderGeometry>();
   if(i==4){g->vertices={{-.7f,-.5f,-.7f},{.7f,-.5f,-.7f},{0,-.5f,.7f},{0,1,0}};p.collider.geometry=g;}
   if(i==5){g->children.resize(2);g->children[0].dimensions={1,.2f,.2f};g->children[1].dimensions={.2f,1,.2f};p.collider.geometry=g;}
   if(i==5)p.collider.centerOfMassOffset={.2f,0,0};
   auto t=At(float(i)*3-8,4,0);t.rotationDegrees={15,20,10};Add(p,t,i%3);
  }
  for(unsigned i=0;i<2;++i){auto terrain=Static();terrain.collider.type=i?ColliderType::Heightfield:ColliderType::Mesh;auto g=Terrain(8,bool(i));g->spacing={1,1,1};g->offset={-3.5f,0,-3.5f};for(auto& v:g->vertices){v.x*=.08f;v.z*=.08f;}terrain.collider.geometry=g;Add(terrain,At(i?7.f:-7.f,.5f,10),3);Add({},At(i?7.f:-7.f,5,10));}
  break;
 }
 case 1:{
  for(int lane=0;lane<3;++lane){auto ramp=Static({3,.3f,8});ramp.collider.material.friction=lane==0?.01f:(lane==1?.5f:1.2f);auto t=At(float(lane*8-8),3,0);t.rotationDegrees.x=18;Add(ramp,t,unsigned(lane));
   for(unsigned i=0;i<std::max(1u,std::min(count/3,20u));++i){p={};p.collider.material=ramp.collider.material;p.collider.material.restitution=lane*.4f;p.body.mass=1+float(i%4)*3;p.body.linearDamping=.02f*float(i%5);p.body.gravityScale=options_.alternate?.5f:1;Add(p,At(float(lane*8-8)+float(i%3)-1,7+float(i/3)*1.2f,-4),unsigned(lane));}}
  break;
 }
 case 2:{
  Add(Static({.05f,20,45}),At(0,15,0),3);
  for(unsigned i=0;i<count;++i){p={};p.collider.type=ColliderType::Sphere;p.collider.dimensions={.1f,.1f,.1f};p.body.continuous=!options_.alternate;p.body.gravityScale=0;p.body.allowSleep=false;auto e=Add(p,At(-10-float(i/1000)*2,1+float(i%20)*.5f,float((i/20)%50)-25));velocities_.push_back({e,{250,0,0}});}break;
 }
 case 3:{
  unsigned n=1;while(n*(n+1)/2<count)++n;unsigned made=0;
  for(unsigned row=0;row<n&&made<count;++row)for(unsigned x=0;x<n-row&&made<count;++x,++made)Add(p,At((float(x)-float(n-row-1)/2)*1.05f,.51f+row*1.02f,0));
  break;
 }
 case 4:{
  const unsigned piles=options_.alternate?1:50;const unsigned per=(count+piles-1)/piles;
  for(unsigned i=0;i<count;++i){unsigned pile=i/per,local=i%per;float baseX=float(pile%10)*9-40,baseZ=float(pile/10)*9-18;
   if(piles==1){baseX=-12;baseZ=-12;}
   unsigned width=piles==1?25:5;Add(p,At(baseX+float(local%width)*1.01f,.51f+float(local/(width*width))*1.01f,baseZ+float((local/width)%width)*1.01f));}break;
 }
 case 5:case 7:case 8:case 12:{
  for(unsigned i=0;i<count;++i){p={};p.body.allowSleep=!(options_.scene==5&&options_.alternate);if(options_.scene==8){p.body.motion=MotionType::Kinematic;p.body.gravityScale=0;}
   float x=float(i%100)-49.5f,z=float((i/100)%100)-49.5f,y=.51f+float(i/10000)*1.1f;
   if(options_.scene==5){x*=1.1f;z*=1.1f;y+=1;}
   if(options_.scene==12){unsigned width=std::max(1u,unsigned(std::ceil(std::sqrt(double(count)))));x=(float(i%width)-float(width-1)/2)*1.5f;z=(float(i/width)-float(width-1)/2)*1.5f;}
   auto e=Add(p,At(x,y,z));if(options_.scene==8)movers_.push_back({e,At(x,y,z)});if(options_.scene==12)lod_.push_back({e,p,0,{},visuals.size()-1});
  }
  if(options_.scene==8){const unsigned sensors=options_.alternate?1000:100;for(unsigned i=0;i<sensors;++i){auto sensor=Static({1,1.5f,1});sensor.body.layer=CollisionLayer::Sensor;sensor.body.sensor=true;Add(sensor,At(float(i%32)*3-49.5f,1,float(i/32)*3-49.5f),4);}}
  break;
 }
 case 6:{
  auto terrain=Static();terrain.collider.type=options_.alternate?ColliderType::Heightfield:ColliderType::Mesh;unsigned side=unsigned(std::sqrt(double(std::max(32u,options_.triangles))/2))+1;if(options_.alternate)side=(side+3)/4*4;terrain.collider.geometry=Terrain(side,options_.alternate);Add(terrain,{},3);
  for(unsigned i=0;i<count;++i)Add({},At(float(i%40)*1.5f-30,2+float(i/1600)*1.2f,float((i/40)%40)*1.5f-30));
  break;
 }
 case 9:{
  for(unsigned i=0;i<8;++i)Add(Static({2,.15f*(i+1),.5f}),At(-8,.15f*(i+1),float(i)-4),3);
  auto ramp=At(0,1,0);ramp.rotationDegrees.x=25;Add(Static({3,.3f,6}),ramp,3);
  auto platform=Static({2,.25f,2});platform.body.motion=MotionType::Kinematic;platform.body.layer=CollisionLayer::Dynamic;auto e=Add(platform,At(8,1,0),2);movers_.push_back({e,At(8,1,0)});
  for(unsigned i=0;i<12;++i)Add({},At(float(i%4)*1.2f-2,1+float(i/4),8));
  Add(Static({4,1,3}),At(-12,1,12),3); // Raised ledge for stepping off.
  auto doorBase=Add(Static({.2f,2,.2f}),At(12,2,10),3);auto doorBody=PhysicsComponent{};doorBody.collider.dimensions={1,2,.15f};auto door=Add(doorBody,At(13,2,10),1);ConstraintDesc hinge;hinge.type=ConstraintType::Hinge;hinge.axis={0,1,0};hinge.anchorA=hinge.anchorB={12,2,10};hinge.minAngleDegrees=-90;hinge.maxAngleDegrees=90;joints_.push_back({doorBase,door,hinge});
  for(unsigned i=0;i<std::max(1u,std::min(count,32u));++i){CharacterDesc d;d.position={float(i%8)*1.5f-5,2,float(i/8)*1.5f-12};auto h=physics.CreateCharacter(d);auto entity=world.Create("Virtual character");world.SetTransform(entity,At(d.position.x,d.position.y,d.position.z));ColliderComponent capsule;capsule.type=ColliderType::Capsule;capsule.dimensions={d.radius,d.halfHeight,d.radius};visuals.push_back({entity,capsule,2,false});characters_.push_back({h,entity});}
  break;
 }
 case 10:{
  const unsigned joints=std::min(count,5000u);Entity previous{};Vec3 previousPosition{};
  for(unsigned i=0;i<joints;++i){unsigned chain=options_.alternate?0:i/10,link=options_.alternate?i:i%10;float x=float(chain%20)*4-38,z=float(chain/20)*3-35;Vec3 position{x,16-float(link)*1.1f,z};if(options_.alternate){position={float((link/100)%2?99-link%100:link%100)*1.1f-50,16,float(link/100)*1.1f-25};x=position.x;z=position.z;}
   if(link==0){previous=Add(Static({.2f,.2f,.2f}),At(x,17.1f,z),3);previousPosition={x,17.1f,z};}
   p={};p.collider.dimensions={.3f,.5f,.3f};p.body.collisionGroup=chain+1;auto e=Add(p,{position,{},{1,1,1}});ConstraintDesc d;d.type=ConstraintType(i%4);d.anchorA={(previousPosition.x+position.x)/2,(previousPosition.y+position.y)/2,(previousPosition.z+position.z)/2};d.anchorB=d.anchorA;d.minDistance=d.maxDistance=0;d.axis={0,0,1};d.motor=i%10==0;d.motorSpeed=.5f;joints_.push_back({previous,e,d});previous=e;previousPosition=position;
  }break;
 }
 case 11:Stream();break;
 case 13:{
  const unsigned dolls=std::max(1u,count/6);
  for(unsigned i=0;i<dolls;++i){float x=options_.alternate?float(i%5)*1.5f:float(i%40)*2.5f-48;float z=options_.alternate?float((i/5)%5)*1.5f:float((i/40)%40)*2.5f-48;float y=options_.alternate?3+float(i/25)*3:3+float(i/1600)*3;
   p={};p.body.layer=CollisionLayer::Ragdoll;p.body.collisionGroup=i+1;p.collider.dimensions={.3f,.5f,.2f};auto torso=Add(p,At(x,y,z));
   for(unsigned part=0;part<5;++part){p.collider.type=part==0?ColliderType::Sphere:ColliderType::Capsule;p.collider.dimensions={.18f,.3f,.18f};float px=x+(part==1?-.6f:part==2?.6f:part==3?-.2f:part==4?.2f:0);float py=y+(part==0?.8f:part<=2?0:-.9f);auto e=Add(p,At(px,py,z),1);ConstraintDesc d;d.type=ConstraintType::Hinge;d.axis=part<=2?Vec3{0,0,1}:Vec3{1,0,0};d.anchorA=d.anchorB={(x+px)/2,(y+py)/2,z};d.minAngleDegrees=-45;d.maxAngleDegrees=45;joints_.push_back({torso,e,d});}
  }break;
 }
 }
}
void Scene::Fixed(double dt){
 time_+=dt;for(auto& m:movers_){auto t=m.initial;t.position.x+=float(std::sin(time_)*2);world.SetTransform(m.entity,t);}
 for(std::size_t i=0;i<characters_.size();++i)physics.SetCharacterInput(characters_[i].first,i==0?characterMovement_:Vec3{0,0,float(std::cos(time_+i)*1.5)},i==0&&jump_);
 jump_=false;
 if(options_.scene==12)UpdateLOD();
}
void Scene::Advance(double dt){
 streamMs=0;lodMs=0;physics.Advance(dt,[this](double fixed){Fixed(fixed);});
 for(auto [h,e]:characters_){auto p=physics.ReadCharacter(h).position;world.SetTransform(e,At(p.x,p.y,p.z));}
 for(auto event:physics.TriggerEvents()){enters+=event.phase==TriggerPhase::Enter;stays+=event.phase==TriggerPhase::Stay;exits+=event.phase==TriggerPhase::Exit;}
 if(options_.scene==11){streamClock_+=dt;if(streamClock_>=5){streamClock_=0;Stream();}}
}
void Scene::Query(unsigned count){
 auto start=Clock::now();queryMarks.clear();ColliderComponent sphere;sphere.type=ColliderType::Sphere;sphere.dimensions={.25f,.25f,.25f};
 for(unsigned i=0;i<count;++i){unsigned hash=(i+options_.seed)*1664525u+1013904223u;Vec3 origin{float(hash%100)-50,12,float((hash/100)%100)-50};std::optional<RayHit> hit;
  if(i%3==0)hit=physics.Raycast(origin,{0,-20,0});else if(i%3==1)hit=physics.ShapeCast(sphere,{origin,{},{1,1,1}},{0,-20,0});else {auto found=physics.Overlap(sphere,{{origin.x,.5f,origin.z},{},{1,1,1}});if(!found.empty())hit=RayHit{found.front(),{origin.x,.5f,origin.z},{0,1,0},.5f};}
  if(i<32)queryMarks.push_back({origin,hit?hit->position:Vec3{origin.x,-8,origin.z},bool(hit)});
 }queryMs=Ms(start);
}
void Scene::Fire(Vec3 position,Vec3 direction){float length=std::sqrt(direction.x*direction.x+direction.y*direction.y+direction.z*direction.z);if(length==0||visuals.size()>=30000)return;PhysicsComponent p;p.body.layer=CollisionLayer::Projectile;p.body.continuous=true;p.body.mass=5;p.collider.type=ColliderType::Sphere;p.collider.dimensions={.2f,.2f,.2f};auto e=Add(p,{position,{},{1,1,1}},1);physics.SetVelocity(e,{direction.x*60/length,direction.y*60/length,direction.z*60/length});}
void Scene::SetCharacterMovement(Vec3 v,bool jump){characterMovement_=v;jump_|=jump;}
void Scene::Stream(){
 auto start=Clock::now();for(auto e:district_)if(world.Alive(e))world.Destroy(e);district_.clear();std::erase_if(visuals,[&](const auto& v){return !world.Alive(v.entity);});physics.Synchronize();
 for(unsigned i=0;i<options_.bodies;++i){auto e=Add(Static(),At(float(i%100)-50,.5f,float(i/100)-50),3);district_.push_back(e);}physics.Synchronize();streamMs=Ms(start);
}
void Scene::UpdateLOD(){
 auto start=Clock::now();
 std::vector<std::pair<LodBody*,int>> changes;
 // Gather first: Read() may reconcile structures, so never interleave reads and removals.
 for(auto& b:lod_){auto position=world.ReadTransform(b.entity).position;float dx=position.x-focus_.x,dz=position.z-focus_.z;float distance=std::sqrt(dx*dx+dz*dz);int next=b.level;
  if(b.level==0&&distance>17)next=distance>32?2:1;else if(b.level==1){if(distance<13)next=0;else if(distance>32)next=2;}else if(b.level==2&&distance<28)next=distance<13?0:1;
  if(next==b.level)continue;
  if(b.level<2){auto state=physics.Read(b.entity);b.velocity=state.velocity;b.angularVelocity=state.angularVelocity;}
  changes.push_back({&b,next});
 }
 for(auto [ptr,next]:changes){auto& b=*ptr;
  if(next==2)world.RemovePhysics(b.entity);else {auto component=b.full;if(next==1){component.collider.type=ColliderType::Sphere;component.collider.dimensions={.5f,.5f,.5f};component.body.continuous=false;}world.SetPhysics(b.entity,component);visuals[b.visual].collider=component.collider;}
  b.level=next;
 }
 // The first setter reconciles the complete batch; later setters use stable bindings.
 for(auto [b,next]:changes)if(next<2){physics.SetVelocity(b->entity,b->velocity);physics.SetAngularVelocity(b->entity,b->angularVelocity);}
 lodMs+=Ms(start);
}
}
