#include "pgcpch.h"
#include <PlagC/Application.h>
#include "RenderDemo.h"
#include <PlagC/Thread.h>
using namespace PlagC;
namespace {
void Check(bool condition, const char* description) {
    if (!condition) throw std::runtime_error(description);
    std::cout << "[PASS] " << description << '\n';
}
void Submit(JobSystem& jobs, JobGroup& group, JobFunction fn, void* context) {
    while (true) {
        auto result = jobs.TrySubmit(group,fn,context);
        if (result == SubmitResult::Accepted) return;
        if (result == SubmitResult::Stopped) throw std::runtime_error("unexpected stopped scheduler");
        std::this_thread::yield();
    }
}
void Increment(void* value) { static_cast<std::atomic<unsigned>*>(value)->fetch_add(1); }
struct Nested { JobSystem* jobs; std::atomic<unsigned>* count; };
void SpawnChildren(void* data) {
    auto& n = *static_cast<Nested*>(data);
    JobGroup children;
    for (int i=0;i<128;++i) {
        // Never block indefinitely on capacity from a worker: help accepted children first.
        while (n.jobs->TrySubmit(children,Increment,n.count) == SubmitResult::Full) n.jobs->Wait(children);
    }
    n.jobs->Wait(children);
}
struct Gate { std::atomic<bool> started{false}, release{false}; };
void Block(void* data) {
    auto& gate = *static_cast<Gate*>(data);
    gate.started.store(true); gate.started.notify_all();
    while (!gate.release.load()) gate.release.wait(false);
}
void TestJobs(JobSystem& jobs) {
    constexpr unsigned Count = 24000;
    auto hits = std::make_unique<std::atomic<unsigned>[]>(Count);
    JobGroup group;
    std::vector<std::thread> producers;
    for (unsigned p=0;p<4;++p) producers.emplace_back([&,p]{
        Thread::Register("Producer-" + std::to_string(p));
        for (unsigned i=p;i<Count;i+=4) Submit(jobs,group,Increment,&hits[i]);
    });
    for (auto& p : producers) p.join();
    jobs.Wait(group);
    bool once = true;
    for (unsigned i=0;i<Count;++i) once = once && hits[i].load() == 1;
    Check(once,"24,000 jobs, four concurrent producers, exactly once");
    std::atomic<unsigned> children{0};
    Nested nested{&jobs,&children};
    for (int i=0;i<16;++i) Submit(jobs,group,SpawnChildren,&nested);
    jobs.Wait(group);
    Check(children == 2048,"nested worker submission and helping waits");
    Submit(jobs,group,[](void*){ throw std::runtime_error("intentional"); },nullptr);
    jobs.Wait(group);
    Check(group.failed == 1,"job exception counted; worker survives");
    struct SelfWait { JobSystem* jobs; JobGroup* group; } self{&jobs,&group};
    Submit(jobs,group,[](void* p){ auto& s=*static_cast<SelfWait*>(p); s.jobs->Wait(*s.group); },&self);
    jobs.Wait(group);
    Check(group.failed == 2,"self-group wait rejected instead of deadlocking");
    for (int i=0;i<16;++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        Submit(jobs,group,Increment,&children); jobs.Wait(group);
    }
    Check(children == 2064,"idle workers wake repeatedly without lost jobs");
    if (jobs.WorkerCount() > 1) {
        struct StealTest { JobSystem* jobs; std::atomic<unsigned> finished{0}; } state{&jobs};
        Submit(jobs,group,[](void* p){
            auto& s=*static_cast<StealTest*>(p);
            JobGroup childrenGroup;
            for (int i=0;i<64;++i) {
                if (s.jobs->TrySubmit(childrenGroup,Increment,&s.finished) != SubmitResult::Accepted) throw std::runtime_error("steal setup full");
            }
            // Owner deliberately does not pop. Other workers must steal these children.
            const auto deadline = std::chrono::steady_clock::now()+std::chrono::seconds(5);
            while (s.finished.load()!=64 && std::chrono::steady_clock::now()<deadline) std::this_thread::yield();
            const bool stolen = s.finished.load()==64;
            s.jobs->Wait(childrenGroup);
            if (!stolen) throw std::runtime_error("steal timeout");
        },&state);
        jobs.Wait(group);
        Check(group.failed == 2 && jobs.Stats().stolen > 0,"workers steal owner-queued jobs");
    }
    auto stats=jobs.Stats();
    Check(stats.submitted == stats.executed,"submitted/executed accounting agrees after waits");
    std::cout << "[STATS] submitted=" << stats.submitted << " executed=" << stats.executed << " stolen=" << stats.stolen << " failed=" << stats.failed << '\n';
}
void TestCapacityAndShutdown() {
    JobSystem jobs(1);
    Gate gate;
    JobGroup group;
    Submit(jobs,group,Block,&gate);
    while (!gate.started.load()) gate.started.wait(false);
    std::atomic<unsigned> hits{0};
    unsigned accepted=0;
    while (jobs.TrySubmit(group,Increment,&hits)==SubmitResult::Accepted) ++accepted;
    const bool full=accepted==4095;
    gate.release.store(true); gate.release.notify_all();
    jobs.Shutdown();
    Check(full,"bounded pool reports Full without allocation or blocking");
    Check(hits==accepted && group.pending==0,"shutdown drains all accepted jobs");
    Check(jobs.TrySubmit(group,Increment,&hits)==SubmitResult::Stopped,"submission rejected after shutdown");
    jobs.Shutdown();
    Check(true,"shutdown is idempotent");
}
void TestConcurrentShutdown() {
    JobSystem jobs(4);
    JobGroup group;
    std::atomic<unsigned> hits{0}, accepted{0}, stopped{0};
    std::vector<std::thread> producers;
    for (int p=0;p<4;++p) producers.emplace_back([&]{
        while (true) {
            auto result=jobs.TrySubmit(group,Increment,&hits);
            if (result==SubmitResult::Stopped) { ++stopped; break; }
            if (result==SubmitResult::Accepted) ++accepted;
            else std::this_thread::yield();
        }
    });
    while (accepted.load()<1000) std::this_thread::yield();
    jobs.Shutdown();
    for (auto& p:producers) p.join();
    Check(stopped==4 && hits==accepted && group.pending==0,
          "shutdown races safely with four active producers");
}
void TestEvents() {
    EventBus events;
    int first=0,second=0,late=0;
    EventBus::Connection b,added;
    auto a=events.Subscribe(EventType::User,[&](const Event&){ ++first; b.Reset(); if (!late && first==1) added=events.Subscribe(EventType::User,[&](const Event&){ ++late; }); });
    b=events.Subscribe(EventType::User,[&](const Event&){ ++second; });
    events.Dispatch({EventType::User});
    Check(first==1 && second==0 && late==0,"unsubscribe during dispatch; new subscription deferred");
    events.Dispatch({EventType::User});
    Check(first==2 && late==1,"subscriptions become visible next dispatch");
    a.Reset(); added.Reset();
    int nested=0;
    auto reentrant=events.Subscribe(EventType::User,[&](const Event& e){ ++nested; if (e.code==0) events.Dispatch({EventType::User,0,0,1}); });
    events.Dispatch({EventType::User});
    Check(nested==2,"synchronous event dispatch supports reentrancy");
    reentrant.Reset();
    std::array<int,4> expected{};
    bool ordered=true;
    auto queued=events.Subscribe(EventType::User,[&](const Event& e){ const auto producer=static_cast<unsigned>(e.x); ordered=ordered && e.code==expected[producer]++; });
    std::vector<std::thread> publishers;
    std::atomic<unsigned> failures{0};
    for (unsigned p=0;p<4;++p) publishers.emplace_back([&,p]{ for (int i=0;i<1000;++i) if (!events.Post({EventType::User,double(p),0,i})) ++failures; });
    for (auto& p:publishers) p.join();
    Check(events.Pump()==4000 && ordered && failures==0,"cross-thread events preserve each producer's order");
    queued.Reset();
    for (unsigned i=0;i<8192;++i) if (!events.Post({EventType::User})) throw std::runtime_error("early event overflow");
    Check(!events.Post({EventType::User}),"bounded event queue reports overflow");
    events.Pump();
    std::atomic<bool> rejected{false};
    std::thread wrong([&]{try { events.Dispatch({EventType::User}); } catch (const std::logic_error&) { rejected=true; }}); wrong.join();
    Check(rejected,"dispatch enforces owning thread");
    EventBus::Connection outlives;
    { EventBus temporary; outlives=temporary.Subscribe(EventType::Close,[](const Event&){}); }
    outlives.Reset();
    Check(true,"connection safely outlives bus");
    auto throwing=events.Subscribe(EventType::Close,[](const Event&){ throw std::runtime_error("intentional event failure"); });
    bool threw=false;
    try { events.Dispatch({EventType::Close}); } catch (const std::runtime_error&) { threw=true; }
    throwing.Reset(); events.Dispatch({EventType::Close});
    Check(threw,"throwing callback restores dispatcher state");
}
class Sandbox final : public Application {
    EngineConfig config_;
    std::vector<EventBus::Connection> subscriptions_;
    unsigned frames_=0;
    RenderDemo demo_;
public:
    explicit Sandbox(EngineConfig config):config_(config){}
    void OnStart(Engine& engine) override {
        TestEvents(); TestJobs(engine.Jobs()); TestCapacityAndShutdown(); TestConcurrentShutdown();
        struct LogJob { Log* log; std::atomic<unsigned>* count; };
        std::atomic<unsigned> logged{0}; LogJob context{&engine.Logger(),&logged}; JobGroup group;
        for (int i=0;i<16;++i) Submit(engine.Jobs(),group,[](void* p){
            auto& s=*static_cast<LogJob*>(p);
            s.log->Write(Severity::Info,"SANDBOX","worker logging validation");
            if (Thread::IsWorker()) ++*s.count;
        },&context);
        engine.Jobs().Wait(group);
        auto records=engine.Logger().Snapshot();
        bool metadata=true;
        for (const auto& r:records) metadata=metadata && !r.timestamp.empty() && !r.thread.empty() && !r.threadID.empty();
        Check(logged==16 && metadata,"worker identity and structured logging snapshots");
        if (auto* window=engine.MainWindow()) {
            window->SetTitle("PlagC | Phase 2 renderer | Escape to close");
            window->Resize(960,540); window->Poll();
            Check(window->Width()==960 && window->Height()==540,"GLFW window creation, title and resize");
            for (int type=0;type<static_cast<int>(EventType::User);++type) subscriptions_.push_back(engine.Events().Subscribe(static_cast<EventType>(type),[this,&engine](const Event& e){
                engine.Logger().Write(Severity::Info,"EVENT",std::string(EventName(e.type))+" code="+std::to_string(e.code)+" x="+std::to_string(e.x)+" y="+std::to_string(e.y));
                if (e.type==EventType::KeyPressed && e.code==256) engine.Stop();
                if (e.type==EventType::KeyPressed && e.code==87) demo_.ToggleWireframe(engine.Graphics());
                if (e.type==EventType::KeyPressed && e.code==83) demo_.ToggleShadows(engine.Graphics());
            }));
        }
        engine.Logger().Write(Severity::Info,"SANDBOX","All automated Phase 1 checks passed");
        demo_.Start(engine.Graphics(),!config_.headless&&!config_.nullPlatform);
    }
    void OnFrame(Engine& engine,double dt) override {
        demo_.Frame(engine.Graphics(),dt);
        if ((config_.headless || config_.nullPlatform || config_.offscreen) && !config_.frameLimit && ++frames_>=6) engine.Stop();
    }
    void OnStop(Engine& engine) override {
        demo_.Stop(engine.Graphics());
        if (auto* window=engine.MainWindow()) { window->RequestClose(); Check(window->ShouldClose(),"programmatic close request"); }
        subscriptions_.clear();
    }
};
}
std::unique_ptr<Application> PlagC::CreateApplication(const EngineConfig& config) { return std::make_unique<Sandbox>(config); }
