#include "pgcpch.h"
#include "BlackDragonLabWorld.h"
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
constexpr float Pi = 3.14159265358979323846f;
constexpr float Rad = Pi / 180.0f;

// UAL1_Standard.plasset clip indices. The lab retargets with
// preserveTargetAnimations=false, therefore these indices remain unchanged.
constexpr unsigned ClipCrouchForward = 1;
constexpr unsigned ClipCrouchIdle = 2;
constexpr unsigned ClipIdle = 9;
constexpr unsigned ClipJog = 13;
constexpr unsigned ClipPistolIdle = 21;
constexpr unsigned ClipPistolShoot = 23;
constexpr unsigned ClipSprint = 36;

float Length2(float x, float z)
{
    return std::sqrt(x * x + z * z);
}

float Approach(float current, float target, float amount)
{
    if (current < target)
        return std::min(current + amount, target);
    return std::max(current - amount, target);
}

float WrapDegrees(float value)
{
    while (value > 180.0f)
        value -= 360.0f;
    while (value < -180.0f)
        value += 360.0f;
    return value;
}

float ApproachAngle(float current, float target, float maxStep)
{
    const float delta = WrapDegrees(target - current);
    return WrapDegrees(current + std::clamp(delta, -maxStep, maxStep));
}

std::unordered_map<std::string, std::string> BlackDragonToUAL()
{
    std::unordered_map<std::string, std::string> map = {
        {"mixamorig:Hips", "pelvis"},
        {"mixamorig:Spine", "spine_01"},
        {"mixamorig:Spine1", "spine_02"},
        {"mixamorig:Spine2", "spine_03"},
        {"mixamorig:Neck", "neck_01"},
        {"mixamorig:Head", "Head"},
        {"mixamorig:LeftShoulder", "clavicle_l"},
        {"mixamorig:LeftArm", "upperarm_l"},
        {"mixamorig:LeftForeArm", "lowerarm_l"},
        {"mixamorig:LeftHand", "hand_l"},
        {"mixamorig:RightShoulder", "clavicle_r"},
        {"mixamorig:RightArm", "upperarm_r"},
        {"mixamorig:RightForeArm", "lowerarm_r"},
        {"mixamorig:RightHand", "hand_r"},
        {"mixamorig:LeftUpLeg", "thigh_l"},
        {"mixamorig:LeftLeg", "calf_l"},
        {"mixamorig:LeftFoot", "foot_l"},
        {"mixamorig:LeftToeBase", "ball_l"},
        {"mixamorig:LeftToe_End", "ball_leaf_l"},
        {"mixamorig:RightUpLeg", "thigh_r"},
        {"mixamorig:RightLeg", "calf_r"},
        {"mixamorig:RightFoot", "foot_r"},
        {"mixamorig:RightToeBase", "ball_r"},
        {"mixamorig:RightToe_End", "ball_leaf_r"},
    };

    struct Finger
    {
        const char *mixamo;
        const char *ual;
    };
    constexpr Finger fingers[] = {
        {"Thumb", "thumb"},
        {"Middle", "middle"},
        {"Ring", "ring"},
        {"Pinky", "pinky"},
    };

    for (const char *side : {"Left", "Right"})
    {
        const char *suffix = std::string_view(side) == "Left" ? "l" : "r";
        for (const auto &finger : fingers)
            for (unsigned segment = 1; segment <= 4; ++segment)
            {
                std::string target = "mixamorig:";
                target += side;
                target += "Hand";
                target += finger.mixamo;
                target += std::to_string(segment);

                std::string source = finger.ual;
                if (segment < 4)
                    source += "_0" + std::to_string(segment) + "_" + suffix;
                else
                    source += "_04_leaf_" + std::string(suffix);
                map.emplace(std::move(target), std::move(source));
            }
    }
    return map;
}

