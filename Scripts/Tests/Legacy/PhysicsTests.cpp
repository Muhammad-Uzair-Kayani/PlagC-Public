#include "pgcpch.h"
#include <PlagC/Physics/PhysicsSystem.h>
#include <PlagC/World/TransformSystem.h>
#include <PlagC/Thread.h>
#include <limits>
#ifdef PC_PHYSICS_RECORDING_TEST
#include <PlagC/World/SceneRenderer.h>
namespace PlagC { Transform RecordedLastTransform(); }
#endif
using namespace PlagC;
namespace {
void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);std::cout<<"[PASS] "<<message<<'\n';}
template<class F> bool Rejects(F f){try{f();}catch(const std::exception&){return true;}return false;}
bool Near(float a,float b,float epsilon=.001f){return std::abs(a-b)<epsilon;}
PhysicsBodyDesc Box(float y=3){PhysicsBodyDesc d;d.transform.position.y=y;return d;}
PhysicsBodyDesc Floor(){auto d=Box(-.5f);d.component.body.motion=MotionType::Static;d.component.body.layer=CollisionLayer::StaticWorld;d.component.collider.dimensions={20,.5f,20};return d;}
PhysicsConfig Config(unsigned workers=0){PhysicsConfig c;c.workers=workers;c.maxBodies=2048;c.maxBodyPairs=8192;c.maxContacts=4096;return c;}
void TestClock(){
    for(double hz:{30.,60.,120.,144.}){
        FixedStepClock clock;unsigned steps=0;for(int i=0;i<int(hz*2);++i)steps+=clock.Advance(1/hz).steps;
        Check(steps==120,"two seconds at different render rates produces 120 fixed steps");
    }
    FixedStepClock clock;auto p=clock.Advance(1.005);
    Check(p.steps==4&&Near(float(p.droppedSeconds),56.f/60)&&Near(float(p.alpha),.3f),"catch-up bounded to four steps, dropped time reported, fraction preserved");
    Check(Rejects([&]{clock.Advance(-1);})&&Rejects([&]{clock.Advance(std::numeric_limits<double>::quiet_NaN());}),"invalid frame delta rejected");
}
float Falling(unsigned workers,int renderHz){
    PhysicsWorld w(Config(workers));auto h=w.CreateBody(Box(20));FixedStepClock clock;
    for(int frame=0;frame<renderHz;++frame){auto p=clock.Advance(1./renderHz);for(unsigned i=0;i<p.steps;++i)w.Step();}
    return w.Read(h).transform.position.y;
}
void TestBodies(unsigned workers){
    PhysicsWorld w(Config(workers));std::array bodies{Floor(),Box()};auto ids=w.CreateBatch(bodies);
    Check(w.Statistics().bodies==2&&w.Statistics().statics==1&&w.Statistics().dynamics==1,"batched floor and dynamic body created");
    auto before=w.Read(ids[1]).transform.position.y;w.Step();auto after=w.Read(ids[1]).transform.position.y;
    auto mid=w.Interpolated(ids[1],.5).position.y;
    Check(after<before&&Near(mid,(before+after)/2,.00001f),"gravity advances fixed step and interpolation stays between poses");
    bool finalPublished=false;unsigned settling=0;
    for(;settling<1200;++settling){
        const bool wasActive=w.Read(ids[1]).active;w.Step();
        if(wasActive&&!w.Read(ids[1]).active){for(auto& p:w.ChangedBodies())if(p.body==ids[1]&&!p.active)finalPublished=true;break;}
    }
    Check(settling<1200&&finalPublished&&Near(w.Read(ids[1]).transform.position.y,.5f,.04f),"body collides with floor, sleeps, and publishes final settled pose");
    Check(Near(w.Interpolated(ids[1],0).position.y,w.Read(ids[1]).transform.position.y),"sleep transition snaps final presentation pose");
    w.Step();Check(w.ChangedBodies().empty()&&w.Statistics().sleeping==1,"sleeping bodies produce no ECS pose work");
    w.ApplyImpulse(ids[1],{0,5,0});w.Step();Check(w.Read(ids[1]).active&&w.Read(ids[1]).velocity.y>0,"impulse wakes sleeping body");
    auto hit=w.Raycast({10,5,0},{0,-10,0});Check(hit&&hit->body==ids[0]&&Near(hit->position.y,0)&&hit->normal.y>.99f,"ray returns engine handle, point and surface normal");
    Check(!w.Raycast({30,5,0},{0,-10,0}),"ray miss returns empty");
    Check(Rejects([&]{w.ApplyImpulse(ids[0],{1,0,0});})&&Rejects([&]{w.MoveKinematic(ids[1],{});}),"motion ownership enforced");
    auto bad=Box();bad.component.body.mass=0;std::array batch{Box(),bad};auto count=w.Statistics().bodies;
    Check(Rejects([&]{w.CreateBatch(batch);})&&w.Statistics().bodies==count,"invalid final descriptor rejects entire batch");
    w.DestroyBody(ids[1]);auto replacement=w.CreateBody(Box());Check(replacement!=ids[1]&&Rejects([&]{w.Read(ids[1]);}),"body slot reuse rejects stale public handle");
    PhysicsWorld other(Config());Check(Rejects([&]{other.Read(replacement);}),"physics handles are world-specific");
    std::atomic<bool> rejected=false;std::thread thread([&]{rejected=Rejects([&]{w.Step();});});thread.join();Check(rejected,"physics API rejects external threads");
}
void TestShapesAndFilters(){
    PhysicsWorld w(Config());auto floor=w.CreateBody(Floor());(void)floor;
    for(auto shape:{ColliderType::Sphere,ColliderType::Capsule,ColliderType::Cylinder}){
        auto d=Box();d.component.collider.type=shape;auto h=w.CreateBody(d);
        for(int i=0;i<600;++i)w.Step();
        auto s=w.Read(h);const float expected=shape==ColliderType::Capsule?1.f:.5f;
        Check(Near(s.transform.position.y,expected,.06f),"primitive collider rests at expected height");w.DestroyBody(h);
    }
    auto k=Box(8);k.component.body.motion=MotionType::Kinematic;k.transform.rotationDegrees={20,30,40};auto h=w.CreateBody(k);
    auto r=w.Read(h).transform.rotationDegrees;Check(Near(r.x,20)&&Near(r.y,30)&&Near(r.z,40),"Euler Y-X-Z convention round-trips through Jolt quaternion");
    auto target=k.transform;target.position.x=1;w.MoveKinematic(h,target);w.Step();Check(Near(w.Read(h).transform.position.x,1,.01f),"kinematic target reaches requested pose in one fixed step");
    auto tele=target;tele.position.x=4;w.Teleport(h,tele);Check(Near(w.Interpolated(h,0).position.x,4),"teleport resets interpolation history");
    auto c=Config();c.collisions.Set(CollisionLayer::Dynamic,CollisionLayer::StaticWorld,false);PhysicsWorld filtered(c);filtered.CreateBody(Floor());auto falling=filtered.CreateBody(Box());
    for(int i=0;i<120;++i)filtered.Step();
    Check(filtered.Read(falling).transform.position.y<0&&!filtered.Raycast({10,5,0},{0,-10,0},CollisionLayer::Dynamic),"collision matrix filters simulation and ray queries");
    c=Config();c.maxBodies=2;PhysicsWorld small(c);small.CreateBody(Floor());std::array pair{Box(),Box()};Check(Rejects([&]{small.CreateBatch(pair);})&&small.Statistics().bodies==1,"capacity overflow leaves existing bodies intact");
}
void TestCCD(){
    auto c=Config();c.gravity={};PhysicsWorld w(c);auto wall=Floor();wall.transform.position={0,0,0};wall.component.collider.dimensions={.05f,10,10};w.CreateBody(wall);
    auto bullet=Box(0);bullet.transform.position.x=-2;bullet.component.collider.type=ColliderType::Sphere;bullet.component.collider.dimensions={.1f,.1f,.1f};bullet.component.body.continuous=true;
    auto h=w.CreateBody(bullet);w.SetVelocity(h,{250,0,0});w.Step();Check(w.Read(h).transform.position.x<0,"linear-cast CCD prevents fast sphere crossing thin wall");
}
#ifdef PC_PHYSICS_RECORDING_TEST
void TestPresentation(){
    Renderer renderer(nullptr,{});auto mesh=renderer.CreateMesh({});auto material=renderer.CreateMaterial({});
    World w;auto e=w.Create();w.SetRenderable(e,{mesh,material});SceneRenderer scene(renderer);scene.Sync(w);
    TransformUpdate pose{e,{{1,2,3},{},{1,1,1}}};scene.Sync(w,std::span(&pose,1));
    Check(Near(RecordedLastTransform().position.y,2)&&Near(w.Read(e).transform.position.y,0)&&scene.Statistics().transforms==1,"presentation updates renderer once and leaves authoritative ECS untouched");
    scene.Sync(w);Check(Near(RecordedLastTransform().position.y,0),"omitted presentation restores authoritative transform");
    scene.Clear();renderer.DestroyMesh(mesh);renderer.DestroyMaterial(material);
}
#endif
void TestBridge(){
    auto c=Config();World w;auto floor=w.Create();auto f=Floor();w.SetTransform(floor,f.transform);w.SetPhysics(floor,f.component);
    auto e=w.Create();auto b=Box();w.SetTransform(e,b.transform);w.SetPhysics(e,b.component);PhysicsSystem system(w,c);system.Advance(0);
    Check(Rejects([&]{PhysicsSystem duplicate(w,c);}),"one physics system owns each ECS world");
    TransformSystem transforms;Check(Rejects([&]{transforms.Bind(w,std::span(&e,1));})&&Rejects([&]{w.SetTransform(e,b.transform);}),"gameplay transform paths reject physics-owned dynamics");
    system.Advance(1./60);auto authoritative=w.Read(e).transform;auto presented=system.Presentation().front().transform;
    Check(authoritative.position.y<3&&Near(presented.position.y,3)&&system.Statistics().ecsWrites==1,"sparse physics writeback separates authoritative and presentation poses");
    system.Advance(1./120);Check(Near(w.Read(e).transform.position.y,authoritative.position.y)&&system.Presentation().front().transform.position.y<3,"zero-step render frame interpolates without ECS mutation");
    for(int i=0;i<600;++i)system.Advance(1./60);
    system.Advance(1./60);
    Check(system.Statistics().ecsWrites==0&&system.Presentation().empty(),"settled ECS scene has zero physics writes or presentation updates");
    auto tele=w.Read(e).transform;tele.position.y=5;system.Teleport(e,tele);system.ApplyImpulse(e,{0,1,0});system.Advance(1./60);Check(w.Read(e).transform.position.y>4.9f,"explicit teleport and impulse update authoritative ECS");
    auto hit=system.Raycast({10,5,0},{0,-10,0});Check(hit&&system.EntityFor(hit->body)==floor,"query handle maps back to ECS entity");
    auto k=w.Create();auto kd=Box(10);kd.component.body.motion=MotionType::Kinematic;w.SetTransform(k,kd.transform);w.SetPhysics(k,kd.component);system.Advance(0);
    auto t=kd.transform;t.position.x=1;w.SetTransform(k,t);system.Advance(1./30);Check(Near(system.Read(k).transform.position.x,1,.01f)&&Near(system.Read(k).velocity.x,0),"kinematic reaches target then stops during catch-up");
    w.RemovePhysics(e);system.Advance(0);Check(Rejects([&]{system.Read(e);}),"component removal releases physics body");
    w.Destroy(k);system.Advance(0);Check(system.Statistics().physics.bodies==1,"entity destruction removes bound body");
    w.Clear();system.Advance(0);Check(system.Statistics().physics.bodies==0,"world clear releases all bound bodies");
    std::unique_ptr<PhysicsSystem> survivor;{World shortLived;survivor=std::make_unique<PhysicsSystem>(shortLived,c);}Check(Rejects([&]{survivor->Advance(0);}),"expired ECS world rejected safely");survivor.reset();
}
}
void TestPhysicsExtended();

void TestPhysicsFoundation(){
    TestClock();const auto baseline=Falling(0,60);Check(Near(baseline,Falling(0,30),.00001f)&&Near(baseline,Falling(2,144),.00001f),"simulation agrees across render rates and worker counts");
    TestBodies(0);TestBodies(2);TestShapesAndFilters();TestCCD();TestBridge();TestPhysicsExtended();
#ifdef PC_PHYSICS_RECORDING_TEST
    TestPresentation();
#endif
}
#ifdef PC_PHYSICS_TEST_MAIN
#include <cstdlib>

int main(){Thread::InitializeMain();try{TestPhysicsFoundation();std::cout<<"Physics foundation tests passed\n";return 0;}catch(const std::exception& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
#endif
