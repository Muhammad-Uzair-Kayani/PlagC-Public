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
void StartupDiagnostics(Engine &engine)
{
    namespace fs = std::filesystem;
    std::ostringstream report;
    report << "\n============================================================\n PlagC RUNTIME "
              "DIAGNOSTICS\n Project : Sandbox -> Game::TPSGame -> OUTPOST ZERO\n";
#ifdef PC_TPS_VISUAL_MAX
    report << " Visual  : PC_TPS_VISUAL_MAX = ON\n";
#else
    report << " Visual  : PC_TPS_VISUAL_MAX = OFF (base regression profile)\n";
#endif
#ifdef PC_DIST
    report << " Config  : Dist\n";
#elif defined(PC_DEBUG)
    report << " Config  : Debug\n";
#else
    report << " Config  : Release\n";
#endif
    std::error_code ec;
    report << " CWD     : " << fs::current_path(ec).string() << "\n";
    const char *required[] = {"Assets/Cooked/TPS/Operative.plasset",
                              "Assets/Cooked/TPS/Crate.plasset",
                              "Assets/Cooked/TPS/Generator.plasset",
#ifdef PC_TPS_VISUAL_MAX
                              "Assets/Cooked/TPS/Human.plasset",
                              "Assets/Cooked/TPS/AAA/Crate.plasset",
                              "Assets/Cooked/TPS/AAA/Generator.plasset",
                              "Assets/Cooked/TPS/AAA/SecurityCamera.plasset",
                              "Assets/Cooked/TPS/AAA/FactoryFacade.plasset",
                              "Assets/Cooked/TPS/AAA/IndustrialPipes.plasset",
                              "Assets/Cooked/TPS/AAA/SteelShelves.plasset",
                              "Assets/Cooked/TPS/AAA/ExteriorAircon.plasset",
                              "Assets/Cooked/TPS/AAA/UtilityBox.plasset",
                              "Assets/Cooked/TPS/AAA/Barrel.plasset",
                              "Assets/Cooked/TPS/AAA/PlasticCrate.plasset",
                              "Assets/Cooked/TPS/AAA/Airduct.plasset",
                              "Assets/Cooked/TPS/AAA/SecurityCamera02.plasset",
                              "Assets/Cooked/TPS/AAA/ModularPipes.plasset",
                              "Assets/Cooked/TPS/AAA/CircularAirduct.plasset",
                              "Assets/Cooked/TPS/AAA/ElectricCables.plasset",
                              "Assets/Cooked/TPS/AAA/ChainlinkFence.plasset",
                              "Assets/Cooked/TPS/AAA/RoadBarrier.plasset",
                              "Assets/Cooked/TPS/AAA/Jerrycan.plasset",
                              "Assets/Cooked/TPS/AAA/WallLamp.plasset",
                              "Assets/Cooked/TPS/AAA/CagedSconce.plasset",
                              "Assets/Cooked/TPS/AAA/FluorescentLights.plasset",
                              "Assets/Cooked/TPS/AAA/FireAlarm.plasset",
                              "Assets/Cooked/TPS/AAA/Toolbox.plasset",
                              "Assets/Cooked/TPS/AAA/HandTruck.plasset"
#endif
    };
    unsigned present = 0;
    for (auto path : required)
    {
        bool ok = fs::exists(path, ec);
        present += ok ? 1u : 0u;
        report << " [" << (ok ? "OK     " : "MISSING") << "] " << path << "\n";
    }
    report << " Assets  : " << present << "/" << std::size(required)
           << " required runtime assets present\n";
    if (present != std::size(required))
        report << " ACTION  : run BuildAAAAssets.bat, rebuild Sandbox, then verify staged "
                  "Assets/Cooked.\n";
    else
        report << " Status  : presence check passed; cooked decoding follows.\n";
    report << "============================================================\n\n";
    engine.Logger().Write(Severity::Info, "ASSETS", report.str());
    if (present != std::size(required))
        throw std::runtime_error("Required runtime assets missing; run BuildAAAAssets.bat and "
                                 "rebuild Sandbox. See Logs/PlagC.log.");
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
    renderer_ = &e.Graphics();
    StartupDiagnostics(e);
    config_ = config;
    e.SetFPSLocked(false);
    health_ = {};
    weapon_ = {};
    hits_ = 0;
    deadTime_ = shotTime_ = 0;
    weaponAnimation_ = {};
    camera_ = {};
    heading_ = 180;
    doorSlide_ = 0;
    doorOpen_ = false;
    fallPeak_ = 1;
    restart_ = false;
    human_ = false;
    actorScale_ = .82f;
#ifdef PC_TPS_VISUAL_MAX
    human_ = true;
    actorScale_ = 1.f;
    operative_ = e.Assets().Load<ModelAsset>("TPS/Human.plasset");
#else
    operative_ = e.Assets().Load<ModelAsset>("TPS/Operative.plasset");
#endif
    groundedLast_ = false;
    landingTime_ = hitTime_ = 0;
    crate_ = e.Assets().Load<ModelAsset>("TPS/Crate.plasset");
    generator_ = e.Assets().Load<ModelAsset>("TPS/Generator.plasset");
    world_ = std::make_unique<World>();
    PhysicsConfig p;
    p.workers = config.physicsWorkers;
    p.fixedHz = config.physicsHz;
    p.scheduler =
        config.physicsScheduler == "plagc" ? PhysicsScheduler::PlagC : PhysicsScheduler::Native;
    p.sharedJobs = &e.Jobs();
    physics_ = std::make_unique<PhysicsSystem>(*world_, p);
    scene_ = std::make_unique<SceneRenderer>(e.Graphics());
    assets_ = std::make_unique<AssetScene>(e.Graphics());
    cube_ = e.Graphics().CreateMesh(MakePrimitive(Primitive::Cube));
    for (Vec3 colour : {Vec3{.27f, .30f, .31f}, Vec3{.42f, .43f, .39f}, Vec3{.14f, .2f, .24f},
                        Vec3{.22f, .3f, .24f}, Vec3{.8f, .24f, .08f}, Vec3{.15f, .6f, .28f},
                        Vec3{.15f, .7f, .9f}})
    {
        MaterialDesc m;
        m.colour = colour;
        m.roughness = .72f;
        m.metallic = materials_.size() == 2 ? .65f : 0;
        std::vector<std::uint8_t> rgba(32 * 32 * 4), normal(rgba.size()), rough(rgba.size());
        for (unsigned y = 0; y < 32; ++y)
            for (unsigned x = 0; x < 32; ++x)
            {
                auto i = (y * 32 + x) * 4;
                unsigned v = 235 + ((x * 17 + y * 13) % 20);
                if (x == 0 || y == 0)
                    v = 205;
                rgba[i] = rgba[i + 1] = rgba[i + 2] = std::uint8_t(v);
                rgba[i + 3] = 255;
                normal[i] = 128;
                normal[i + 1] = 128;
                normal[i + 2] = 255;
                normal[i + 3] = 255;
                rough[i] = rough[i + 1] = rough[i + 2] = 180;
                rough[i + 3] = 255;
            }
        m.baseColourTexture = e.Graphics().CreateTexture(32, 32, rgba, true, true);
        textures_.push_back(m.baseColourTexture);
        m.normalTexture = e.Graphics().CreateTexture(32, 32, normal, false, true);
        textures_.push_back(m.normalTexture);
        m.roughnessTexture = e.Graphics().CreateTexture(32, 32, rough, false, true);
        textures_.push_back(m.roughnessTexture);
        if (materials_.size() == 6)
            m.emissive = {.1f, .45f, .7f};
        materials_.push_back(e.Graphics().CreateMaterial(m));
    }
#ifdef PC_TPS_VISUAL_MAX
    LoadAAAWorldAssets(e);
    InstallVisualMaxMaterials(e);
#endif
    BuildFacility(e);
#ifdef PC_TPS_VISUAL_MAX
    DecorateAAAWorld(e);
#endif
    landscape_ = std::make_unique<Landscape>(e.Graphics());
    landscape_->Build(*world_);
    BuildSettlement(e);
    physics_->Synchronize();
    character_ = physics_->CreateCharacter({{0, 1.1f, 27}});
    actor_ = world_->Create("Operative");
    world_->SetTransform(actor_,
                         {{0, .2f, 27}, {0, 180, 0}, {actorScale_, actorScale_, actorScale_}});
    world_->SetModel(actor_, {operative_});
    AnimationStateMachine graph;
    graph.parameters.push_back({"Mode", AnimationParameterType::Float, 0});
    std::vector<const char *> states{"Idle", "Walk",   "Run",  "Aim",
                                     "Fire", "Reload", "Jump", "Death"};
    if (human_)
        states.insert(states.end(),
                      {"Sprint", "Fall", "Land", "Hit", "Crouch", "CrouchWalk", "AimWalk", "AimRun",
                       "AimBack", "AimLeft", "AimRight", "AimForwardLeft", "AimForwardRight",
                       "AimBackLeft", "AimBackRight"});
    for (const char *name : states)
    {
        auto found = std::find_if(operative_->animations.begin(), operative_->animations.end(),
                                  [&](const auto &clip) { return clip.name == name; });
        if (found == operative_->animations.end())
            throw std::runtime_error(std::string("Missing TPS clip: ") + name);
        graph.states.push_back({name, unsigned(found - operative_->animations.begin()),
                                std::string(name) != "Death" && std::string(name) != "Reload" &&
                                    std::string(name) != "Hit" && std::string(name) != "Land" &&
                                    std::string(name) != "Jump" && std::string(name) != "Fire",
                                1});
    }
    for (unsigned i = 0; i < graph.states.size(); ++i)
        for (unsigned j = 0; j < graph.states.size(); ++j)
            if (i != j)
                graph.transitions.push_back(
                    {i, j, .12f, {{"Mode", AnimationCompare::Equal, float(j)}}});
    AnimatorComponent animator;
    animator.controller.emplace(graph, unsigned(operative_->animations.size()));
    AnimationUpperBody upper;
    upper.root = human_ ? "spine_01" : "spine";
    // Resolve by rig name, never by an imported joint index.
    if (std::none_of(operative_->skeleton.joints.begin(), operative_->skeleton.joints.end(),
                     [&](const auto &j) { return j.name == upper.root; }))
        throw std::runtime_error("Missing weapon animation spine root");
    upper.pose = {graph.states[3].clip, 0, 0, false, false};
    animator.upperBody = upper;
    world_->SetAnimator(actor_, animator);
    if (e.Audio().Available())
        for (unsigned kind = 0; kind < 5; ++kind)
        {
            auto clip = std::make_shared<AudioClip>();
            clip->sampleRate = 48000;
            clip->channels = 1;
            unsigned count = kind == 4 ? 48000 : kind == 0 ? 8000 : 5000;
            std::uint32_t noise = 1234567;
            for (unsigned i = 0; i < count; ++i)
            {
                noise = noise * 1664525u + 1013904223u;
                float t = float(i) / 48000;
                float wave = kind == 4 ? std::sin(2 * 3.14159265f * 60 * t) * .15f
                                       : (float(noise & 65535) / 32768 - 1) *
                                             std::exp(-t * (kind == 0 ? 38 : 60)) * .45f;
                clip->samples.push_back(std::int16_t(wave * 24000));
            }
            AudioSourceDesc d;
            d.loop = kind == 4;
            d.position = kind == 4 ? Vec3{8, -3, -54} : Vec3{};
            d.gain = kind == 4 ? .3f : .65f;
            d.referenceDistance = 3;
            sounds_.push_back(e.Audio().CreateSource(AssetHandle<AudioClip>(clip), d));
            if (kind == 4)
                e.Audio().Play(sounds_.back());
        }
    effects_ = std::make_unique<CombatEffects>(e.Graphics());
    MaterialDesc flashMaterial;
    flashMaterial.colour = {1, .48f, .08f};
    flashMaterial.emissive = {18, 7, .6f};
    materials_.push_back(e.Graphics().CreateMaterial(flashMaterial));
    flash_ = e.Graphics().CreateObject(cube_, materials_.back());
    e.Graphics().SetVisible(flash_, false);
    e.Graphics().SetCastShadows(flash_, false);
    e.Graphics().SetEnvironment({"PlagCEnvironment.dds", true, true});
    auto settings = e.Graphics().Settings();
    settings.hdr = true;
    settings.exposure = 1.05f;
#ifdef PC_TPS_VISUAL_MAX
    settings.fogDensity = .0018f;
#endif
    e.Graphics().SetSettings(settings);
    key_ = e.Events().Subscribe(EventType::KeyPressed, [this, &e](const Event &ev) {
        switch (ev.key)
        {
        case KeyCode::Escape:
            Capture(e, !captured_);
            break;
        case KeyCode::Space:
            if (captured_)
                jump_ = true;
            break;
        case KeyCode::R:
            if (captured_)
                Reload();
            break;
        case KeyCode::E:
            if (captured_)
                Interact(e);
            break;
        case KeyCode::Backspace:
            restart_ = true;
            break;
        case KeyCode::F3:
            stats_ = !stats_;
            break;
        case KeyCode::F4: {
            auto s = e.Graphics().Settings();
            s.wireframe = !s.wireframe;
            e.Graphics().SetSettings(s);
            break;
        }
        case KeyCode::F6:
            if (landscape_)
                landscape_->CycleQuality();
            break;
        case KeyCode::F7: {
            auto s = e.Graphics().Settings();
            s.bloom = !s.bloom;
            e.Graphics().SetSettings(s);
            break;
        }
        case KeyCode::F11:
            e.SetFPSLocked(!e.IsFPSLocked());
            break;
        default:
            break;
        }
    });
    focus_ = e.Events().Subscribe(EventType::Focus, [this, &e](const Event &ev) {
        if (!ev.code)
            Capture(e, false);
    });
    mouse_ = e.Events().Subscribe(EventType::MousePressed, [this, &e](const Event &ev) {
        if (ev.button == MouseCode::Left && !captured_)
            Capture(e, true);
    });
    Capture(e, true);
    if (auto w = e.MainWindow())
        w->SetTitle("OUTPOST ZERO | Third-person sandbox");
    Tick(e, 0, {});
}
void TPSGame::Sound(Engine &e, unsigned i, Vec3 p)
{
    if (i >= sounds_.size())
        return;
    AudioSourceDesc d;
    d.position = p;
    d.gain = .65f;
    d.referenceDistance = 3;
    e.Audio().UpdateSource(sounds_[i], d);
    e.Audio().Play(sounds_[i]);
}
void TPSGame::Reload()
{
    if (health_.Alive())
        weapon_.Reload();
}
void TPSGame::Interact(Engine &e)
{
    if (!health_.Alive())
        return;
    auto p = Player().position;
    if (Length(Sub(p, {0, 1, -20})) < 6)
    {
        doorOpen_ = !doorOpen_;
        Sound(e, 2, p);
    }
    if (Length(Sub(p, {-7, 1, 28})) < 4)
    {
        health_.value = 100;
        weapon_.reserve = 150;
        Sound(e, 2, p);
    }
}
void TPSGame::Shoot(Engine &e)
{
    if (!weapon_.Fire())
        return;
    shotTime_ = .10;
    Sound(e, 0, Player().position);
    auto f = camera_.look.Forward();
    auto distant = Add(view_.position, Scale(f, 150));
    if (auto hit = physics_->Raycast(view_.position, Scale(f, 150)))
        distant = hit->position;
    auto p = Player().position;
    Vec3 muzzle = Add(p, {std::sin(heading_ * Rad) * .55f, .5f, std::cos(heading_ * Rad) * .55f});
    if (auto socket =
            assets_->AnimatedNodePoint(*world_, actor_, "Rifle_blaster-e", {-.045f, 0, 1.39f}))
        muzzle = *socket;
    auto ray = Sub(distant, muzzle);
    if (Length(ray) < .001f)
        return;
    ray = Scale(ray, (Length(ray) + .05f) / Length(ray));
    effects_->Muzzle(muzzle);
    auto hit = physics_->Raycast(muzzle, ray);
    if (hit)
    {
        const bool metal = targets_.contains(physics_->EntityFor(hit->body).value);
        effects_->Impact(hit->position, hit->normal, metal);
        auto target = targets_.find(physics_->EntityFor(hit->body).value);
        if (target != targets_.end() && target->second.Alive())
        {
            target->second.Damage(25);
            ++hits_;
            Sound(e, 3, hit->position);
            if (!target->second.Alive())
            {
                Entity entity{target->first};
                auto render = world_->Read(entity).renderable.value();
                render.material = materials_[5];
                world_->SetRenderable(entity, render);
            }
        }
    }
    e.Graphics().SetTransform(flash_, {muzzle, {0, heading_, 0}, {.04f, .04f, .12f}});
}
void TPSGame::Tick(Engine &e, double dt, const Controls &input)
{
    if (!std::isfinite(dt) || dt < 0)
        throw std::invalid_argument("invalid TPS timestep");
    const double measuredFrameMs = dt * 1000;
    dt = std::min(dt, .1);
    if (effects_)
        effects_->Update(dt);
    weapon_.Tick(dt);
    shotTime_ = std::max(0., shotTime_ - dt);
    if (health_.Alive())
        camera_.look.Rotate(input.mouseX, input.mouseY);
    float yaw = camera_.look.yaw * Rad;
    Vec3 f{std::cos(yaw), 0, std::sin(yaw)}, right{-f.z, 0, f.x};
    Vec3 move = Add(Scale(f, input.forward), Scale(right, input.right));
    float length = Length(move);
    float speed = input.aim      ? (input.sprint ? 3.7f : 2.3f)
                  : input.walk   ? 1.7f
                  : input.sprint ? 7.f
                                 : 3.7f;
    if (length > 1)
        move = Scale(move, 1 / length);
    move = Scale(move, speed);
    if (!health_.Alive())
        move = {};
    physics_->SetCharacterInput(character_, move, input.jump && health_.Alive());
    auto phaseStart = std::chrono::steady_clock::now();
    physics_->Advance(dt);
    e.timings.physicsMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - phaseStart)
            .count();
    auto player = Player();
    landingTime_ = std::max(0., landingTime_ - dt);
    hitTime_ = std::max(0., hitTime_ - dt);
    if (player.grounded && !groundedLast_)
        landingTime_ = .16;
    groundedLast_ = player.grounded;
    fallPeak_ = std::max(fallPeak_, player.position.y);
    if (player.grounded)
    {
        float drop = fallPeak_ - player.position.y;
        if (drop > 5)
            Damage((drop - 5) * 15);
        fallPeak_ = player.position.y;
    }
    if (player.position.y < -15)
        Damage(100);
    if (player.position.y < -2 && player.position.x > 6 && player.position.z < -51 &&
        player.position.z > -57)
        Damage(float(dt) * 25);
    if (!health_.Alive())
    {
        deadTime_ += dt;
        if (deadTime_ > 2.8)
            restart_ = true;
    }
    const bool fire = health_.Alive() && input.fire && weapon_.cooldown <= 0 &&
                      weapon_.reload <= 0 && hitTime_ <= 0 && weapon_.rounds;
    const bool weaponBlocked = !health_.Alive() || weapon_.reload > 0 || hitTime_ > 0;
    weaponAnimation_.Update(dt, input.aim, input.fire && weapon_.rounds > 0, fire, weaponBlocked);
    float desired = heading_;
    if (health_.Alive() && weaponAnimation_.ready)
        desired = 90 - camera_.look.yaw;
    else if (Length(move) > .1f)
        desired = std::atan2(move.x, move.z) / Rad;
    heading_ += std::remainder(desired - heading_, 360.f) * (1 - std::exp(-float(dt) * 15));
    world_->SetTransform(actor_, {Add(player.position, {0, -.9f, 0}),
                                  {0, heading_, 0},
                                  {actorScale_, actorScale_, actorScale_}});
    view_ = camera_.Update(*physics_, Add(player.position, {0, .65f, 0}), input.aim, dt);
    if (fire)
        shotTime_ = .10;
    float velocity =
        std::sqrt(player.velocity.x * player.velocity.x + player.velocity.z * player.velocity.z);
    unsigned mode = !health_.Alive()     ? 7
                    : !player.grounded   ? 6
                    : weapon_.reload > 0 ? 5
                    : velocity > 5       ? 2
                    : velocity > .2f     ? 1
                                         : 0;
    if (human_ && health_.Alive())
    {
        if (!player.grounded)
            mode = player.velocity.y > .8f ? 6 : 9;
        else if (hitTime_ > 0)
            mode = 11;
        else if (weapon_.reload <= 0 && weaponAnimation_.ready && velocity > .2f)
        {
            if (input.forward > .25f)
                mode = input.right < -.25f  ? 19
                       : input.right > .25f ? 20
                       : velocity > 2.4f    ? 15
                                            : 14;
            else if (input.forward < -.25f)
                mode = input.right < -.25f ? 21 : input.right > .25f ? 22 : 16;
            else
                mode = input.right < 0 ? 17 : 18;
        }
        else if (weapon_.reload <= 0 && !weaponAnimation_.ready)
            mode = landingTime_ > 0 ? 10 : velocity > 5 ? 8 : velocity > 2.4f ? 2 : mode;
    }
    auto animator = world_->Read(actor_).animator.value();
    animator.controller->SetFloat("Mode", float(mode));
    animator.upperBody->weight = weaponAnimation_.weight;
    animator.upperBody->recoilRadians = weaponAnimation_.Recoil();
    world_->SetAnimator(actor_, animator);
    phaseStart = std::chrono::steady_clock::now();
    assets_->Update(*world_, dt);
    e.timings.animationMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - phaseStart)
            .count();
    animation_ = world_->Read(actor_).animator->controller->Current().name;
    if (fire)
        Shoot(e);
    float wanted = doorOpen_ ? 4.6f : 0;
    doorSlide_ += std::clamp(wanted - doorSlide_, -float(dt) * 3, float(dt) * 3);
    auto door = world_->ReadTransform(door_);
    door.position.x = doorSlide_;
    world_->SetTransform(door_, door);
    phaseStart = std::chrono::steady_clock::now();
    scene_->Sync(*world_, physics_->Presentation());
    e.timings.syncMs =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - phaseStart)
            .count();
    if (landscape_)
        landscape_->Update(view_, dt);
    e.Graphics().SetCamera(view_);
    e.Graphics().SetVisible(flash_, shotTime_ > 0);
    if (e.Audio().Available())
    {
        auto look = camera_.look.Forward();
        e.Audio().SetListener({view_.position, {}, look});
        footTime_ -= dt;
        if (health_.Alive() && player.grounded && velocity > .3f && footTime_ <= 0)
        {
            Sound(e, 1, player.position);
            footTime_ = velocity > 5 ? .28 : .45;
        }
    }
    hudTime_ -= dt;
    if (hudTime_ <= 0)
    {
        hudTime_ = .15;
        std::ostringstream text;
        text << "OUTPOST ZERO\nHP " << int(health_.value) << "  RIFLE " << weapon_.rounds << " / "
             << weapon_.reserve << "  HITS " << hits_ << "\n";
        text << (weapon_.reload > 0 ? "RELOADING" : animation_)
             << "\nWASD JOG  ALT WALK  SHIFT SPRINT  SPACE JUMP\nRMB AIM  LMB FIRE  R RELOAD\nE "
                "DOOR / "
                "SUPPLIES  ESC MOUSE  BACKSPACE REDEPLOY";
        if (landscape_)
            text << "\nF6 DETAIL " << landscape_->QualityName() << "  F7 BLOOM "
                 << e.Graphics().Settings().bloom;
        if (!health_.Alive())
            text << "\nDOWN - REDEPLOYING";
        if (stats_)
        {
            const auto render = e.Graphics().Statistics();
            text << "\nDRAWS " << render.drawCalls << "  INSTANCES " << render.instances
                 << "  TRIANGLES " << render.triangles << "  RENDER CPU MS "
                 << int(render.cpuMilliseconds);
            text << "\nENTITIES " << world_->Size() << "  FRAME MS " << int(measuredFrameMs)
                 << "  F11 FPS LOCK " << e.IsFPSLocked();
        }
        hud_.Update(e.Graphics(), text.str());
    }
    float aspect = 16.f / 9;
    if (auto w = e.MainWindow())
    {
        auto height = w->Height();
        if (height)
            aspect = float(w->Width()) / height;
    }
    hud_.Place(e.Graphics(), view_.position, camera_.look.yaw, camera_.look.pitch, aspect,
               view_.fov);
}
void TPSGame::Frame(Engine &e, double dt)
{
    if (restart_)
    {
        auto config = config_;
        Stop(e);
        Start(e, config);
        return;
    }
    Controls c;
    if (captured_)
    {
        auto d = Input::GetMouseDelta();
        c.mouseX = float(d.x);
        c.mouseY = float(d.y);
        c.forward = float(Input::IsKeyPressed(KeyCode::W)) - float(Input::IsKeyPressed(KeyCode::S));
        c.right = float(Input::IsKeyPressed(KeyCode::D)) - float(Input::IsKeyPressed(KeyCode::A));
        c.sprint = Input::IsKeyPressed(KeyCode::LeftShift);
        c.walk = Input::IsKeyPressed(KeyCode::LeftAlt);
        c.aim = Input::IsMouseButtonPressed(MouseCode::Right);
        c.fire = Input::IsMouseButtonPressed(MouseCode::Left);
        c.jump = jump_;
    }
    jump_ = false;
    Tick(e, dt, c);
}
void TPSGame::Stop(Engine &e)
{
    key_ = {};
    focus_ = {};
    mouse_ = {};
    Capture(e, false);
    hud_.Clear(e.Graphics());
    landscape_.reset();
    effects_.reset();
    assets_.reset();
    scene_.reset();
    physics_.reset();
    world_.reset();
    targets_.clear();
    propBounds_.clear();
    if (flash_)
        e.Graphics().DestroyObject(flash_);
    flash_ = {};
    for (auto s : sounds_)
        e.Audio().DestroySource(s);
    sounds_.clear();
    if (e.Audio().Available())
        e.Audio().CollectUnused();
    for (auto m : materials_)
        e.Graphics().DestroyMaterial(m);
    materials_.clear();
    for (auto t : textures_)
        e.Graphics().DestroyTexture(t);
    textures_.clear();
    if (fenceWire_)
        e.Graphics().DestroyMesh(fenceWire_);
    fenceWire_ = {};
    for (auto mesh : exteriorMeshes_)
        e.Graphics().DestroyMesh(mesh);
    exteriorMeshes_.clear();
    boxMeshes_.clear();
    renderer_ = nullptr;
    if (cube_)
        e.Graphics().DestroyMesh(cube_);
    cube_ = {};
    operative_ = {};
    crate_ = {};
    generator_ = {};
    for (auto *asset :
         {&securityCamera_, &securityCamera02_, &factoryFacade_,     &industrialPipes_,
          &modularPipes_,   &steelShelves_,     &exteriorAircon_,    &utilityBox_,
          &barrel_,         &plasticCrate_,     &airduct_,           &circularAirduct_,
          &electricCables_, &chainlinkFence_,   &roadBarrier_,       &jerrycan_,
          &wallLamp_,       &cagedSconce_,      &fluorescentLights_, &fireAlarm_,
          &toolbox_,        &handTruck_})
        *asset = {};
    e.Assets().CollectUnused();
}
} // namespace Game
