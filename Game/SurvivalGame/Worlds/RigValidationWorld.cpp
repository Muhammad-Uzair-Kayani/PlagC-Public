#include "pgcpch.h"
#include "RigValidationWorld.h"
#include <PlagC/Animation/AnimationRetarget.h>
#include <PlagC/Assets/AssetManager.h>
#include <PlagC/Input/Input.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_map>

namespace PlagCGame
{
namespace
{
constexpr float Rad = 3.14159265358979323846f / 180.0f;
constexpr float JokerVisualScale = 100.0f;
constexpr unsigned UalPageSize = 12;

void SetClip(PlagC::World &world, PlagC::Entity entity, unsigned clip)
{
    PlagC::AnimatorComponent animator;
    animator.state = {clip, 0.0, 1.0f, true, true};
    world.SetAnimator(entity, animator);
}

std::string ThreeDigits(unsigned value)
{
    if (value < 10)
        return "00" + std::to_string(value);
    if (value < 100)
        return "0" + std::to_string(value);
    return std::to_string(value);
}

std::unordered_map<std::string, std::string> JokerToUAL()
{
    std::unordered_map<std::string, std::string> map = {
        {"Bip001_01", "root"},
        {"Bip001 Pelvis_02", "pelvis"},
        {"Bip001 Spine_03", "spine_01"},
        {"Bip001 Spine1_012", "spine_02"},
        {"Bip001 Neck_013", "neck_01"},
        {"Bip001 Head_00", "Head"},
        {"Bip001 L Clavicle_015", "clavicle_l"},
        {"Bip001 L UpperArm_016", "upperarm_l"},
        {"Bip001 L Forearm_017", "lowerarm_l"},
        {"Bip001 L Hand_018", "hand_l"},
        {"Bip001 R Clavicle_035", "clavicle_r"},
        {"Bip001 R UpperArm_036", "upperarm_r"},
        {"Bip001 R Forearm_037", "lowerarm_r"},
        {"Bip001 R Hand_038", "hand_r"},
        {"Bip001 L Thigh_04", "thigh_l"},
        {"Bip001 L Calf_05", "calf_l"},
        {"Bip001 L Foot_06", "foot_l"},
        {"Bip001 L Toe0_07", "ball_l"},
        {"Bip001 R Thigh_08", "thigh_r"},
        {"Bip001 R Calf_09", "calf_r"},
        {"Bip001 R Foot_010", "foot_r"},
        {"Bip001 R Toe0_011", "ball_r"},
    };

    struct FingerMap
    {
        const char *biped;
        const char *ual;
    } fingers[] = {
        {"Finger0", "thumb"}, {"Finger1", "index"}, {"Finger2", "middle"},
        {"Finger3", "ring"}, {"Finger4", "pinky"},
    };

    for (const char side : {'L', 'R'})
    {
        const char *ualSide = side == 'L' ? "l" : "r";
        const unsigned base = side == 'L' ? 19u : 39u;
        for (unsigned finger = 0; finger < 5; ++finger)
            for (unsigned segment = 0; segment < 3; ++segment)
            {
                std::string target = "Bip001 ";
                target += side;
                target += ' ';
                target += fingers[finger].biped;
                if (segment == 1)
                    target += '1';
                else if (segment == 2)
                    target += '2';
                target += '_';
                target += ThreeDigits(base + finger * 3 + segment);

                std::string source = fingers[finger].ual;
                source += "_0" + std::to_string(segment + 1) + "_" + ualSide;
                map.emplace(std::move(target), std::move(source));
            }
    }
    return map;
}

int FunctionKeySlot(PlagC::KeyCode key)
{
    switch (key)
    {
    case PlagC::KeyCode::F1: return 0;
    case PlagC::KeyCode::F2: return 1;
    case PlagC::KeyCode::F3: return 2;
    case PlagC::KeyCode::F4: return 3;
    case PlagC::KeyCode::F5: return 4;
    case PlagC::KeyCode::F6: return 5;
    case PlagC::KeyCode::F7: return 6;
    case PlagC::KeyCode::F8: return 7;
    case PlagC::KeyCode::F9: return 8;
    case PlagC::KeyCode::F10: return 9;
    case PlagC::KeyCode::F11: return 10;
    case PlagC::KeyCode::F12: return 11;
    default: return -1;
    }
}
} // namespace

void RigValidationWorld::Start(PlagC::Engine &engine)
{
    auto jokerBase = engine.Assets().Get<PlagC::ModelAsset>(
        "Game://Characters/Player/JokerBase/JokerBase.plasset");
    auto ual = engine.Assets().Get<PlagC::ModelAsset>(
        "Game://Animations/UAL/UAL1_Standard.plasset");

    if (!jokerBase || !ual)
        throw std::runtime_error("RigValidation JokerBase/UAL assets are not ready");
    if (!jokerBase->animations.empty())
        throw std::runtime_error("JokerBase must contain zero native animations");
    if (ual->animations.empty())
        throw std::runtime_error("RigValidation UAL asset contains no animations");

    ualClipCount_ = unsigned(ual->animations.size());

    PlagC::AnimationRetargetConfig retarget;
    retarget.rootTranslation = false;
    retarget.pelvisTranslation = false;
    retarget.translationScale = 1.0f;
    retarget.jointMap = JokerToUAL();
    retarget.sourceReferenceClip = "A_TPose";
    retarget.preserveTargetAnimations = false;
    jokerAsset_ = PlagC::RetargetAnimations(jokerBase, ual, retarget);

    if (jokerAsset_->animations.size() != ualClipCount_)
        throw std::runtime_error("RigValidation UAL-only clip count mismatch");

    requestedMode_ = 0;
    activeMode_ = -1;
    ualPage_ = 0;

    world_ = std::make_unique<PlagC::World>();
    assetScene_ = std::make_unique<PlagC::AssetScene>(engine.Graphics());
    sceneRenderer_ = std::make_unique<PlagC::SceneRenderer>(engine.Graphics());
#ifdef PC_ENABLE_PROFILING
    sceneRenderer_->SetProfiling(true);
#endif

    joker_ = world_->Create("JokerBase UAL Validation");
    world_->SetTransform(joker_,
                         {{0, 0, 0}, {0, 180, 0},
                          {JokerVisualScale, JokerVisualScale, JokerVisualScale}});
    world_->SetModel(joker_, {jokerAsset_, true});
    // No AnimatorComponent: mode 0 displays only the stripped mesh + bind/rest skeleton.

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

    auto logPage = [this, &engine]() {
        const unsigned first = ualPage_ * UalPageSize;
        std::string line = "UAL page " + std::to_string(ualPage_ + 1) + ": ";
        for (unsigned slot = 0; slot < UalPageSize; ++slot)
        {
            const unsigned ualIndex = first + slot;
            if (ualIndex >= ualClipCount_)
                break;
            if (slot)
                line += ", ";
            line += "F" + std::to_string(slot + 1) + "=" +
                    jokerAsset_->animations[ualIndex].name;
        }
        engine.Logger().Write(PlagC::Severity::Info, "RIG", line);
    };

    key_ = engine.Events().Subscribe(PlagC::EventType::KeyPressed,
                                     [this, &engine, logPage](const PlagC::Event &event) mutable {
        if (event.key == PlagC::KeyCode::D0)
            requestedMode_ = 0;
        else if (event.key == PlagC::KeyCode::PageUp)
        {
            const unsigned pages = (ualClipCount_ + UalPageSize - 1) / UalPageSize;
            ualPage_ = (ualPage_ + 1) % pages;
            logPage();
        }
        else if (event.key == PlagC::KeyCode::PageDown)
        {
            const unsigned pages = (ualClipCount_ + UalPageSize - 1) / UalPageSize;
            ualPage_ = (ualPage_ + pages - 1) % pages;
            logPage();
        }
        else if (const int slot = FunctionKeySlot(event.key); slot >= 0)
        {
            const unsigned ualIndex = ualPage_ * UalPageSize + unsigned(slot);
            if (ualIndex < ualClipCount_)
                requestedMode_ = 100 + int(ualIndex);
        }
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
        window->SetTitle("PlagC | Rig Validation | JokerBase + UAL");
        window->SetCursorCaptured(true);
    }
    engine.SetFPSLocked(false);

    assetScene_->Update(*world_, 0.0);
    sceneRenderer_->Sync(*world_);
    activeMode_ = 0;

    engine.Logger().Write(
        PlagC::Severity::Info, "RIG",
        "JokerBase UAL-only validation: 0=bare mesh+rest skeleton, F1-F12=UAL clips, PageUp/PageDown=UAL pages, mouse orbit");
    logPage();
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
            world_->RemoveAnimator(joker_);
            engine.Logger().Write(PlagC::Severity::Info, "RIG",
                                  "JokerBase mode: bare rest skeleton (no animation)");
        }
        else if (requestedMode_ >= 100)
        {
            const unsigned ualIndex = unsigned(requestedMode_ - 100);
            if (ualIndex >= ualClipCount_)
                throw std::out_of_range("UAL clip selection out of range");
            SetClip(*world_, joker_, ualIndex);
            engine.Logger().Write(
                PlagC::Severity::Info, "RIG",
                "UAL retarget clip: " + jokerAsset_->animations[ualIndex].name);
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
                          "JokerBase UAL RigValidation stopped");
}
} // namespace PlagCGame