PlagC::AnimationStateMachine BuildLocomotionGraph()
{
    using P = PlagC::AnimationParameter;
    using PT = PlagC::AnimationParameterType;
    using N = PlagC::AnimationNode;
    using T = PlagC::AnimationTransition;
    using C = PlagC::AnimationCondition;
    using Compare = PlagC::AnimationCompare;

    PlagC::AnimationStateMachine graph;
    graph.parameters = {P{"Speed", PT::Float, 0.0f}, P{"Crouch", PT::Bool, 0.0f}};
    graph.states = {
        N{"Idle", ClipIdle, true, 1.0f},
        N{"Jog", ClipJog, true, 1.0f},
        N{"Sprint", ClipSprint, true, 1.0f},
        N{"CrouchIdle", ClipCrouchIdle, true, 1.0f},
        N{"CrouchMove", ClipCrouchForward, true, 1.0f},
    };
    graph.entry = 0;

    auto speedGreater = [](float value) { return C{"Speed", Compare::Greater, value}; };
    auto speedLess = [](float value) { return C{"Speed", Compare::Less, value}; };
    const C crouchOn{"Crouch", Compare::Equal, 1.0f};
    const C crouchOff{"Crouch", Compare::Equal, 0.0f};

    constexpr float stop = 0.15f;
    constexpr float sprint = 4.0f;
    constexpr float fade = 0.18f;

    // Stand locomotion. Crouch transitions are listed first so posture changes
    // win over speed changes in the same frame.
    graph.transitions.push_back(T{0, 3, fade, {crouchOn}});
    graph.transitions.push_back(T{0, 2, fade, {crouchOff, speedGreater(sprint)}});
    graph.transitions.push_back(T{0, 1, fade, {crouchOff, speedGreater(stop)}});

    graph.transitions.push_back(T{1, 4, fade, {crouchOn, speedGreater(stop)}});
    graph.transitions.push_back(T{1, 3, fade, {crouchOn, speedLess(stop)}});
    graph.transitions.push_back(T{1, 0, fade, {crouchOff, speedLess(stop)}});
    graph.transitions.push_back(T{1, 2, fade, {crouchOff, speedGreater(sprint)}});

    graph.transitions.push_back(T{2, 4, fade, {crouchOn, speedGreater(stop)}});
    graph.transitions.push_back(T{2, 3, fade, {crouchOn, speedLess(stop)}});
    graph.transitions.push_back(T{2, 0, fade, {crouchOff, speedLess(stop)}});
    graph.transitions.push_back(T{2, 1, fade, {crouchOff, speedLess(sprint)}});

    graph.transitions.push_back(T{3, 0, fade, {crouchOff, speedLess(stop)}});
    graph.transitions.push_back(T{3, 2, fade, {crouchOff, speedGreater(sprint)}});
    graph.transitions.push_back(T{3, 1, fade, {crouchOff, speedGreater(stop)}});
    graph.transitions.push_back(T{3, 4, fade, {crouchOn, speedGreater(stop)}});

    graph.transitions.push_back(T{4, 3, fade, {crouchOn, speedLess(stop)}});
    graph.transitions.push_back(T{4, 0, fade, {crouchOff, speedLess(stop)}});
    graph.transitions.push_back(T{4, 2, fade, {crouchOff, speedGreater(sprint)}});
    graph.transitions.push_back(T{4, 1, fade, {crouchOff, speedGreater(stop)}});
    return graph;
}
} // namespace

