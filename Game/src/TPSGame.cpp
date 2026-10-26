#include "pgcpch.h"
#include "TPSGame.h"
#include <PlagC/Input/Input.h>
using namespace PlagC;
namespace Game
{
namespace
{
constexpr float Rad = 3.14159265359f / 180;
Vec3 Add(Vec3 a, Vec3 b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}
Vec3 Sub(Vec3 a, Vec3 b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}
Vec3 Scale(Vec3 a, float s)
{
    return {a.x * s, a.y * s, a.z * s};
}
float Length(Vec3 v)
{
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}
void StartupDiagnostics()
{
    namespace fs = std::filesystem;
    std::cout << "\n============================================================\n";
    std::cout << " PlagC RUNTIME DIAGNOSTICS\n";
    std::cout << " Project : Sandbox -> Game::TPSGame -> OUTPOST ZERO\n";
#ifdef PC_TPS_VISUAL_MAX
    std::cout << " Visual  : PC_TPS_VISUAL_MAX = ON\n";
#else
    std::cout << " Visual  : PC_TPS_VISUAL_MAX = OFF  [ERROR: AAA dressing disabled]\n";
#endif
#ifdef PC_DIST
    std::cout << " Config  : Dist\n";
#elif defined(PC_DEBUG)
    std::cout << " Config  : Debug\n";
#else
    std::cout << " Config  : Release\n";
#endif
    std::error_code ec;
    std::cout << " CWD     : " << fs::current_path(ec).string() << "\n";
    const char *required[] = {
        "Assets/Cooked/TPS/Operative.plasset", "Assets/Cooked/TPS/Crate.plasset",
        "Assets/Cooked/TPS/Generator.plasset", "Assets/Cooked/TPS/AAA/SecurityCamera.plasset",
        "Assets/Cooked/TPS/AAA/FactoryFacade.plasset", "Assets/Cooked/TPS/AAA/IndustrialPipes.plasset",
        "Assets/Cooked/TPS/AAA/SteelShelves.plasset", "Assets/Cooked/TPS/AAA/ExteriorAircon.plasset",
        "Assets/Cooked/TPS/AAA/UtilityBox.plasset", "Assets/Cooked/TPS/AAA/Barrel.plasset",
        "Assets/Cooked/TPS/AAA/PlasticCrate.plasset", "Assets/Cooked/TPS/AAA/Airduct.plasset",
        "Assets/Cooked/TPS/AAA/SecurityCamera02.plasset", "Assets/Cooked/TPS/AAA/ModularPipes.plasset",
        "Assets/Cooked/TPS/AAA/CircularAirduct.plasset", "Assets/Cooked/TPS/AAA/ElectricCables.plasset",
        "Assets/Cooked/TPS/AAA/ChainlinkFence.plasset", "Assets/Cooked/TPS/AAA/RoadBarrier.plasset",
        "Assets/Cooked/TPS/AAA/Jerrycan.plasset", "Assets/Cooked/TPS/AAA/WallLamp.plasset",
        "Assets/Cooked/TPS/AAA/CagedSconce.plasset", "Assets/Cooked/TPS/AAA/FluorescentLights.plasset",
        "Assets/Cooked/TPS/AAA/FireAlarm.plasset", "Assets/Cooked/TPS/AAA/Toolbox.plasset",
        "Assets/Cooked/TPS/AAA/HandTruck.plasset"};
    unsigned present = 0;
    for (auto path : required)
    {
        const bool ok = fs::exists(path, ec);
        present += ok ? 1u : 0u;
        std::cout << " [" << (ok ? "OK     " : "MISSING") << "] " << path << "\n";
    }
    std::cout << " Assets  : " << present << "/" << std::size(required) << " required runtime assets present\n";
    if (present != std::size(required))
        std::cout << " ACTION  : run BuildAAAAssets.bat, rebuild Sandbox, then verify staged Assets/Cooked.\n";
    else
        std::cout << " Status  : runtime asset set is complete.\n";
    std::cout << "============================================================\n\n";
}
} // namespace
void TPSGame::Capture(Engine &e, bool enabled)
{
    captured_ = enabled;
    if (auto w = e.MainWindow())
        w->SetCursorCaptured(enabled);
}
void TPSGame::Start(Engine &e, const EngineConfig &config)
{
    StartupDiagnostics();
    config_ = config;
    e.SetFPSLocked(false);
    health_ = {};
    weapon_ = {};
    hits_ = 0;
    deadTime_ = shotTime_ = 0;
    camera_ = {};
    heading_ = 180;
    doorSlide_ = 0;
    doorOpen_ = false;
    fallPeak_ = 1;
    restart_ = false;
    operative_ = e.Assets().Load<ModelAsset>("TPS/Operative.plasset");
    crate_ = e.Assets().Load<ModelAsset>("TPS/Crate.plasset");
    generator_ = e.Assets().Load<ModelAsset>("TPS/Generator.plasset");
    world_ = std::make_unique<World>();
    PhysicsConfig p;
    p.workers = config.physicsWorkers;
    p.fixedHz = config.physicsHz;
    p.scheduler = config.physicsScheduler == "plagc" ? PhysicsScheduler::PlagC : PhysicsScheduler::Native;
    p.sharedJobs = &e.Jobs();
    physics_ = std::make_unique<PhysicsSystem>(*world_, p);
    scene_ = std::make_unique<SceneRenderer>(e.Graphics());
    assets_ = std::make_unique<AssetScene>(e.Graphics());
    cube_ = e.Graphics().CreateMesh(MakePrimitive(Primitive::Cube));
    for (Vec3 colour : {Vec3{.27f,.30f,.31f},Vec3{.42f,.43f,.39f},Vec3{.14f,.2f,.24f},Vec3{.22f,.3f,.24f},Vec3{.8f,.24f,.08f},Vec3{.15f,.6f,.28f},Vec3{.15f,.7f,.9f}})
    {
        MaterialDesc m; m.colour=colour; m.roughness=.72f; m.metallic=materials_.size()==2?.65f:0;
        std::vector<std::uint8_t> rgba(32*32*4),normal(rgba.size()),rough(rgba.size());
        for(unsigned y=0;y<32;++y) for(unsigned x=0;x<32;++x){auto i=(y*32+x)*4;unsigned v=235+((x*17+y*13)%20);if(x==0||y==0)v=205;rgba[i]=rgba[i+1]=rgba[i+2]=std::uint8_t(v);rgba[i+3]=255;normal[i]=128;normal[i+1]=128;normal[i+2]=255;normal[i+3]=255;rough[i]=rough[i+1]=rough[i+2]=180;rough[i+3]=255;}
        m.baseColourTexture=e.Graphics().CreateTexture(32,32,rgba,true,true);textures_.push_back(m.baseColourTexture);m.normalTexture=e.Graphics().CreateTexture(32,32,normal,false,true);textures_.push_back(m.normalTexture);m.roughnessTexture=e.Graphics().CreateTexture(32,32,rough,false,true);textures_.push_back(m.roughnessTexture);if(materials_.size()==6)m.emissive={.1f,.45f,.7f};materials_.push_back(e.Graphics().CreateMaterial(m));
    }
    BuildFacility(e);
    physics_->Synchronize();
    character_=physics_->CreateCharacter({{0,1.1f,27}}); actor_=world_->Create("Operative"); world_->SetTransform(actor_,{{0,.2f,27},{0,180,0},{.82f,.82f,.82f}}); world_->SetModel(actor_,{operative_});
    AnimationStateMachine graph; graph.parameters.push_back({"Mode",AnimationParameterType::Float,0});
    for(const char *name:{"Idle","Walk","Run","Aim","Fire","Reload","Jump","Death"}){auto found=std::find_if(operative_->animations.begin(),operative_->animations.end(),[&](const auto &clip){return clip.name==name;});if(found==operative_->animations.end())throw std::runtime_error(std::string("Missing TPS clip: ")+name);graph.states.push_back({name,unsigned(found-operative_->animations.begin()),std::string(name)!="Death"&&std::string(name)!="Reload",1});}
    for(unsigned i=0;i<graph.states.size();++i)for(unsigned j=0;j<graph.states.size();++j)if(i!=j)graph.transitions.push_back({i,j,.12f,{{"Mode",AnimationCompare::Equal,float(j)}}});
    AnimatorComponent animator;animator.controller.emplace(graph,unsigned(operative_->animations.size()));world_->SetAnimator(actor_,animator);
    if(e.Audio().Available())for(unsigned kind=0;kind<5;++kind){auto clip=std::make_shared<AudioClip>();clip->sampleRate=48000;clip->channels=1;unsigned count=kind==4?48000:kind==0?8000:5000;std::uint32_t noise=1234567;for(unsigned i=0;i<count;++i){noise=noise*1664525u+1013904223u;float t=float(i)/48000;float wave=kind==4?std::sin(2*3.14159265f*60*t)*.15f:(float(noise&65535)/32768-1)*std::exp(-t*(kind==0?38:60))*.45f;clip->samples.push_back(std::int16_t(wave*24000));}AudioSourceDesc d;d.loop=kind==4;d.position=kind==4?Vec3{8,-3,-54}:Vec3{};d.gain=kind==4?.3f:.65f;d.referenceDistance=3;sounds_.push_back(e.Audio().CreateSource(AssetHandle<AudioClip>(clip),d));if(kind==4)e.Audio().Play(sounds_.back());}
    flash_=e.Graphics().CreateObject(cube_,materials_[6]);e.Graphics().SetVisible(flash_,false);e.Graphics().SetCastShadows(flash_,false);e.Graphics().SetEnvironment({"PlagCEnvironment.dds",true,true});auto settings=e.Graphics().Settings();settings.hdr=true;settings.exposure=1.1f;e.Graphics().SetSettings(settings);
    key_=e.Events().Subscribe(EventType::KeyPressed,[this,&e](const Event &ev){switch(ev.key){case KeyCode::Escape:Capture(e,!captured_);break;case KeyCode::Space:if(captured_)jump_=true;break;case KeyCode::R:if(captured_)Reload();break;case KeyCode::E:if(captured_)Interact(e);break;case KeyCode::Backspace:restart_=true;break;case KeyCode::F3:stats_=!stats_;break;case KeyCode::F4:{auto s=e.Graphics().Settings();s.wireframe=!s.wireframe;e.Graphics().SetSettings(s);break;}default:break;}});
    focus_=e.Events().Subscribe(EventType::WindowFocus,[this,&e](const Event &ev){if(!ev.focused)Capture(e,false);});
    mouse_=e.Events().Subscribe(EventType::MouseButtonPressed,[this,&e](const Event &){if(!captured_)Capture(e,true);});
    Capture(e,true);
}
