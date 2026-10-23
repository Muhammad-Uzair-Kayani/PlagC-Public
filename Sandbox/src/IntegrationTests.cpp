#include "pgcpch.h"
#include "IntegrationDemo.h"
#include <PlagC/Thread.h>
using namespace PlagC;
namespace {
void Require(bool ok,const char* what) { if(!ok) throw std::runtime_error(what); std::cout<<"[PASS] "<<what<<'\n'; }
}
void TestInputAndPacing() {
    InputState input;
    EventBus bus;
    Event event{EventType::KeyPressed};event.key=KeyCode::W;
    unsigned pressed=0,released=0;
    auto p=bus.Subscribe(EventType::KeyPressed,[&](const Event& e){ if(e.key==KeyCode::W) ++pressed; });
    auto r=bus.Subscribe(EventType::KeyReleased,[&](const Event& e){ if(e.key==KeyCode::W) ++released; });
    auto feed=[&](Event e){ input.Apply(e); if(!bus.Post(e)) throw std::runtime_error("test event overflow"); };
    feed(event);event.type=EventType::KeyRepeated;feed(event);bus.Pump();
    Require(input.IsKeyPressed(KeyCode::W)&&pressed==1,"held/repeated input stays down without repeated press toggles");
    input.BeginFrame();Require(input.IsKeyPressed(KeyCode::W),"BeginFrame preserves held keys");
    event.type=EventType::KeyReleased;feed(event);bus.Pump();
    Require(!input.IsKeyPressed(KeyCode::W)&&released==1,"key release updates state and event subscriber");
    Event mouse{EventType::MousePressed};mouse.button=MouseCode::Right;feed(mouse);
    Require(input.IsMouseButtonPressed(MouseCode::Right),"mouse button press polling");
    mouse.type=EventType::MouseReleased;feed(mouse);
    Require(!input.IsMouseButtonPressed(MouseCode::Right),"mouse button release polling");
    feed({EventType::MouseMoved,100,200});
    Require(input.GetMouseDelta().x==0,"initial cursor position does not cause a jump");
    feed({EventType::MouseMoved,110,190});feed({EventType::MouseMoved,115,195});
    feed({EventType::MouseScrolled,1,2});feed({EventType::MouseScrolled,-1,3});
    Require(input.GetMousePosition().x==115&&input.GetMouseDelta().x==15&&input.GetMouseDelta().y==-5&&input.GetScrollDelta().y==5,
            "mouse movement and scroll accumulate within one frame");
    input.BeginFrame();Require(input.GetMouseDelta().x==0&&input.GetScrollDelta().y==0&&input.GetMousePosition().x==115,
                               "new frame clears transient state, retains cursor position");
    event.type=EventType::KeyPressed;feed(event);mouse.type=EventType::MousePressed;feed(mouse);
    feed({EventType::Focus,0,0,0});
    Require(!input.IsKeyPressed(KeyCode::W)&&!input.IsMouseButtonPressed(MouseCode::Right),"focus loss clears held input");
    feed({EventType::MouseMoved,500,500});Require(input.GetMouseDelta().x==0,"focus restoration rebases mouse delta");
    Require(!input.IsKeyPressed(static_cast<KeyCode>(65535))&&!input.IsMouseButtonPressed(MouseCode::Unknown),"unknown and invalid codes safely return false");
    std::atomic<bool> denied=false;
    std::thread worker([&]{try{Input::GetMouseDelta();}catch(const std::logic_error&){denied=true;}});worker.join();
    Require(denied,"global input facade rejects worker reads");
    FrameLimiter limiter;const auto start=FrameLimiter::Clock::time_point{};
    auto budget=limiter.Remaining(start,start);
    Require(budget>std::chrono::milliseconds(16)&&budget<std::chrono::milliseconds(17),"locked pacing budget is 60 FPS");
    Require(limiter.Remaining(start,start+std::chrono::milliseconds(20))==FrameLimiter::Clock::duration::zero(),"slow frames never add pacing delay");
    limiter.SetLocked(false);
    Require(!limiter.IsLocked()&&limiter.Remaining(start,start)==FrameLimiter::Clock::duration::zero(),"false disables all software pacing");
    limiter.SetLocked(true);Require(limiter.Remaining(start,start)==budget,"FPS lock can be re-enabled");
}
void TestIntegrationWork(JobSystem& jobs) {
    World world;
    for(unsigned i=0;i<1000;++i)world.Create("Stress regression");
    auto updates=world.TransformSnapshot();
    auto before=jobs.Stats().executed;
    auto count=AnimateIntegration(jobs,updates,1,2,4);
    Require(count>0&&jobs.Stats().executed-before==count,"stress workload submits and completes all worker chunks");
    Require(world.Read(updates.front().entity).transform.rotationDegrees.y==0,"stress jobs leave live ECS unchanged until commit");
    world.ApplyTransforms(updates);
    bool valid=true;
    for(auto& u:world.TransformSnapshot())valid&=u.transform.rotationDegrees.y==45&&std::isfinite(u.transform.position.y);
    Require(valid,"1000 job-generated transforms commit exactly once");
    TransformSystem fast;
    std::vector<Entity> selection;for(const auto& u:updates)selection.push_back(u.entity);
    fast.Bind(world,selection);auto packed=fast.Prepare();
    auto expected=world.TransformSnapshot();
    AnimateIntegration(jobs,expected,.1f,3,4);AnimateTransforms(jobs,packed,.1f,3,4);fast.Commit();
    bool equivalent=true;const auto actual=world.TransformSnapshot();
    for(std::size_t i=0;i<actual.size();++i) equivalent &= actual[i].transform.rotationDegrees.y==expected[i].transform.rotationDegrees.y && actual[i].transform.position.y==expected[i].transform.position.y;
    Require(equivalent,"packed transform jobs match safe-path results for every entity");
    for(unsigned i=0;i<1000;++i)world.Create("Growth regression");
    updates=world.TransformSnapshot();AnimateIntegration(jobs,updates,.1f,3,1);world.ApplyTransforms(updates);
    Require(world.Size()==2000,"incremental entity growth updates newly created entities");
    auto stale=updates.back().entity;world.Destroy(stale);updates=world.TransformSnapshot();
    AnimateIntegration(jobs,updates,.1f,4,1);world.ApplyTransforms(updates);
    Require(!world.Alive(stale)&&world.Size()==1999,"destroyed entities excluded from next job snapshot");
    updates.clear();Require(AnimateIntegration(jobs,updates,1,1,1)==0,"empty scene submits no animation jobs");
    world.Clear();
}
#ifdef PC_INTEGRATION_TEST_MAIN
int main() {
    try {
        Thread::InitializeMain();
        TestTransformSystem();
        TestInputAndPacing();
        for(unsigned workers : {1u,4u}) { JobSystem jobs(workers);TestIntegrationWork(jobs); }
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
}
#endif