void BlackDragonLabWorld::Start(PlagC::Engine &engine)
{
    if (!engine.Assets().HasMount("Game"))
        engine.Assets().Mount("Game", "GameAssets/Default/Runtime");

    const auto base = engine.Assets().Load<PlagC::ModelAsset>(
        "Game://Characters/BlackDragon/BlackDragon.plasset");
    const auto ual = engine.Assets().Load<PlagC::ModelAsset>(
        "Game://Animations/UAL/UAL1_Standard.plasset");
    if (!base || !ual)
        throw std::runtime_error("BlackDragonLab assets failed to load");
    if (ual->animations.size() != 43)
        throw std::runtime_error("BlackDragonLab requires the 43-clip UAL1_Standard asset");

    PlagC::AnimationRetargetConfig retarget;
    retarget.rootTranslation = false;
    retarget.pelvisTranslation = false;
    retarget.jointMap = BlackDragonToUAL();
    retarget.sourceReferenceClip = "A_TPose";
    retarget.preserveTargetAnimations = false;
    characterAsset_ = PlagC::RetargetAnimations(base, ual, retarget);
    if (characterAsset_->animations.size() != 43)
        throw std::runtime_error("BlackDragonLab retarget clip count mismatch");

    world_ = std::make_unique<PlagC::World>();
    assetScene_ = std::make_unique<PlagC::AssetScene>(engine.Graphics());
    sceneRenderer_ = std::make_unique<PlagC::SceneRenderer>(engine.Graphics());
#ifdef PC_ENABLE_PROFILING
    sceneRenderer_->SetProfiling(true);
#endif

    character_ = world_->Create("BlackDragon");
    position_ = {0.0f, 0.0f, 0.0f};
    characterYaw_ = 0.0f; // BlackDragon's authored face direction is +Z.
    world_->SetTransform(character_, {position_, {0.0f, characterYaw_, 0.0f}, {1.0f, 1.0f, 1.0f}});
    world_->SetModel(character_, {characterAsset_, true});

    PlagC::AnimatorComponent animator;
    animator.controller.emplace(BuildLocomotionGraph(), unsigned(characterAsset_->animations.size()));
    world_->SetAnimator(character_, animator);

    // Neutral visual floor only. No PhysicsComponent is attached in this phase.
    groundMesh_ = engine.Graphics().CreateMesh(PlagC::MakePrimitive(PlagC::Primitive::Plane));
    PlagC::MaterialDesc groundMaterial;
    groundMaterial.colour = {0.11f, 0.12f, 0.14f};
    groundMaterial.roughness = 0.92f;
    groundMaterial_ = engine.Graphics().CreateMaterial(groundMaterial);
    ground_ = world_->Create("Animation Lab Floor (visual only)");
    world_->SetTransform(ground_, {{0.0f, -0.002f, 0.0f}, {}, {30.0f, 1.0f, 30.0f}});
    world_->SetRenderable(ground_, {groundMesh_, groundMaterial_, true, true});

    sun_ = world_->Create("Animation Lab Sun");
    world_->SetTransform(sun_, {{0.0f, 8.0f, 0.0f}, {}, {1.0f, 1.0f, 1.0f}});
    PlagC::LightDesc sun;
    sun.type = PlagC::LightType::Directional;
    sun.direction = {-0.35f, -0.85f, -0.40f};
    sun.colour = {1.0f, 0.96f, 0.90f};
    sun.power = 4.0f;
    world_->SetLight(sun_, {sun});

    fill_ = world_->Create("Animation Lab Fill");
    world_->SetTransform(fill_, {{-2.0f, 2.2f, -1.5f}, {}, {1.0f, 1.0f, 1.0f}});
    PlagC::LightDesc fill;
    fill.type = PlagC::LightType::Point;
    fill.colour = {0.42f, 0.56f, 1.0f};
    fill.power = 6.0f;
    fill.radius = 7.0f;
    world_->SetLight(fill_, {fill});

    camera_ = world_->Create("Animation Lab Camera");
    world_->SetTransform(camera_, {{0.0f, 1.35f, -3.2f}, {}, {1.0f, 1.0f, 1.0f}});
    PlagC::CameraComponent camera;
    camera.target = {0.0f, 1.05f, 0.0f};
    camera.fov = 52.0f;
    camera.nearClip = 0.04f;
    camera.farClip = 120.0f;
    world_->SetCamera(camera_, camera);
    world_->SetActiveCamera(camera_);

    auto settings = engine.Graphics().Settings();
    settings.hdr = true;
    settings.bloom = false;
    settings.exposure = 1.0f;
    settings.fogDensity = 0.0f;
    settings.background = {0.025f, 0.030f, 0.045f};
    engine.Graphics().SetSettings(settings);
    engine.SetFPSLocked(false);

    key_ = engine.Events().Subscribe(PlagC::EventType::KeyPressed,
                                     [this, &engine](const PlagC::Event &event) {
        if (event.key == PlagC::KeyCode::Escape)
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
                                       [this, &engine](const PlagC::Event &) {
        if (!cursorCaptured_)
        {
            cursorCaptured_ = true;
            if (auto *window = engine.MainWindow())
                window->SetCursorCaptured(true);
        }
    });

    if (auto *window = engine.MainWindow())
    {
        window->SetTitle("PlagC | BlackDragon + UAL Animation Lab");
        window->SetCursorCaptured(true);
    }

    assetScene_->Update(*world_, 0.0);
    sceneRenderer_->Sync(*world_);

    engine.Logger().Write(
        PlagC::Severity::Info, "GAME",
        "BlackDragonLab ready | WASD move | Shift sprint | C/Ctrl crouch | RMB aim | LMB fire animation | mouse look | Esc cursor");
    engine.Logger().Write(
        PlagC::Severity::Info, "ANIMATION",
        "BlackDragon: 1.70m authored scale, A-pose bind/rest; UAL 43 clips retargeted in global reference space with A_TPose; root/pelvis translation disabled for in-place locomotion");
}

void BlackDragonLabWorld::Frame(PlagC::Engine &engine, double dt)
{
    if (!world_ || !assetScene_ || !sceneRenderer_ || !characterAsset_)
        throw std::logic_error("BlackDragonLab frame before Start");
    if (!std::isfinite(dt) || dt < 0.0)
        throw std::invalid_argument("invalid BlackDragonLab timestep");
    dt = std::min(dt, 0.1);

    if (cursorCaptured_)
    {
        const auto mouse = PlagC::Input::GetMouseDelta();
        cameraYaw_ = WrapDegrees(cameraYaw_ + float(mouse.x) * 0.12f);
        cameraPitch_ = std::clamp(cameraPitch_ - float(mouse.y) * 0.10f, -25.0f, 55.0f);
    }

    const bool forwardPressed = PlagC::Input::IsKeyPressed(PlagC::KeyCode::W);
    const bool backwardPressed = PlagC::Input::IsKeyPressed(PlagC::KeyCode::S);
    const bool leftPressed = PlagC::Input::IsKeyPressed(PlagC::KeyCode::A);
    const bool rightPressed = PlagC::Input::IsKeyPressed(PlagC::KeyCode::D);
    const bool crouching = PlagC::Input::IsKeyPressed(PlagC::KeyCode::C) ||
                            PlagC::Input::IsKeyPressed(PlagC::KeyCode::LeftControl);
    const bool aiming = PlagC::Input::IsMouseButtonPressed(PlagC::MouseCode::Right);

    const float yaw = cameraYaw_ * Rad;
    const PlagC::Vec3 cameraForward{std::sin(yaw), 0.0f, std::cos(yaw)};
    const PlagC::Vec3 cameraRight{std::cos(yaw), 0.0f, -std::sin(yaw)};

    float inputX = float(rightPressed) - float(leftPressed);
    float inputZ = float(forwardPressed) - float(backwardPressed);
    float moveX = cameraRight.x * inputX + cameraForward.x * inputZ;
    float moveZ = cameraRight.z * inputX + cameraForward.z * inputZ;
    const float moveLength = Length2(moveX, moveZ);
    const bool moving = moveLength > 0.001f;
    if (moving)
    {
        moveX /= moveLength;
        moveZ /= moveLength;
    }

    const bool sprinting = moving && !crouching && !aiming &&
                           (PlagC::Input::IsKeyPressed(PlagC::KeyCode::LeftShift) ||
                            PlagC::Input::IsKeyPressed(PlagC::KeyCode::RightShift));
    const float targetSpeed = !moving ? 0.0f : (crouching ? 1.35f : (sprinting ? 5.2f : 2.6f));
    const float response = targetSpeed > locomotionSpeed_ ? 12.0f : 16.0f;
    locomotionSpeed_ = Approach(locomotionSpeed_, targetSpeed, response * float(dt));

    // Phase 1 deliberately has no physics. Entity translation is authoritative so
    // animation, scale and transform correctness can be isolated before collision.
    if (moving)
    {
        position_.x += moveX * targetSpeed * float(dt);
        position_.z += moveZ * targetSpeed * float(dt);
    }

    float desiredYaw = characterYaw_;
    if (aiming)
        desiredYaw = cameraYaw_;
    else if (moving)
        desiredYaw = std::atan2(moveX, moveZ) / Rad;
    characterYaw_ = ApproachAngle(characterYaw_, desiredYaw, 720.0f * float(dt));
    world_->SetTransform(character_,
                         {position_, {0.0f, characterYaw_, 0.0f}, {1.0f, 1.0f, 1.0f}});

    auto record = world_->Read(character_);
    auto animator = record.animator.value();
    if (!animator.controller)
        throw std::logic_error("BlackDragonLab animator controller missing");
    animator.controller->SetFloat("Speed", locomotionSpeed_);
    animator.controller->SetBool("Crouch", crouching);

    const bool fireDown = PlagC::Input::IsMouseButtonPressed(PlagC::MouseCode::Left);
    if (fireDown && !previousFire_)
    {
        firing_ = true;
        upperBodyTime_ = 0.0;
    }
    previousFire_ = fireDown;

    if (firing_)
    {
        upperBodyTime_ += dt;
        const double duration = characterAsset_->animations.at(ClipPistolShoot).duration;
        if (upperBodyTime_ >= duration)
        {
            upperBodyTime_ = 0.0;
            firing_ = false;
        }
    }
    else if (aiming)
    {
        const double duration = characterAsset_->animations.at(ClipPistolIdle).duration;
        upperBodyTime_ = std::fmod(upperBodyTime_ + dt, duration);
    }
    else
        upperBodyTime_ = 0.0;

    const float upperTarget = (aiming || firing_) ? 1.0f : 0.0f;
    aimWeight_ = Approach(aimWeight_, upperTarget, float(dt) / 0.12f);
    if (aimWeight_ > 0.001f)
    {
        PlagC::AnimationUpperBody upper;
        upper.root = "mixamorig:Spine1";
        upper.weight = aimWeight_;
        upper.pose.clip = firing_ ? ClipPistolShoot : ClipPistolIdle;
        upper.pose.time = upperBodyTime_;
        upper.pose.speed = 1.0f;
        upper.pose.playing = false; // The lab supplies the upper-body clock explicitly.
        upper.pose.loop = !firing_;
        if (firing_)
        {
            const float duration = characterAsset_->animations.at(ClipPistolShoot).duration;
            const float phase = duration > 0.0f ? float(upperBodyTime_) / duration : 0.0f;
            upper.recoilRadians = std::sin(std::clamp(phase, 0.0f, 1.0f) * Pi) * 0.045f;
        }
        animator.upperBody = upper;
    }
    else
        animator.upperBody.reset();
    world_->SetAnimator(character_, animator);

    const float pitch = cameraPitch_ * Rad;
    const float cp = std::cos(pitch);
    const PlagC::Vec3 viewForward{std::sin(yaw) * cp, std::sin(pitch), std::cos(yaw) * cp};
    const float cameraDistance = aiming ? 2.35f : 3.35f;
    const float shoulder = aiming ? 0.32f : 0.0f;
    const float targetHeight = crouching ? 0.88f : 1.12f;
    const PlagC::Vec3 target{position_.x, position_.y + targetHeight, position_.z};
    const PlagC::Vec3 cameraPosition{
        target.x - viewForward.x * cameraDistance + cameraRight.x * shoulder,
        target.y - viewForward.y * cameraDistance + 0.18f,
        target.z - viewForward.z * cameraDistance + cameraRight.z * shoulder};
    world_->SetTransform(camera_, {cameraPosition, {}, {1.0f, 1.0f, 1.0f}});
    auto camera = world_->Read(camera_).camera.value();
    camera.target = target;
    camera.fov = aiming ? 45.0f : 52.0f;
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

void BlackDragonLabWorld::Stop(PlagC::Engine &engine)
{
    key_.Reset();
    focus_.Reset();
    mouse_.Reset();
    if (auto *window = engine.MainWindow())
        window->SetCursorCaptured(false);

    sceneRenderer_.reset();
    assetScene_.reset();
    world_.reset();
    characterAsset_ = {};

    if (groundMesh_)
        engine.Graphics().DestroyMesh(groundMesh_);
    if (groundMaterial_)
        engine.Graphics().DestroyMaterial(groundMaterial_);
    groundMesh_ = {};
    groundMaterial_ = {};
}
} // namespace PlagCGame
