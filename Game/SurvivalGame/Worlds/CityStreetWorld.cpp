#include "pgcpch.h"
#include "CityStreetWorld.h"
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
constexpr PlagC::Vec3 PlayerPosition{0.0f, 0.0f, 0.0f};

PlagC::Entity ModelEntity(PlagC::World &world, std::string name,
                          PlagC::AssetHandle<PlagC::ModelAsset> asset,
                          const PlagC::Transform &transform)
{
    if (!asset)
        throw std::runtime_error("cannot create model entity from an empty asset handle: " + name);
    auto entity = world.Create(std::move(name));
    world.SetTransform(entity, transform);
    world.SetModel(entity, {std::move(asset), true});
    return entity;
}
} // namespace

void CityStreetWorld::Start(PlagC::Engine &engine)
{
    auto cityStreet = engine.Assets().Get<PlagC::ModelAsset>(
        "Game://Environment/CityStreet/CityStreet.plasset");
    auto joker = engine.Assets().Get<PlagC::ModelAsset>(
        "Game://Characters/Player/Joker/Joker.plasset");
    auto jeep = engine.Assets().Get<PlagC::ModelAsset>(
        "Game://Environment/Props/Jeep/Jeep.plasset");

    if (!cityStreet || !joker || !jeep)
        throw std::runtime_error("CityStreetWorld required assets are not ready");
    if (joker->animations.empty())
        throw std::runtime_error("Joker reference character contains no native animations");

    playerAsset_ = joker;
    requestedClip_ = 0;
    activeClip_ = ~0u;

    world_ = std::make_unique<PlagC::World>();
    assetScene_ = std::make_unique<PlagC::AssetScene>(engine.Graphics());
    sceneRenderer_ = std::make_unique<PlagC::SceneRenderer>(engine.Graphics());
#ifdef PC_ENABLE_PROFILING
    sceneRenderer_->SetProfiling(true);
#endif

    environment_ = ModelEntity(*world_, "CityStreet Environment", cityStreet,
                               {{0, 0, 0}, {0, 0, 0}, {1, 1, 1}});

    // Native rigged reference character. No UAL retargeting and no arbitrary model scale.
    // The source mesh is approximately 1.92 m tall in its authored coordinate system.
    player_ = ModelEntity(*world_, "Joker", playerAsset_,
                          {PlayerPosition, {0, 180.0f, 0}, {1, 1, 1}});
    PlagC::AnimatorComponent animator;
    animator.state = {requestedClip_, 0, 1, true, true};
    world_->SetAnimator(player_, animator);

    jeep_ = ModelEntity(*world_, "Jeep", jeep,
                        {{-4.0f, 0.60f, -3.0f}, {0, 28, 0}, {1, 1, 1}});

    sun_ = world_->Create("Sun");
    world_->SetTransform(sun_, {{0, 12, 0}, {}, {1, 1, 1}});
    PlagC::LightDesc sun;
    sun.type = PlagC::LightType::Directional;
    sun.direction = {-0.42f, -0.86f, -0.28f};
    sun.colour = {1.0f, 0.94f, 0.84f};
    sun.power = 4.2f;
    world_->SetLight(sun_, {sun});

    camera_ = world_->Create("Gameplay Camera");
    world_->SetTransform(camera_, {{0.0f, 1.45f, 4.0f}, {}, {1, 1, 1}});
    PlagC::CameraComponent camera;
    camera.target = {0.0f, 1.0f, 0.0f};
    camera.fov = 55.0f;
    camera.nearClip = 0.05f;
    camera.farClip = 350.0f;
    world_->SetCamera(camera_, camera);
    world_->SetActiveCamera(camera_);

    key_ = engine.Events().Subscribe(PlagC::EventType::KeyPressed,
                                     [this, &engine](const PlagC::Event &event) {
        auto select = [&](unsigned clip) {
            if (playerAsset_ && clip < playerAsset_->animations.size())
                requestedClip_ = clip;
        };
        if (event.key == PlagC::KeyCode::D1) select(0);
        else if (event.key == PlagC::KeyCode::D2) select(1);
        else if (event.key == PlagC::KeyCode::D3) select(2);
        else if (event.key == PlagC::KeyCode::D4) select(3);
        else if (event.key == PlagC::KeyCode::D5) select(4);
        else if (event.key == PlagC::KeyCode::D6) select(5);
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

    auto settings = engine.Graphics().Settings();
    settings.hdr = true;
    settings.bloom = true;
    settings.exposure = 1.0f;
    settings.fogDensity = 0.0015f;
    settings.fogColour = {0.40f, 0.44f, 0.46f};
    settings.background = {0.08f, 0.10f, 0.12f};
    engine.Graphics().SetSettings(settings);

    engine.SetFPSLocked(false);
    if (auto *window = engine.MainWindow())
    {
        window->SetTitle("PlagC | Survival Game | City Street | Joker Rig Test");
        window->SetCursorCaptured(true);
    }

    assetScene_->Update(*world_, 0.0);
    sceneRenderer_->Sync(*world_);
    activeClip_ = requestedClip_;

    std::string clips;
    for (unsigned i = 0; i < playerAsset_->animations.size(); ++i)
    {
        if (!clips.empty()) clips += ", ";
        clips += std::to_string(i + 1) + "=" + playerAsset_->animations[i].name;
    }
    engine.Logger().Write(PlagC::Severity::Info, "WORLD",
                          "CityStreet Joker native-rig test ready: " + clips);
}

void CityStreetWorld::Frame(PlagC::Engine &engine, double dt)
{
    if (!world_ || !assetScene_ || !sceneRenderer_ || !playerAsset_)
        throw std::logic_error("CityStreetWorld frame before Start");
    if (!std::isfinite(dt) || dt < 0)
        throw std::invalid_argument("invalid CityStreetWorld timestep");
    dt = std::min(dt, 0.1);

    if (cursorCaptured_)
    {
        const auto mouse = PlagC::Input::GetMouseDelta();
        cameraYaw_ += float(mouse.x) * 0.12f;
        cameraPitch_ = std::clamp(cameraPitch_ - float(mouse.y) * 0.10f, -25.0f, 55.0f);
    }

    if (requestedClip_ != activeClip_)
    {
        auto animator = world_->Read(player_).animator.value();
        animator.state = {requestedClip_, 0, 1, true, true};
        world_->SetAnimator(player_, animator);
        activeClip_ = requestedClip_;
        engine.Logger().Write(PlagC::Severity::Info, "ANIMATION",
                              "Joker native clip: " + playerAsset_->animations[activeClip_].name);
    }

    const float yaw = cameraYaw_ * Rad;
    const float pitch = cameraPitch_ * Rad;
    const float cp = std::cos(pitch);
    const PlagC::Vec3 target{0.0f, 1.0f, 0.0f};
    const PlagC::Vec3 direction{std::sin(yaw) * cp, std::sin(pitch), -std::cos(yaw) * cp};
    const float distance = 4.0f;
    const PlagC::Vec3 cameraPosition{target.x - direction.x * distance,
                                     target.y - direction.y * distance,
                                     target.z - direction.z * distance};
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

void CityStreetWorld::Stop(PlagC::Engine &engine)
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
    physics_.reset();
    if (world_)
        world_->Clear();
    world_.reset();

    playerAsset_ = {};
    character_ = {};
    environment_ = {};
    player_ = {};
    jeep_ = {};
    camera_ = {};
    sun_ = {};

    engine.Logger().Write(PlagC::Severity::Info, "WORLD", "CityStreetWorld stopped");
}
} // namespace PlagCGame
