#include "pgcpch.h"
#include "RigValidationWorld.h"
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
constexpr float JokerVisualScale = 100.0f;

void SetNativeClip(PlagC::World &world,
                   PlagC::Entity entity,
                   unsigned clip)
{
    PlagC::AnimatorComponent animator;
    animator.state = {clip, 0.0, 1.0f, true, true};
    world.SetAnimator(entity, animator);
}
} // namespace

void RigValidationWorld::Start(PlagC::Engine &engine)
{
    jokerAsset_ = engine.Assets().Get<PlagC::ModelAsset>(
        "Game://Characters/Player/Joker/Joker.plasset");
    if (!jokerAsset_)
        throw std::runtime_error("RigValidation Joker asset is not ready");
    if (jokerAsset_->animations.size() < 6)
        throw std::runtime_error("RigValidation requires Joker's six native animations");

    requestedMode_ = 0;
    activeMode_ = -1;

    world_ = std::make_unique<PlagC::World>();
    assetScene_ = std::make_unique<PlagC::AssetScene>(engine.Graphics());
    sceneRenderer_ = std::make_unique<PlagC::SceneRenderer>(engine.Graphics());
#ifdef PC_ENABLE_PROFILING
    sceneRenderer_->SetProfiling(true);
#endif

    joker_ = world_->Create("Joker Native Rig Validation");
    world_->SetTransform(joker_,
                         {{0, 0, 0}, {0, 180, 0},
                          {JokerVisualScale, JokerVisualScale, JokerVisualScale}});
    world_->SetModel(joker_, {jokerAsset_, true});

    // IMPORTANT: do not attach an AnimatorComponent here.
    // AssetScene therefore deforms Joker using the model's own unanimated rest
    // skeleton, which is the exact authored A-pose contained in Joker.plasset.

    sun_ = world_->Create("Validation Sun");
    world_->SetTransform(sun_, {{0, 8, 0}, {}, {1, 1, 1}});
    PlagC::LightDesc sun;
    sun.type = PlagC::LightType::Directional;
    sun.direction = {-0.35f, -0.90f, -0.25f};
    sun.colour = {1.0f, 0.96f, 0.90f};
    sun.power = 4.0f;
    world_->SetLight(sun_, {sun});

    camera_ = world_->Create("Validation Camera");
    world_->SetTransform(camera_, {{0, 1.35f, 4.2f}, {}, {1, 1, 1}});
    PlagC::CameraComponent camera;
    camera.target = {0, 1.0f, 0};
    camera.fov = 52.0f;
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
        if (event.key == PlagC::KeyCode::D0)
            requestedMode_ = 0;
        else if (event.key == PlagC::KeyCode::D1)
            requestedMode_ = 1;
        else if (event.key == PlagC::KeyCode::D2)
            requestedMode_ = 2;
        else if (event.key == PlagC::KeyCode::D3)
            requestedMode_ = 3;
        else if (event.key == PlagC::KeyCode::D4)
            requestedMode_ = 4;
        else if (event.key == PlagC::KeyCode::D5)
            requestedMode_ = 5;
        else if (event.key == PlagC::KeyCode::D6)
            requestedMode_ = 6;
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
        window->SetTitle("PlagC | Rig Validation | Joker Native Rig");
        window->SetCursorCaptured(true);
    }
    engine.SetFPSLocked(false);

    assetScene_->Update(*world_, 0.0);
    sceneRenderer_->Sync(*world_);
    activeMode_ = 0;

    std::string clips;
    for (unsigned i = 0; i < jokerAsset_->animations.size(); ++i)
    {
        if (!clips.empty())
            clips += ", ";
        clips += std::to_string(i + 1) + "=" + jokerAsset_->animations[i].name;
    }

    engine.Logger().Write(
        PlagC::Severity::Info, "RIG",
        "Joker native validation ready: 0=A-pose/rest, " + clips + ", mouse orbit");
}

void RigValidationWorld::Frame(PlagC::Engine &engine, double dt)
{
    if (!world_ || !assetScene_ || !sceneRenderer_ || !jokerAsset_)
        throw std::logic_error("RigValidation frame before Start");
    if (!std::isfinite(dt) || dt < 0)
        throw std::invalid_argument("invalid RigValidation timestep");
    dt = std::min(dt, 0.1);

    if (cursorCaptured_)
    {
        const auto delta = PlagC::Input::GetMouseDelta();
        cameraYaw_ += float(delta.x) * 0.12f;
        cameraPitch_ = std::clamp(cameraPitch_ - float(delta.y) * 0.10f,
                                  -20.0f, 45.0f);
    }

    if (requestedMode_ != activeMode_)
    {
        if (requestedMode_ == 0)
        {
            // Removing the animator makes AssetScene rebuild this instance from
            // Joker's exact authored rest/A-pose. No clip and no blending is involved.
            world_->RemoveAnimator(joker_);
            engine.Logger().Write(PlagC::Severity::Info, "RIG",
                                  "Joker mode: authored A-pose/rest skeleton");
        }
        else
        {
            const unsigned clip = unsigned(requestedMode_ - 1);
            if (clip >= jokerAsset_->animations.size())
                throw std::out_of_range("Joker native clip selection out of range");
            SetNativeClip(*world_, joker_, clip);
            engine.Logger().Write(
                PlagC::Severity::Info, "RIG",
                "Joker native clip: " + jokerAsset_->animations[clip].name);
        }
        activeMode_ = requestedMode_;
    }

    const float yaw = cameraYaw_ * Rad;
    const float pitch = cameraPitch_ * Rad;
    const float cp = std::cos(pitch);
    const PlagC::Vec3 target{0, 1.0f, 0};
    const PlagC::Vec3 direction{
        std::sin(yaw) * cp,
        std::sin(pitch),
        -std::cos(yaw) * cp};
    const float distance = 4.2f;
    const PlagC::Vec3 cameraPosition{
        target.x - direction.x * distance,
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

void RigValidationWorld::Stop(PlagC::Engine &engine)
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

    jokerAsset_ = {};
    joker_ = {};
    camera_ = {};
    sun_ = {};

    engine.Logger().Write(PlagC::Severity::Info, "RIG",
                          "Joker RigValidation stopped");
}
} // namespace PlagCGame
