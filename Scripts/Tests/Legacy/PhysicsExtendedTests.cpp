#include "pgcpch.h"
#include <PlagC/Physics/PhysicsSystem.h>
#include <PlagC/Jobs.h>
using namespace PlagC;
namespace {
void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);std::cout<<"[PASS] "<<message<<'\n';}
template<class F> bool Rejected(F f){try{f();}catch(const std::exception&){return true;}return false;}
PhysicsConfig Config(){PhysicsConfig c;c.workers=2;c.maxBodies=4096;return c;}
PhysicsBodyDesc Floor(){PhysicsBodyDesc d;d.component.body.motion=MotionType::Static;d.component.body.layer=CollisionLayer::StaticWorld;d.component.collider.dimensions={30,.5f,30};d.transform.position.y=-.5f;return d;}
PhysicsBodyDesc Box(float y){PhysicsBodyDesc d;d.transform.position.y=y;return d;}
void Shapes(){
 PhysicsWorld w(Config());w.CreateBody(Floor());
 auto convex=Box(4);convex.component.collider.type=ColliderType::ConvexHull;auto hull=std::make_shared<ColliderGeometry>();hull->vertices={{-1,-.5f,-1},{1,-.5f,-1},{0,-.5f,1},{0,1,0}};convex.component.collider.geometry=hull;
 auto h=w.CreateBody(convex);for(int i=0;i<600;++i)w.Step();Check(w.Read(h).transform.position.y>-.1f,"convex hull collision");w.DestroyBody(h);
 auto compound=Box(4);compound.component.collider.type=ColliderType::Compound;auto parts=std::make_shared<ColliderGeometry>();parts->children={ColliderChild{},ColliderChild{}};parts->children[1].transform.position.x=1;compound.component.collider.geometry=parts;h=w.CreateBody(compound);
 for(int i=0;i<600;++i)w.Step();Check(w.Read(h).transform.position.y>-.1f,"compound collision");w.DestroyBody(h);
 PhysicsWorld meshWorld(Config());auto mesh=Floor();auto g=std::make_shared<ColliderGeometry>();g->vertices={{-10,0,-10},{10,0,-10},{10,0,10},{-10,0,10}};g->indices={0,2,1,0,3,2};mesh.component.collider.type=ColliderType::Mesh;mesh.component.collider.geometry=g;mesh.transform.position={};meshWorld.CreateBody(mesh);h=meshWorld.CreateBody(Box(4));for(int i=0;i<600;++i)meshWorld.Step();Check(meshWorld.Read(h).transform.position.y>.4f,"triangle mesh collision");
 PhysicsWorld terrain(Config());auto hf=Floor();auto heights=std::make_shared<ColliderGeometry>();heights->samples=8;heights->heights.resize(64,0);heights->offset={-3.5f,0,-3.5f};hf.component.collider.type=ColliderType::Heightfield;hf.component.collider.geometry=heights;hf.transform.position={};terrain.CreateBody(hf);h=terrain.CreateBody(Box(4));for(int i=0;i<600;++i)terrain.Step();Check(terrain.Read(h).transform.position.y>.4f,"heightfield collision");
 auto bad=mesh;bad.component.body.motion=MotionType::Dynamic;bad.component.body.layer=CollisionLayer::Dynamic;Check(Rejected([&]{w.CreateBody(bad);}),"dynamic triangle meshes rejected");
}
void QueriesAndTriggers(){
 PhysicsWorld w(Config());auto floor=w.CreateBody(Floor());ColliderComponent sphere;sphere.type=ColliderType::Sphere;auto hit=w.ShapeCast(sphere,{{0,3,0},{},{1,1,1}},{0,-5,0});Check(hit&&hit->body==floor&&hit->fraction>.4f&&hit->fraction<.6f&&hit->normal.y>.9f,"shape cast returns closest floor hit and normal");
 auto overlap=w.Overlap(sphere,{{0,.2f,0},{},{1,1,1}});Check(overlap.size()==1&&overlap[0]==floor,"shape overlap returns unique body handles");
 auto sensor=Box(1);sensor.component.body.motion=MotionType::Kinematic;sensor.component.body.layer=CollisionLayer::Sensor;sensor.component.body.sensor=true;sensor.component.collider.dimensions={2,2,2};auto zone=w.CreateBody(sensor);auto body=w.CreateBody(Box(1));w.Step();bool enter=false;for(auto e:w.TriggerEvents())enter|=e.sensor==zone&&e.other==body&&e.phase==TriggerPhase::Enter;Check(enter,"sensor enter queued after worker completion");
 bool falseExit=false,stay=false;for(int i=0;i<300;++i){w.Step();for(auto e:w.TriggerEvents())if(e.sensor==zone&&e.other==body){falseExit|=e.phase==TriggerPhase::Exit;stay|=e.phase==TriggerPhase::Stay;}}
 Check(stay&&!falseExit,"sensor stay survives body sleeping without false exit");
 w.DestroyBody(body);w.Step();bool exit=false;for(auto e:w.TriggerEvents())exit|=e.sensor==zone&&e.other==body&&e.phase==TriggerPhase::Exit;Check(exit,"destroying overlapping body emits exit with stale-safe identity");
 std::array duplicate{zone,zone};Check(Rejected([&]{w.DestroyBatch(duplicate);})&&w.Statistics().bodies==2,"duplicate removal batch rejected before mutation");std::array remove{floor,zone};w.DestroyBatch(remove);Check(w.Statistics().bodies==0,"batch body removal releases all bodies");
}
void ConstraintsAndCharacter(){
 PhysicsWorld w(Config());auto floor=w.CreateBody(Floor());auto b=w.CreateBody(Box(4));ConstraintDesc d;d.a=floor;d.b=b;d.type=ConstraintType::Distance;d.anchorA={0,5,0};d.anchorB={0,4,0};d.minDistance=d.maxDistance=1;auto joint=w.CreateConstraint(d);
 for(int i=0;i<180;++i)w.Step();Check(w.Read(b).transform.position.y>3.9f,"distance constraint suspends dynamic body");w.DestroyConstraint(joint);Check(Rejected([&]{w.DestroyConstraint(joint);}),"stale constraint rejected");
 d.type=ConstraintType::Hinge;d.anchorA=d.anchorB={0,4,0};d.motor=true;d.motorSpeed=1;joint=w.CreateConstraint(d);w.Step();w.DestroyBody(b);Check(w.Statistics().constraints==0,"body removal also releases connected constraints");
 CharacterDesc settings;auto character=w.CreateCharacter(settings);for(int i=0;i<120;++i)w.Step();auto state=w.ReadCharacter(character);Check(state.grounded&&state.position.y>.8f&&state.position.y<1.1f,"CharacterVirtual falls onto floor and becomes grounded");
 w.SetCharacterInput(character,{2,0,0});for(int i=0;i<60;++i)w.Step();Check(w.ReadCharacter(character).position.x>1.8f,"character horizontal input advances at fixed rate");w.SetCharacterInput(character,{},true);w.Step();Check(w.ReadCharacter(character).velocity.y>0,"grounded character can jump");w.DestroyCharacter(character);Check(Rejected([&]{w.ReadCharacter(character);}),"stale character rejected");
}
float RunScheduler(JobSystem* jobs){auto c=Config();if(jobs){c.scheduler=PhysicsScheduler::PlagC;c.sharedJobs=jobs;}PhysicsWorld w(c);w.CreateBody(Floor());auto b=w.CreateBody(Box(10));for(int i=0;i<120;++i)w.Step();return w.Read(b).transform.position.y;}
}
void TestPhysicsExtended(){Shapes();QueriesAndTriggers();ConstraintsAndCharacter();auto native=RunScheduler(nullptr);JobSystem jobs(2);for(int i=0;i<3;++i)Check(std::abs(RunScheduler(&jobs)-native)<.001f,"PlagC scheduler adapter matches native pool and drains across world teardown");}
