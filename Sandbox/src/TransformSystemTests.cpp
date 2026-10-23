#include "pgcpch.h"
#include <PlagC/World/TransformSystem.h>
#include <PlagC/Thread.h>
#include <limits>
using namespace PlagC;
namespace {
void Check(bool value,const char* what) { if(!value)throw std::runtime_error(what);std::cout<<"[PASS] "<<what<<'\n'; }
template<class F>bool Rejects(F f) {try{f();}catch(const std::exception&){return true;}return false;}
}
void TestTransformSystem() {
    World world,other;TransformSystem system;
    const auto a=world.Create(), b=world.Create();const std::array selection{a,b};
    system.Bind(world,selection);auto values=system.Prepare();values[0].position.x=5;values[1].position.x=6;
    Check(world.Read(a).transform.position.x==0,"prepared system buffer is detached from ECS");
    system.Commit();Check(world.Read(a).transform.position.x==5&&world.Read(b).transform.position.x==6,"system commits selected transforms");
    Check(Rejects([&]{system.Commit();}),"system rejects double commit");
    values=system.Prepare();values[0].position.x=10;values[1].scale.x=0;
    Check(Rejects([&]{system.Commit();})&&world.Read(a).transform.position.x==5,"invalid system batch is atomic");
    values=system.Prepare();values[0].position.x=10;values[1].position.z=std::numeric_limits<float>::quiet_NaN();
    Check(Rejects([&]{system.Commit();}),"system rejects nonfinite values");
    values=system.Prepare();auto t=world.Read(b).transform;t.position.y=2;world.SetTransform(b,t);
    Check(Rejects([&]{system.Commit();})&&world.Read(b).transform.position.y==2,"system rejects intervening safe transform writes");
    const std::array duplicate{a,a};Check(Rejects([&]{system.Bind(world,duplicate);})&&system.IsBindingValid(),"failed duplicate bind preserves valid selection");
    const std::array foreign{other.Create()};Check(Rejects([&]{system.Bind(world,foreign);}),"system rejects foreign entities");
    system.Prepare();world.Destroy(b);world.Create();
    Check(!system.IsBindingValid()&&Rejects([&]{system.Commit();})&&Rejects([&]{system.Prepare();}),"entity destruction/reuse invalidates cached bindings before dereference");
    const std::array one{a};system.Bind(world,one);
    world.SetCamera(a,{{0,0,0}});
    Check(!system.IsBindingValid(),"camera component insertion invalidates cached component pointers");
    system.Bind(world,one);values=system.Prepare();values[0].position={0,0,0};
    Check(Rejects([&]{system.Commit();}),"system preserves camera position/target constraints");
    system.Prepare();world.SetCamera(a,{{1,0,0}});
    Check(Rejects([&]{system.Commit();}),"system rejects intervening camera changes");
    world.RemoveCamera(a);Check(!system.IsBindingValid(),"camera removal invalidates system binding");
    system.Bind(world,one);system.Prepare();world.Clear();
    Check(!system.IsBindingValid()&&Rejects([&]{system.Commit();}),"world clear invalidates pending system batch");
    system.Bind(world,{});Check(system.Prepare().empty(),"empty system selection is valid");system.Commit();
    { World temporary;const std::array entities{temporary.Create()};system.Bind(temporary,entities);system.Prepare(); }
    Check(!system.IsBindingValid()&&Rejects([&]{system.Commit();}),"system safely outlives its world");
    std::atomic<bool> denied=false;std::thread worker([&]{denied=Rejects([&]{system.Prepare();});});worker.join();
    Check(denied,"system access remains main-thread only");
}
