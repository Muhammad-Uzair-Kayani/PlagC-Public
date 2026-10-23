#include "pgcpch.h"
#include "IntegrationDemo.h"
#include <iomanip>
using namespace PlagC;
namespace {
using Clock = std::chrono::steady_clock;
double Ms(Clock::time_point a, Clock::time_point b) { return std::chrono::duration<double, std::milli>(b-a).count(); }
}
void IntegrationDemo::SetCount(std::size_t requested) {
    requested = std::min<std::size_t>(requested, 10000);
    if(requested==cubes_.size())return;
    while (cubes_.size() > requested) {
        auto e = cubes_.back(); world_.Destroy(e); cubes_.pop_back();
    }
    while (cubes_.size() < requested) {
        const auto i = cubes_.size();
        auto e = world_.Create("Stress cube " + std::to_string(i));
        try {
            world_.SetTransform(e, {{float(i % 32) * 2 - 31, 1.2f, -float(i / 32) * 2}, {}, {.7f, .7f, .7f}});
            world_.SetRenderable(e, {cubeMesh_, cubeMaterial_});
            cubes_.push_back(e);
        } catch (...) { world_.Destroy(e); throw; }
    }
    transforms_.Bind(world_,cubes_);
}
void IntegrationDemo::Start(Engine& e) {
    auto& r = e.Graphics();
    e.SetFPSLocked(false); // Explicit client choice: the stress scene starts uncapped.
    scene_ = std::make_unique<SceneRenderer>(r);
    cubeMesh_ = r.CreateMesh(MakePrimitive(Primitive::Cube));
    groundMesh_ = r.CreateMesh(MakePrimitive(Primitive::Plane));
    MaterialDesc metal; metal.colour = {.25f, .6f, .9f}; metal.metallic = .6f;
    cubeMaterial_ = r.CreateMaterial(metal);
    MaterialDesc floor; floor.colour = {1,1,1}; floor.colourTexture = "checker.ppm";
    groundMaterial_ = r.CreateMaterial(floor);
    SetCount(1000);
    auto ground = world_.Create("Ground");
    world_.SetTransform(ground, {{0,0,-300}, {}, {100,1,700}});
    world_.SetRenderable(ground, {groundMesh_, groundMaterial_});
    auto sun = world_.Create("Sun");
    LightComponent sunDesc; sunDesc.light.direction = {.6f,-1,.6f};
    world_.SetLight(sun, sunDesc);
    for (unsigned i=0; i<4; ++i) {
        lights_[i] = world_.Create("Point light");
        LightComponent l; l.light.type = LightType::Point; l.light.radius = 30;
        l.light.colour = {i == 0 ? 1.f : .2f, i == 1 ? 1.f : .2f, i >= 2 ? 1.f : .2f};
        l.light.power = 20; world_.SetLight(lights_[i], l);
    }
    camera_ = world_.Create("Free camera");
    world_.SetTransform(camera_, {position_, {}, {1,1,1}});
    world_.SetCamera(camera_, {{0,0,0},60,.1f,1500}); world_.SetActiveCamera(camera_);
    Camera(0);
    r.SetEnvironment({"EnvironmentTest.dds",true,true});
    scene_->Sync(world_);
    keys_ = e.Events().Subscribe(EventType::KeyPressed, [this,&e](const Event& event) { Key(e,event.key); });
    std::cout << "[INTEGRATION] 1000 cubes, FPS unlocked; +1000 every 5s up to 10000.\n"
                 "WASD move, Q/E vertical, hold RMB + mouse to look, Shift sprint.\n"
                 "F1 wireframe, F2 shadows, F3 sky, F4 reflections, F5 FPS lock, F6 VSync, F7 detailed sync profiling.\n"
                 "1/2/3/4: 100/1000/5000/10000 cubes; Insert/Delete: +/-100; G: auto growth.\n"
                 "Up/Down: CPU math iterations (1..4096); Escape: exit.\n";
}
void IntegrationDemo::Validate(Engine& e) {
    auto press=[&](KeyCode key) { Event event{EventType::KeyPressed};event.key=key;e.Events().Dispatch(event); };
    auto require=[&](std::size_t count) {
        scene_->Sync(world_);
        if(cubes_.size()!=count || world_.Size()!=count+7 || scene_->Objects()!=count+1 || scene_->Lights()!=5)
            throw std::runtime_error("integration entity/render synchronization mismatch");
    };
    require(1000);
    press(KeyCode::Insert);require(1100);
    press(KeyCode::Delete);require(1000);
    press(KeyCode::D1);require(100);
    press(KeyCode::Delete);require(0);Frame(e,.016);
    press(KeyCode::D2);require(1000);
    press(KeyCode::G);Frame(e,5.1);require(2000);
    press(KeyCode::D2);require(1000);
    press(KeyCode::F5);
    if(!e.IsFPSLocked()) throw std::runtime_error("FPS lock event failed");
    press(KeyCode::F6);
    if(!e.Graphics().Settings().vsync) throw std::runtime_error("VSync event failed");
    press(KeyCode::F5);
    if(e.IsFPSLocked()||e.Graphics().Settings().vsync) throw std::runtime_error("FPS unlock event failed");
    // Startup tests leave a fresh 1000-entity workload, unlocked and growing.
    elapsed_=growthTime_=statsTime_=snapshotMs_=jobsMs_=commitMs_=syncMs_=renderMs_=0;
    statsFrames_=0;grow_=true;
    std::cout<<"[PASS] integration input controls, automatic growth, empty world animation and FPS toggles\n";
}
void IntegrationDemo::Key(Engine& e, KeyCode key) {
    auto& r = e.Graphics();
    auto settings = r.Settings(); auto environment = r.Environment();
    switch (key) {
    case KeyCode::Escape: e.Stop(); break;
    case KeyCode::F1: settings.wireframe = !settings.wireframe; r.SetSettings(settings); break;
    case KeyCode::F2: settings.shadows = !settings.shadows; r.SetSettings(settings); break;
    case KeyCode::F3: environment.skybox = !environment.skybox; r.SetEnvironment(environment); break;
    case KeyCode::F4: environment.reflections = !environment.reflections; r.SetEnvironment(environment); break;
    case KeyCode::F5: e.SetFPSLocked(!e.IsFPSLocked()); break;
    case KeyCode::F7: profileSync_=!profileSync_;scene_->SetProfiling(profileSync_);break;
    case KeyCode::F6: r.SetVSync(!settings.vsync); break;
    case KeyCode::D1: grow_=false; SetCount(100); break;
    case KeyCode::D2: grow_=false; SetCount(1000); break;
    case KeyCode::D3: grow_=false; SetCount(5000); break;
    case KeyCode::D4: grow_=false; SetCount(10000); break;
    case KeyCode::Insert: grow_=false; SetCount(cubes_.size()+100); break;
    case KeyCode::Delete: grow_=false; SetCount(cubes_.size()>100 ? cubes_.size()-100 : 0); break;
    case KeyCode::G: grow_=!grow_; growthTime_=0; break;
    case KeyCode::Up: iterations_=std::min(4096u,iterations_*2); break;
    case KeyCode::Down: iterations_=std::max(1u,iterations_/2); break;
    default: break;
    }
}
void IntegrationDemo::Camera(double dt) {
    const bool look = Input::IsMouseButtonPressed(MouseCode::Right);
    if (look && looking_) {
        auto delta = Input::GetMouseDelta();
        yaw_ += float(delta.x)*.12f; pitch_ = std::clamp(pitch_-float(delta.y)*.12f,-89.f,89.f);
    }
    looking_ = look;
    constexpr float rad = 3.14159265f/180;
    const Vec3 forward{std::cos(yaw_*rad)*std::cos(pitch_*rad),std::sin(pitch_*rad),std::sin(yaw_*rad)*std::cos(pitch_*rad)};
    const Vec3 right{-std::sin(yaw_*rad),0,std::cos(yaw_*rad)};
    Vec3 move{};
    const auto add=[&](Vec3 v,float sign) { move.x+=v.x*sign;move.y+=v.y*sign;move.z+=v.z*sign; };
    if(Input::IsKeyPressed(KeyCode::W)) add(forward,1);
    if(Input::IsKeyPressed(KeyCode::S)) add(forward,-1);
    if(Input::IsKeyPressed(KeyCode::D)) add(right,1);
    if(Input::IsKeyPressed(KeyCode::A)) add(right,-1);
    if(Input::IsKeyPressed(KeyCode::Q)) move.y-=1;
    if(Input::IsKeyPressed(KeyCode::E)) move.y+=1;
    auto length=std::sqrt(move.x*move.x+move.y*move.y+move.z*move.z);
    if(length>0) {
        const float distance=float(dt)*(Input::IsKeyPressed(KeyCode::LeftShift)?60.f:15.f)/length;
        position_.x+=move.x*distance;position_.y+=move.y*distance;position_.z+=move.z*distance;
    }
    // Commit target first; both states stay valid even when travelling through the old target.
    world_.SetCamera(camera_, {{position_.x+forward.x,position_.y+forward.y,position_.z+forward.z},60,.1f,1500});
    world_.SetTransform(camera_, {position_, {}, {1,1,1}});
}
void IntegrationDemo::Frame(Engine& e, double dt) {
    const auto delta=std::clamp(dt,0.0,.1);
    elapsed_+=delta; growthTime_+=std::max(0.0,dt);
    if(grow_ && growthTime_>=5) { SetCount(cubes_.size()+1000); growthTime_=0; }
    const bool look = Input::IsMouseButtonPressed(MouseCode::Right);
    if (look != looking_) if (auto* window = e.MainWindow()) window->SetCursorCaptured(look);
    Camera(delta);
    for(unsigned i=0;i<4;++i) {
        const float a=float(elapsed_)*.5f+float(i)*1.5707963f;
        world_.SetTransform(lights_[i], {{20*std::cos(a),6,-25+20*std::sin(a)}, {}, {1,1,1}});
    }
    const auto a=Clock::now();
    if(!transforms_.IsBindingValid()) transforms_.Bind(world_,cubes_);
    auto updates=transforms_.Prepare();
    const auto b=Clock::now();
    const auto count=AnimateTransforms(e.Jobs(),updates,float(delta),float(elapsed_),iterations_);
    const auto c=Clock::now();
    transforms_.Commit();
    const auto d=Clock::now();
    scene_->Sync(world_);
    const auto f=Clock::now();
    snapshotMs_+=Ms(a,b);jobsMs_+=Ms(b,c);commitMs_+=Ms(c,d);syncMs_+=Ms(d,f);
    renderMs_+=e.Graphics().Statistics().cpuMilliseconds; // Previous render; CPU wall time, not GPU time.
    statsTime_+=std::max(0.0,dt);++statsFrames_;
    if(statsTime_>=1) {
        std::ostringstream s;s<<std::fixed<<std::setprecision(2);
        const double n=statsFrames_;
        s<<"PlagC | FPS "<<n/statsTime_<<" | frame "<<statsTime_*1000/n<<" ms | cubes "<<cubes_.size()
         <<" | entities "<<world_.Size()<<" | lights "<<scene_->Lights()<<" | workers "<<e.Jobs().WorkerCount()
         <<" | jobs/frame "<<count<<" | math "<<iterations_<<" | lock "<<e.IsFPSLocked()
         <<" | vsync "<<e.Graphics().Settings().vsync<<" | grow "<<grow_
         <<" | gather/job+wait/commit/sync/render ms "<<snapshotMs_/n<<"/"<<jobsMs_/n<<"/"
         <<commitMs_/n<<"/"<<syncMs_/n<<"/"<<renderMs_/n;
        const auto sync=scene_->Statistics();
        s<<" | sync visited/transform/properties "<<sync.scanned<<"/"<<sync.transforms<<"/"<<sync.propertyCalls;
        if(profileSync_)s<<" | sync LAST frame structure/resource/transform/property/light-camera/traversal ms "
            <<sync.structureMs<<"/"<<sync.resourceMs<<"/"<<sync.transformMs<<"/"<<sync.propertyMs<<"/"<<sync.lightCameraMs<<"/"<<sync.traversalMs;
        #ifdef NDEBUG
        s<<" | optimized build";
        #else
        s<<" | DEBUG build (use Release for benchmarks)";
        #endif
        if(auto* w=e.MainWindow()) w->SetTitle(s.str());
        std::cout<<s.str()<<'\n';
        statsTime_=snapshotMs_=jobsMs_=commitMs_=syncMs_=renderMs_=0;statsFrames_=0;
    }
}
void IntegrationDemo::Stop(Engine& e) {
    keys_.Reset();
    if (auto* window=e.MainWindow()) window->SetCursorCaptured(false);
    world_.Clear();scene_->Sync(world_);scene_.reset();
    auto& r=e.Graphics();r.DestroyMaterial(cubeMaterial_);r.DestroyMaterial(groundMaterial_);
    r.DestroyMesh(cubeMesh_);r.DestroyMesh(groundMesh_);r.SetEnvironment({});
    cubes_.clear();
    const auto s=r.Statistics();
    if(s.objects||s.lights||s.meshes||s.materials) throw std::runtime_error("integration resources leaked");
    std::cout<<"[PASS] integration world and renderer resources released\n";
}
