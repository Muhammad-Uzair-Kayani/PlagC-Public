#include "pgcpch.h"
#include "JoelRigValidationWorld.h"
#include <PlagC/Animation/AnimationRetarget.h>
#include <PlagC/Assets/AssetManager.h>
#include <PlagC/Input/Input.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace PlagCGame
{
namespace
{
constexpr float Rad = 3.14159265358979323846f / 180.0f;

unsigned Clip(const PlagC::ModelAsset &asset, std::string_view name)
{
    auto it = std::find_if(asset.animations.begin(), asset.animations.end(),
                           [&](const auto &clip) { return clip.name == name; });
    if (it == asset.animations.end())
        throw std::runtime_error("JoelRigValidation missing clip: " + std::string(name));
    return unsigned(it - asset.animations.begin());
}
} // namespace

void JoelRigValidationWorld::Start(PlagC::Engine &engine)
{
    auto joel = engine.Assets().Get<PlagC::ModelAsset>(
        "Game://Characters/Validation/JoelRigV2/JoelRigV2.plasset");
    auto ual = engine.Assets().Get<PlagC::ModelAsset>(
        "Game://Animations/UAL/UAL1_Standard.plasset");
    if (!joel || !ual)
        throw std::runtime_error("JoelRigValidation assets are not ready");

    // The replacement rig is already fitted to the UAL hierarchy. Preserve Joel's
    // fitted bind translations and apply UAL local rotation deltas only.
    PlagC::AnimationRetargetConfig config;
    config.rootTranslation = false;
    config.pelvisTranslation = false;
    config.translationScale = 1.0f;
    playerAsset_ = PlagC::RetargetAnimations(joel, ual, config);

    requestedClip_ = Clip(*playerAsset_, "Idle_Loop");

    world_ = std::make_unique<PlagC::World>();
    assetScene_ = std::make_unique<PlagC::AssetScene>(engine.Graphics());
    sceneRenderer_ = std::make_unique<PlagC::SceneRenderer>(engine.Graphics());
#ifdef PC_ENABLE_PROFILING
    sceneRenderer_->SetProfiling(true);
#endif

    player_ = world_->Create("Joel Rig V2");
    world_->SetTransform(player_, {{0, 0, 0}, {0, 180, 0}, {1, 1, 1}});
    world_->SetModel(player_, {playerAsset_, true});
    PlagC::AnimatorComponent animator;
    animator.state = {requestedClip_, 0, 1, true, true};
    world_->SetAnimator(player_, animator);

    sun_ = world_->Create("Joel Validation Sun");
    world_->SetTransform(sun_, {{0, 8, 0}, {}, {1, 1, 1}});
    PlagC::LightDesc sun;
    sun.type = PlagC::LightType::Directional;
    sun.direction = {-0.35f, -0.90f, -0.25f};
    sun.colour = {1.0f, 0.96f, 0.90f};
    sun.power = 4.0f;
    world_->SetLight(sun_, {sun});

    camera_ = world_->Create("Joel Validation Camera");
    world_->SetTransform(camera_, {{0, 1.20f, 4.0f}, {}, {1, 1, 1}});
    PlagC::CameraComponent camera;
    camera.target = {0, 0.95f, 0};
    camera.fov = 50.0f;
    camera.nearClip = 0.05f;
    camera.farClip = 100.0f;
    world_->SetCamera(camera_, camera);
    world_->SetActiveCamera(camera_);

    auto settings = engine.Graphics().Settings();
    settings.hdr = true;
    settings.bloom = false;
    settings.exposure = 1.0f;
    settings.fogDensity = 0.0f;
    settings.background = {0.06f, 0.07f, 0.09f};
    engine.Graphics().SetSettings(settings);

    key_ = engine.Events().Subscribe(PlagC::EventType::KeyPressed,
                                     [this, &engine](const PlagC::Event &event) {
        if (event.key == PlagC::KeyCode::D1)
            requestedClip_ = Clip(*playerAsset_, "A_TPose");
        else if (event.key == PlagC::KeyCode::D2)
            requestedClip_ = Clip(*playerAsset_, "Idle_Loop");
        else if (event.key == PlagC::KeyCode::D3)
            requestedClip_ = Clip(*playerAsset_, "Walk_Loop");
        else if (event.key == PlagC::KeyCode::D4)
            requestedClip_ = Clip(*playerAsset_, "Jog_Fwd_Loop");
        else if (event.key == PlagC::KeyCode::D5)
            requestedClip_ = Clip(*playerAsset_, "Crouch_Idle_Loop");
        else if (event.key == PlagC::KeyCode::D6)
            requestedClip_ = Clip(*playerAsset_, "Crouch_Fwd_Loop");
        else if (event.key == PlagC::KeyCode::D7)
            requestedClip_ = Clip(*playerAsset_, "Pistol_Aim_Neutral");
        else if (event.key == PlagC::KeyCode::D8)
            requestedClip_ = Clip(*playerAsset_, "Pistol_Reload");
        else if (event.key == PlagC::KeyCode::Escape)
        {
            cursorCaptured_ = !cursorCaptured_;
            if (auto *window = engine.MainWindow())
                window->SetCursorCaptured(cursorCaptured_);
        }
    });
    focus_ = engine.Events().Subscribe(PlagC::EventType::Focus,
                                       [this, &engine](const PlagC::Event &event) {
        if (!event.code)
        {
            cursorCaptured_ = false;
            if (auto *window = engine.MainWindow())
                window->SetCursorCaptured(false);
        }
    });
    mouse_ = engine.Events().Subscribe(PlagC::EventType::MousePressed,
                                       [this, &engine](const PlagC::Event &event) {
        if (event.button == PlagC::MouseCode::Left && !cursorCaptured_)
        {
            cursorCaptured_ = true;
            if (auto *window = engine.MainWindow())
                window->SetCursorCaptured(true);
        }
    });

    if (auto *window = engine.MainWindow())
    {
        window->SetTitle("PlagC | Joel Rig V2 Validation");
        window->SetCursorCaptured(true);
    }
    engine.SetFPSLocked(false);

    assetScene_->Update(*world_, 0.0);
    sceneRenderer_->Sync(*world_);
    activeClip_ = requestedClip_;

    engine.Logger().Write(
        PlagC::Severity::Info, "RIG",
        "Joel Rig V2 ready: 1 T-pose, 2 idle, 3 walk, 4 jog, 5 crouch idle, 6 crouch walk, 7 pistol aim, 8 reload");
}

void JoelRigValidationWorld::Frame(PlagC::Engine &engine, double dt)
{
    if (!world_ || !assetScene_ || !sceneRenderer_ || !playerAsset_)
        throw std::logic_error("JoelRigValidation frame before Start");
    if (!std::isfinite(dt) || dt < 0)
        throw std::invalid_argument("invalid JoelRigValidation timestep");
    dt = std::min(dt, 0.1);

    if (cursorCaptured_)
    {
        const auto delta = PlagC::Input::GetMouseDelta();
        cameraYaw_ += float(delta.x) * 0.12f;
        cameraPitch_ = std::clamp(cameraPitch_ - float(delta.y) * 0.10f, -20.0f, 45.0f);
    }

    if (requestedClip_ != activeClip_)
    {
        auto animator = world_->Read(player_).animator.value();
        animator.state = {requestedClip_, 0, 1, true, true};
        world_->SetAnimator(player_, animator);
        activeClip_ = requestedClip_;
        engine.Logger().Write(PlagC::Severity::Info, "RIG",
                              "Playing Joel Rig V2 clip: " + playerAsset_->animations[activeClip_].name);
    }

    const float yaw = cameraYaw_ * Rad;
    const float pitch = cameraPitch_ * Rad;
    const float cp = std::cos(pitch);
    const PlagC::Vec3 target{0, 0.95f, 0};
    const PlagC::Vec3 direction{std::sin(yaw) * cp, std::sin(pitch), -std::cos(yaw) * cp};
    const PlagC::Vec3 cameraPosition{target.x - direction.x * 4.0f,
                                     target.y - direction.y * 4.0f,
                                     target.z - direction.z * 4.0f};
    world_->SetTransform(camera_, {cameraPosition, {}, {1, 1, 1}});
    auto camera = world_->Read(camera_).camera.value();
    camera.target = target;
    world_->SetCamera(camera_, camera);

    assetScene_->Update(*world_, dt);
    sceneRenderer_->Sync(*world_);

    const auto animation = assetScene_->Statistics();
    engine.timings.animationMs = animation.animationMs;
    const auto sync = sceneRenderer_->Statistics();
    engine.timings.syncMs = sync.totalMs;
    engine.timings.syncStructureMs = sync.structureMs;
    engine.timings.syncResourceMs = sync.resourceMs;
    engine.timings.syncTransformMs = sync.transformMs;
    engine.timings.syncPropertyMs = sync.propertyMs;
    engine.timings.syncLightCameraMs = sync.lightCameraMs;
    engine.timings.syncTraversalMs = sync.traversalMs;
}

void JoelRigValidationWorld::Stop(PlagC::Engine &engine)
{
    key_.Reset();
    focus_.Reset();
    mouse_.Reset();
    if (auto *window = engine.MainWindow())
        window->SetCursorCaptured(false);

    if (sceneRenderer_)
        sceneRenderer_->Clear();
    if (assetScene_)
        assetScene_->Clear();
    sceneRenderer_.reset();
    assetScene_.reset();
    if (world_)
        world_->Clear();
    world_.reset();
    playerAsset_ = {};
    player_ = {};
    camera_ = {};
    sun_ = {};

    engine.Logger().Write(PlagC::Severity::Info, "RIG", "JoelRigValidation stopped");
}
} // namespace PlagCGame
