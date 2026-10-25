#include "pgcpch.h"
#include "StressWorld.h"
#include <PlagC/Input/Input.h>
#include <iomanip>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#endif
using namespace PlagC;
namespace
{
using Clock = std::chrono::steady_clock;
double Ms(Clock::time_point t)
{
    return std::chrono::duration<double, std::milli>(Clock::now() - t).count();
}
bool Same(Vec3 a, Vec3 b)
{
    return a.x == b.x && a.y == b.y && a.z == b.z;
}
bool Same(const ColliderComponent &a, const ColliderComponent &b)
{
    return a.type == b.type && Same(a.dimensions, b.dimensions) && a.geometry == b.geometry &&
           Same(a.centerOfMassOffset, b.centerOfMassOffset);
}
std::uint64_t ResidentBytes()
{
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS counters{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)))
        return counters.WorkingSetSize;
#elif defined(__linux__)
    std::ifstream file("/proc/self/status");
    std::string line;
    while (std::getline(file, line))
        if (line.rfind("VmRSS:", 0) == 0)
        {
            std::istringstream input(line.substr(6));
            std::uint64_t kb = 0;
            input >> kb;
            return kb * 1024;
        }
#endif
    return 0;
}
double Quantile(std::vector<double> values, double q)
{
    if (values.empty())
        return 0;
    std::sort(values.begin(), values.end());
    return values[std::size_t(q * double(values.size() - 1))];
}
} // namespace
void StressWorld::Start(Engine &engine, const EngineConfig &config)
{
    config_ = config;
    engine.SetFPSLocked(false);
    Load(engine);
    key_ = engine.Events().Subscribe(EventType::KeyPressed,
                                     [this, &engine](const Event &event) { Key(engine, event); });
    mouse_ = engine.Events().Subscribe(EventType::MousePressed, [this](const Event &event) {
        if (!config_.stressBenchmark && captured_ && event.button == MouseCode::Left)
            model_->ThrowOrPush();
    });
    focus_ = engine.Events().Subscribe(EventType::Focus, [this, &engine](const Event &event) {
        if (!event.code)
            Capture(engine, false);
    });
    Capture(engine, !config_.stressBenchmark);
    if (config_.stressBenchmark || config_.stressSoak)
    {
        std::filesystem::path path(config_.stressCSV);
        if (path.has_parent_path())
            std::filesystem::create_directories(path.parent_path());
        csv_.open(path);
        if (!csv_)
            throw std::runtime_error("cannot open stress CSV");
        csv_ << "frame,sim_seconds,entities,target,active_target,feasible_active,activity_met,"
                "bodies,active,sleeping,static,kinematic,contacts,constraints,frame_ms,prepare_ms,"
                "physics_ms,input_ms,output_ms,presentation_ms,sync_ms,previous_render_ms,query_ms,"
                "gameplay_ms,event_ms,character_ms,ecs_writes,render_transforms,render_objects,"
                "raycasts,shape_casts,overlaps,recycled,spawned,trigger_events,rss_bytes,dropped_"
                "seconds,asset_animation_ms,asset_upload_ms\n";
    }
    std::cout << "[STRESS] One persistent FPS world. WASD mouse Space Shift; E grab, LMB "
                 "throw/push, G drop, F projectile. F1 help.\n";
}
void StressWorld::Load(Engine &engine)
{
    PhysicsConfig physics;
    physics.workers = config_.physicsWorkers;
    physics.fixedHz = config_.physicsHz;
    physics.maxBodies = 12000;
    physics.maxBodyPairs = 262144;
    physics.maxContacts = 131072;
    physics.scheduler =
        config_.physicsScheduler == "plagc" ? PhysicsScheduler::PlagC : PhysicsScheduler::Native;
    physics.sharedJobs = &engine.Jobs();
    model_ = std::make_unique<Stress::Simulation>(Stress::Settings{config_.stressEntities,
                                                                   config_.stressSeed,
                                                                   config_.stressProgressive, true},
                                                  physics);
    scene_ = std::make_unique<SceneRenderer>(engine.Graphics());
    scene_->SetProfiling(true);
    for (auto colour : {Vec3{.22f, .26f, .30f}, Vec3{.08f, .12f, .17f}, Vec3{.14f, .35f, .48f},
                        Vec3{.62f, .43f, .12f}, Vec3{.53f, .2f, .18f}, Vec3{.28f, .25f, .45f},
                        Vec3{.12f, .4f, .35f}, Vec3{.28f, .38f, .18f}, Vec3{.45f, .50f, .55f},
                        Vec3{.9f, .42f, .12f}, Vec3{.15f, .65f, .85f}, Vec3{.88f, .8f, .3f},
                        Vec3{.16f, .25f, .4f}})
    {
        MaterialDesc d;
        d.colour = colour;
        d.metallic = materials_.size() == 8 ? .65f : 0;
        d.roughness = .6f;
        materials_.push_back(engine.Graphics().CreateMaterial(d));
    }
    engine.Graphics().SetEnvironment({"EnvironmentTest.dds", true, true});
    BindVisuals(engine.Graphics());
    scene_->Sync(model_->world, model_->physics.Presentation());
    previousRays_ = previousCasts_ = previousOverlaps_ = selected_ = 0;
    debugClock_ = 0;
    assetScene_ = std::make_unique<AssetScene>(engine.Graphics());
    assetErrorReported_ = false;
    if (config_.assetDemo)
    {
        engine.Assets().Request("Beacon.plasset", AssetType::Model);
        engine.Assets().Request("Chime.plasset", AssetType::Audio);
    }
    hudClock_ = 1;
    revision_ = model_->visualRevision;
    waypoint_ = 0;
    if (auto *window = engine.MainWindow())
        window->SetTitle("PlagC | Integrated FPS Stress World | F1 controls");
}
void StressWorld::Unload(Engine &engine)
{
    if (chime_)
    {
        engine.Audio().DestroySource(chime_);
        chime_ = {};
        engine.Audio().CollectUnused();
    }
    assetScene_.reset();
    assetActors_.clear();
    beacon_ = {};
    chimeClip_ = {};
    hud_.Clear(engine.Graphics());
    scene_.reset();
    model_.reset();
    for (auto h : materials_)
        engine.Graphics().DestroyMaterial(h);
    for (auto &mesh : meshes_)
        engine.Graphics().DestroyMesh(mesh.handle);
    materials_.clear();
    meshes_.clear();
    bound_.clear();
}
void StressWorld::BindVisuals(Renderer &renderer)
{
    for (const auto &visual : model_->visuals)
    {
        MeshHandle mesh{};
        for (const auto &cached : meshes_)
            if (Same(cached.shape, visual.shape))
            {
                mesh = cached.handle;
                break;
            }
        if (!mesh)
        {
            mesh = renderer.CreateMesh(model_->physics.BuildColliderMesh(visual.shape));
            meshes_.push_back({visual.shape, mesh});
        }
        auto colour = visual.colour;
        if (debug_ && visual.dynamic)
            colour = model_->physics.Read(visual.entity).active ? 9 : 12;
        RenderableComponent value{mesh, materials_.at(colour), !visual.sensor || debug_,
                                  !visual.sensor, 10};
        auto found = bound_.find(visual.entity.value);
        if (found == bound_.end() || found->second.mesh.value != mesh.value ||
            found->second.material.value != value.material.value ||
            found->second.visible != value.visible)
        {
            model_->world.SetRenderable(visual.entity, value);
            bound_[visual.entity.value] = value;
        }
    }
    std::erase_if(bound_,
                  [this](const auto &entry) { return !model_->world.Alive({entry.first}); });
    revision_ = model_->visualRevision;
}
void StressWorld::Capture(Engine &engine, bool capture)
{
    captured_ = capture;
    if (auto *window = engine.MainWindow())
        window->SetCursorCaptured(capture);
}
void StressWorld::AssetMessage(std::string message)
{
    assetStatus_ = std::move(message);
    hudClock_ = 1;
    std::cout << "[ASSET] " << assetStatus_ << std::endl;
}
void StressWorld::Key(Engine &engine, const Event &event)
{
    if (event.key == KeyCode::Escape)
    {
        engine.Stop();
        return;
    }
    if (config_.stressBenchmark)
        return;
    switch (event.key)
    {
    case KeyCode::Tab:
        Capture(engine, !captured_);
        break;
    case KeyCode::F1:
        help_ = !help_;
        hudClock_ = 1;
        break;
    case KeyCode::F2:
        debug_ = !debug_;
        BindVisuals(engine.Graphics());
        break;
    case KeyCode::F3:
        performance_ = !performance_;
        hudClock_ = 1;
        break;
    case KeyCode::F4: {
        auto settings = engine.Graphics().Settings();
        settings.wireframe = !settings.wireframe;
        engine.Graphics().SetSettings(settings);
        break;
    }
    case KeyCode::F5:
        if (!config_.assetDemo || !beacon_ || beacon_->animations.empty())
        {
            AssetMessage("F5 NO ANIMATED MODEL READY - CHECK ASSET STATUS");
            break;
        }
        animationPlaying_ = !animationPlaying_;
        for (auto e : assetActors_)
        {
            auto record = model_->world.Read(e);
            if (record.animator)
            {
                record.animator->state.playing = animationPlaying_;
                model_->world.SetAnimator(e, *record.animator);
            }
        }
        AssetMessage(animationPlaying_ ? "F5 BEACON ANIMATION PLAYING" : "F5 BEACON ANIMATION PAUSED");
        break;
    case KeyCode::F6:
        if (chime_)
        {
            if (engine.Audio().State(chime_) == AudioPlayback::Playing)
            {
                engine.Audio().Pause(chime_);
                AssetMessage("F6 CHIME PAUSED");
            }
            else
            {
                engine.Audio().Play(chime_);
                AssetMessage("F6 CHIME PLAYING - RIGHT BEACON NEAR SPAWN");
            }
        }
        else if (!engine.Audio().Available())
        {
            AssetMessage("F6 AUDIO UNAVAILABLE - SEE CONSOLE");
            std::cout << "[AUDIO] " << engine.Audio().Error() << std::endl;
        }
        else
            AssetMessage("F6 CHIME NOT READY - CHECK ASSET STATUS");
        break;
    case KeyCode::F7:
        if (config_.assetDemo)
        {
            engine.Assets().Reload("Beacon.plasset");
            engine.Assets().Reload("Chime.plasset");
            assetErrorReported_ = false;
            AssetMessage("F7 RELOADING COOKED ASSETS");
        }
        else
            AssetMessage("F7 ASSET DEMO DISABLED");
        break;
    case KeyCode::D1:
    case KeyCode::D2:
    case KeyCode::D3:
    case KeyCode::D4: {
        const unsigned sizes[]{1000, 2500, 5000, 10000};
        model_->SetProgressive(false);
        model_->SetTarget(sizes[unsigned(event.key) - unsigned(KeyCode::D1)]);
        break;
    }
    case KeyCode::P:
        paused_ = !paused_;
        break;
    case KeyCode::N:
        paused_ = true;
        step_ = true;
        break;
    case KeyCode::B:
        model_->SetTarget(model_->Target() + 100);
        break;
    case KeyCode::V:
        model_->SetTarget(model_->Target() + 500);
        break;
    case KeyCode::T:
        model_->SetProgressive(!model_->Progressive());
        break;
    case KeyCode::Space:
        if (captured_)
            jump_ = true;
        break;
    case KeyCode::E:
        if (captured_)
            model_->Grab();
        break;
    case KeyCode::G:
        model_->Drop();
        break;
    case KeyCode::F:
        if (captured_)
            model_->Fire();
        break;
    case KeyCode::R:
        if (event.modifiers & 1)
        {
            Unload(engine);
            Load(engine);
            paused_ = false;
            yaw_ = -90;
            pitch_ = 0;
        }
        else
            model_->ResetPlayer();
        break;
    default:
        break;
    }
}
void StressWorld::Input(Engine &, double)
{
    constexpr float rad = 3.14159265358979323846f / 180;
    Vec3 velocity{};
    if (config_.stressBenchmark)
    {
        constexpr Vec3 waypoints[] = {{0, 0, -120}, {75, 0, -120}, {75, 0, 60},
                                      {0, 0, 60},   {-75, 0, 60},  {-75, 0, -120}};
        auto player = model_->Player().position;
        auto target = waypoints[waypoint_ % 6];
        float dx = target.x - player.x, dz = target.z - player.z,
              distance = std::sqrt(dx * dx + dz * dz);
        if (distance < 3)
            ++waypoint_;
        if (distance > .01f)
        {
            velocity = {dx * 5 / distance, 0, dz * 5 / distance};
            yaw_ = std::atan2(dz, dx) / rad;
        }
        pitch_ = -8;
        if (frame_ % 180 == 30)
            model_->Grab();
        if (frame_ % 180 == 90)
            model_->ThrowOrPush();
        if (frame_ % 120 == 60)
            model_->Fire();
    }
    else if (captured_)
    {
        auto delta = PlagC::Input::GetMouseDelta();
        yaw_ += float(delta.x) * .12f;
        pitch_ = std::clamp(pitch_ - float(delta.y) * .12f, -85.f, 85.f);
        Vec3 forward{std::cos(yaw_ * rad), 0, std::sin(yaw_ * rad)},
            right{-forward.z, 0, forward.x};
        auto add = [&](Vec3 v, float sign) {
            velocity.x += v.x * sign;
            velocity.z += v.z * sign;
        };
        if (PlagC::Input::IsKeyPressed(KeyCode::W))
            add(forward, 1);
        if (PlagC::Input::IsKeyPressed(KeyCode::S))
            add(forward, -1);
        if (PlagC::Input::IsKeyPressed(KeyCode::D))
            add(right, 1);
        if (PlagC::Input::IsKeyPressed(KeyCode::A))
            add(right, -1);
        float length = std::sqrt(velocity.x * velocity.x + velocity.z * velocity.z);
        if (length > 0)
        {
            float speed = PlagC::Input::IsKeyPressed(KeyCode::LeftShift) ? 9.f : 4.5f;
            velocity.x *= speed / length;
            velocity.z *= speed / length;
        }
    }
    Vec3 look{std::cos(yaw_ * rad) * std::cos(pitch_ * rad), std::sin(pitch_ * rad),
              std::sin(yaw_ * rad) * std::cos(pitch_ * rad)};
    model_->SetInput(velocity, jump_, look);
    jump_ = false;
}
void StressWorld::Hud(Engine &engine, double dt)
{
    auto s = model_->physics.Statistics();
    auto p = s.physics;
    auto sync = scene_->Statistics();
    auto render = engine.Graphics().Statistics();
    auto &m = model_->metrics;
    std::ostringstream text;
    text << std::fixed << std::setprecision(2) << "PLAGC FPS STRESS WORLD - "
         << Stress::DistrictNames[model_->District()] << "\n";
    text << (paused_ ? "PAUSED " : "RUNNING ")
         << (captured_ ? "MOUSE CAPTURED" : "TAB TO CAPTURE MOUSE") << "  TARGET " << selected_
         << "  HELD " << model_->Held().value << "\n";
    text << assetStatus_ << "\n";
    if (performance_)
    {
        text << "FPS " << (dt > 0 ? 1 / dt : 0) << " FRAME " << dt * 1000 << " MS\nENTITIES "
             << model_->world.Size() << " TARGET " << m.target << "\nBODIES " << p.bodies
             << " ACTIVE " << p.active << " SLEEP " << p.sleeping << "\nSTATIC " << p.statics
             << " KIN " << p.kinematics << " CONTACTS " << p.contacts << " JOINTS " << p.constraints
             << "\n";
        text << "PHYSICS " << s.stepMs << " IN " << s.inputMs << " OUT " << s.outputMs
             << " MS\nGAMEPLAY " << m.gameplayMs << " CHARACTER " << s.charactersMs << " EVENTS "
             << s.eventsMs << "\nSYNC " << sync.totalMs << " RENDER CPU " << render.cpuMilliseconds
             << " QUERY " << queryMs_ << "\nECS WRITES " << s.ecsWrites << " RENDER UPDATES "
             << sync.transforms << " OBJECTS " << render.objects << "\n";
        text << "RAYS " << p.raycasts - previousRays_ << " CASTS " << p.shapeCasts - previousCasts_
             << " OVERLAPS " << p.overlaps - previousOverlaps_ << " PER FRAME\nRSS MB "
             << double(ResidentBytes()) / (1024 * 1024) << " PHYSICS MEMORY N/A\n";
        text << "STAGE " << m.stage << " NEXT " << (model_->Progressive() ? m.nextIncrease : 0)
             << " S  AUTO " << model_->Progressive() << "\nACTIVE TARGET " << m.requestedActive
             << " FEASIBLE " << m.feasibleActive << " MET " << m.activityMet << "\nRECYCLED "
             << m.recycled << " TRIGGERS " << m.triggerEvents << " DROP " << s.droppedSeconds
             << " S\n";
    }
    if (performance_ && assetScene_)
    {
        auto a = assetScene_->Statistics();
        auto audio = engine.Audio().Statistics();
        text << "ASSET MODELS " << a.models << " INSTANCES " << a.instances << " ANIM "
             << a.animationMs << " UPLOAD " << a.uploadMs << " MS\nAUDIO " << audio.available
             << " SOURCES " << audio.sources << " BUFFERS " << audio.buffers << "\n";
    }
    if (help_)
        text << "WASD MOVE  MOUSE LOOK  SPACE JUMP  SHIFT SPRINT\nE GRAB  LMB THROW/PUSH  G DROP  "
                "F PROJECTILE\n1/2/3/4 LOAD 1000/2500/5000/10000  T AUTO\nB ADD 100  V ADD 500  P "
                "PAUSE  N STEP\nR RESET PLAYER  SHIFT R RESET WORLD\nF1 HELP  F2 PHYSICS "
                "COLOURS/ZONES  F3 PERF\nF4 WIREFRAME  TAB MOUSE  ESC EXIT\nF5 ANIMATION  F6 CHIME "
                " F7 RELOAD ASSETS\n";
    text << "AIM AT DYNAMIC CARGO TO GRAB";
    hud_.Update(engine.Graphics(), text.str());
    if (auto *window = engine.MainWindow())
        window->SetTitle("PlagC FPS Stress World | " +
                         std::string(Stress::DistrictNames[model_->District()]) + " | Entities " +
                         std::to_string(model_->world.Size()) + " | FPS " +
                         std::to_string(int(dt > 0 ? 1 / dt : 0)));
}
void StressWorld::SampleFrame(Engine &engine, double dt)
{
    auto stats = model_->physics.Statistics();
    const auto &p = stats.physics;
    auto sync = scene_->Statistics();
    auto render = engine.Graphics().Statistics();
    const auto &m = model_->metrics;
    if ((config_.stressBenchmark || config_.stressSoak) && frame_ >= config_.stressWarmup)
    {
        auto assets = assetScene_ ? assetScene_->Statistics() : AssetSceneStats{};
        Sample sample{
            dt * 1000,          prepareMs_,           stats.stepMs,   stats.inputMs,
            stats.outputMs,     stats.presentationMs, sync.totalMs,   render.cpuMilliseconds,
            queryMs_,           m.gameplayMs,         stats.eventsMs, stats.charactersMs,
            assets.animationMs, assets.uploadMs};
        ++sampleCount_;
        captureElapsed_ += dt;
        double Sample::*fields[] = {
            &Sample::frame,          &Sample::prepare,      &Sample::physics, &Sample::input,
            &Sample::output,         &Sample::presentation, &Sample::sync,    &Sample::render,
            &Sample::queries,        &Sample::gameplay,     &Sample::events,  &Sample::characters,
            &Sample::assetAnimation, &Sample::assetUpload};
        for (auto field : fields)
            peaks_.*field = std::max(peaks_.*field, sample.*field);
        // Uniform bounded reservoir: every frame is written to CSV; long-run quantiles sample up to
        // 65536 frames.
        if (samples_.size() < 65536)
            samples_.push_back(sample);
        else
        {
            auto index =
                std::uniform_int_distribution<std::uint64_t>(0, sampleCount_ - 1)(sampling_);
            if (index < samples_.size())
                samples_[index] = sample;
        }
        csv_ << frame_ << ',' << m.simulationSeconds << ',' << model_->world.Size() << ','
             << m.target << ',' << m.requestedActive << ',' << m.feasibleActive << ','
             << m.activityMet << ',' << p.bodies << ',' << p.active << ',' << p.sleeping << ','
             << p.statics << ',' << p.kinematics << ',' << p.contacts << ',' << p.constraints << ','
             << sample.frame << ',' << sample.prepare << ',' << sample.physics << ','
             << sample.input << ',' << sample.output << ',' << sample.presentation << ','
             << sample.sync << ',' << sample.render << ',' << sample.queries << ','
             << sample.gameplay << ',' << sample.events << ',' << sample.characters << ','
             << stats.ecsWrites << ',' << sync.transforms << ',' << render.objects << ','
             << p.raycasts - previousRays_ << ',' << p.shapeCasts - previousCasts_ << ','
             << p.overlaps - previousOverlaps_ << ',' << m.recycled << ',' << m.spawned << ','
             << m.triggerEvents << ',' << ResidentBytes() << ',' << stats.droppedSeconds << ','
             << sample.assetAnimation << ',' << sample.assetUpload << '\n';
        if (config_.stressSoak
                ? captureElapsed_ >= config_.stressSeconds
                : sampleCount_ >= std::size_t(config_.stressSeconds * config_.physicsHz))
        {
            FinishCapture();
            engine.Stop();
        }
    }
    previousRays_ = p.raycasts;
    previousCasts_ = p.shapeCasts;
    previousOverlaps_ = p.overlaps;
    ++frame_;
}
void StressWorld::FinishCapture()
{
    if (!csv_.is_open())
        return;
    csv_.close();
    if (!csv_)
        throw std::runtime_error("stress CSV write failed");
    std::ofstream summary(config_.stressCSV + ".summary.csv");
    summary << "metric,median,p95,p99,peak,samples,quantile_samples\n";
    const std::pair<const char *, double Sample::*> fields[] = {
        {"frame_ms", &Sample::frame},
        {"prepare_ms", &Sample::prepare},
        {"physics_ms", &Sample::physics},
        {"input_ms", &Sample::input},
        {"output_ms", &Sample::output},
        {"presentation_ms", &Sample::presentation},
        {"sync_ms", &Sample::sync},
        {"previous_render_ms", &Sample::render},
        {"query_ms", &Sample::queries},
        {"gameplay_ms", &Sample::gameplay},
        {"event_ms", &Sample::events},
        {"character_ms", &Sample::characters},
        {"asset_animation_ms", &Sample::assetAnimation},
        {"asset_upload_ms", &Sample::assetUpload}};
    for (auto [name, field] : fields)
    {
        std::vector<double> values;
        for (auto &s : samples_)
            values.push_back(s.*field);
        summary << name << ',' << Quantile(values, .5) << ',' << Quantile(values, .95) << ','
                << Quantile(values, .99) << ',' << peaks_.*field << ',' << sampleCount_ << ','
                << values.size() << '\n';
    }
    if (!summary)
        throw std::runtime_error("stress summary write failed");
    std::cout << "[STRESS BENCHMARK] " << sampleCount_ << " samples: " << config_.stressCSV
              << std::endl;
}
void StressWorld::Frame(Engine &engine, double dt)
{
    auto start = Clock::now();
    Input(engine, dt);
    double advance = config_.stressBenchmark ? 1 / config_.physicsHz
                     : step_                 ? 1 / config_.physicsHz
                     : paused_               ? 0
                                             : dt;
    step_ = false;
    model_->Advance(advance);
    UpdateAssets(engine, advance);
    double existingQueries = model_->metrics.queryMs;
    auto queryStart = Clock::now();
    auto hit = model_->Aim(6);
    selected_ = hit ? model_->physics.EntityFor(hit->body).value : 0;
    // A small recurring sphere sweep exercises the query path independently of the solver.
    if (frame_ % 4 == 0)
    {
        ColliderComponent sphere;
        sphere.type = ColliderType::Sphere;
        sphere.dimensions = {.2f, .2f, .2f};
        model_->physics.ShapeCast(sphere, {model_->Eye(), {}, {1, 1, 1}}, {0, -3, 0});
    }
    queryMs_ = Ms(queryStart) + existingQueries;
    debugClock_ += dt;
    if (revision_ != model_->visualRevision || (debug_ && debugClock_ >= .5))
    {
        BindVisuals(engine.Graphics());
        debugClock_ = 0;
    }
    scene_->Sync(model_->world, model_->physics.Presentation());
    prepareMs_ = Ms(start);
    hudClock_ += dt;
    if (!config_.stressBenchmark && hudClock_ >= .25)
    {
        Hud(engine, dt);
        hudClock_ = 0;
    }
    if (!config_.stressBenchmark)
    {
        float aspect = 16.f / 9;
        if (auto *window = engine.MainWindow(); window && window->Height() > 0)
            aspect = float(window->Width()) / window->Height();
        hud_.Place(engine.Graphics(), model_->world.ReadTransform(model_->camera).position, yaw_,
                   pitch_, aspect);
    }
    SampleFrame(engine, dt);
}
void StressWorld::Stop(Engine &engine)
{
    FinishCapture();
    key_.Reset();
    mouse_.Reset();
    focus_.Reset();
    Capture(engine, false);
    Unload(engine);
}
void StressWorld::UpdateAssets(Engine &engine, double dt)
{
    if (!config_.assetDemo)
        return;
    engine.Assets().Pump();
    auto model = engine.Assets().Get<ModelAsset>("Beacon.plasset");
    auto clip = engine.Assets().Get<AudioClip>("Chime.plasset");
    if (model && model.Shared() != beacon_.Shared())
    {
        if (assetActors_.empty())
        {
            for (unsigned i = 0; i < 2; ++i)
            {
                auto e = model_->world.Create(i ? "Animated asset beacon" : "Static asset beacon");
                model_->world.SetTransform(e, {{i ? 4.f : -4.f, 0, 52}, {}, {1, 1, 1}});
                assetActors_.push_back(e);
            }
            model_->SetTarget(model_->Target());
        }
        for (auto e : assetActors_)
            model_->world.SetModel(e, {model, true});
        if (!model->animations.empty())
            model_->world.SetAnimator(assetActors_.back(), {{0, 0, 1, animationPlaying_, true}});
        else
            model_->world.RemoveAnimator(assetActors_.back());
        beacon_ = model;
        AssetMessage("BEACONS READY - R TO SPAWN - LOOK LEFT AND RIGHT");
    }
    if (clip && clip.Shared() != chimeClip_.Shared() && engine.Audio().Available())
    {
        AudioSourceDesc desc;
        desc.position = {4, 1.5f, 52};
        desc.loop = true;
        desc.gain = .12f;
        desc.referenceDistance = 3;
        auto replacement = engine.Audio().CreateSource(clip, desc);
        engine.Audio().Play(replacement);
        if (chime_)
            engine.Audio().DestroySource(chime_);
        chime_ = replacement;
        chimeClip_ = clip;
        engine.Audio().CollectUnused();
    }
    if (!assetErrorReported_)
        for (auto &info : engine.Assets().Registry())
            if (info.state == AssetState::Failed)
            {
                std::cerr << "[ASSET] " << info.metadata.path << ": " << info.error
                          << ". Run AssetCooker --demo to restore fixtures.\n";
                AssetMessage("ASSET LOAD FAILED - " + info.metadata.path + " - SEE CONSOLE");
                assetErrorReported_ = true;
            }
    assetScene_->Update(model_->world, dt);
    if (engine.Audio().Available())
    {
        constexpr float rad = 3.14159265358979323846f / 180;
        AudioListener listener;
        listener.position = model_->Eye();
        listener.forward = {std::cos(yaw_ * rad) * std::cos(pitch_ * rad), std::sin(pitch_ * rad),
                            std::sin(yaw_ * rad) * std::cos(pitch_ * rad)};
        engine.Audio().SetListener(listener);
    }
}
