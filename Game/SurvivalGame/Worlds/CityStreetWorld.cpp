#include "pgcpch.h"
#include "CityStreetWorld.h"
#include <PlagC/Animation/AnimationRetarget.h>
#include <PlagC/Assets/AssetManager.h>
#include <PlagC/Input/Input.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>

namespace PlagCGame
{
namespace
{
using Clock = std::chrono::steady_clock;
constexpr float Pi = 3.14159265358979323846f;
constexpr float Rad = Pi / 180.0f;
constexpr int MagazineSize = 12;

double Milliseconds(Clock::time_point start)
{
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

PlagC::Vec3 Add(PlagC::Vec3 a, PlagC::Vec3 b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}
PlagC::Vec3 Scale(PlagC::Vec3 v, float s)
{
    return {v.x * s, v.y * s, v.z * s};
}
float LengthXZ(PlagC::Vec3 v)
{
    return std::sqrt(v.x * v.x + v.z * v.z);
}

unsigned Clip(const PlagC::ModelAsset &asset, std::string_view name)
{
    const auto found = std::find_if(asset.animations.begin(), asset.animations.end(),
                                    [&](const auto &clip) { return clip.name == name; });
    if (found == asset.animations.end())
        throw std::runtime_error("required animation clip is missing: " + std::string(name));
    return unsigned(found - asset.animations.begin());
}

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

PlagC::AnimationStateMachine LocomotionGraph(const PlagC::ModelAsset &asset)
{
    PlagC::AnimationStateMachine graph;
    graph.parameters.push_back({"Mode", PlagC::AnimationParameterType::Float, 0});
    graph.states = {
        {"Idle", Clip(asset, "Idle_Loop"), true, 1.0f},
        {"Walk", Clip(asset, "Walk_Loop"), true, 1.0f},
        {"Jog", Clip(asset, "Jog_Fwd_Loop"), true, 1.0f},
        {"Sprint", Clip(asset, "Sprint_Loop"), true, 1.0f},
        {"Jump", Clip(asset, "Jump_Loop"), true, 1.0f},
        {"CrouchIdle", Clip(asset, "Crouch_Idle_Loop"), true, 1.0f},
        {"CrouchWalk", Clip(asset, "Crouch_Fwd_Loop"), true, 1.0f},
    };
    for (unsigned from = 0; from < graph.states.size(); ++from)
        for (unsigned to = 0; to < graph.states.size(); ++to)
            if (from != to)
                graph.transitions.push_back(
                    {from, to, 0.12f, {{"Mode", PlagC::AnimationCompare::Equal, float(to)}}});
    return graph;
}
} // namespace

void CityStreetWorld::Start(PlagC::Engine &engine)
{
    auto cityStreet = engine.Assets().Get<PlagC::ModelAsset>(
        "Game://Environment/CityStreet/CityStreet.plasset");
    auto joel = engine.Assets().Get<PlagC::ModelAsset>(
        "Game://Characters/Player/Joel/Joel.plasset");
    auto ual = engine.Assets().Get<PlagC::ModelAsset>(
        "Game://Animations/UAL/UAL1_Standard.plasset");
    auto pistol = engine.Assets().Get<PlagC::ModelAsset>(
        "Game://Weapons/Pistol/Pistol.plasset");
    auto jeep = engine.Assets().Get<PlagC::ModelAsset>(
        "Game://Environment/Props/Jeep/Jeep.plasset");

    if (!cityStreet || !joel || !ual || !pistol || !jeep)
        throw std::runtime_error("CityStreetWorld required assets are not ready");

    playerAsset_ = PlagC::RetargetAnimations(joel, ual);

    world_ = std::make_unique<PlagC::World>();

    PlagC::PhysicsConfig physicsConfig;
    physicsConfig.workers = config_.physicsWorkers;
    physicsConfig.fixedHz = config_.physicsHz;
    physicsConfig.scheduler = config_.physicsScheduler == "plagc"
                                  ? PlagC::PhysicsScheduler::PlagC
                                  : PlagC::PhysicsScheduler::Native;
    physicsConfig.sharedJobs = &engine.Jobs();
    physics_ = std::make_unique<PlagC::PhysicsSystem>(*world_, physicsConfig);

    assetScene_ = std::make_unique<PlagC::AssetScene>(engine.Graphics());
    sceneRenderer_ = std::make_unique<PlagC::SceneRenderer>(engine.Graphics());
#ifdef PC_ENABLE_PROFILING
    sceneRenderer_->SetProfiling(true);
#endif

    environment_ = ModelEntity(*world_, "CityStreet Environment", cityStreet,
                               {{0, 0, 0}, {0, 0, 0}, {1, 1, 1}});

    // Broad gameplay collision for the first playable slice. Authored mesh collision
    // extraction is a later world-cooking step; this already gives Jolt grounding,
    // gravity, jumping and a stable street test surface.
    auto streetCollider = world_->Create("CityStreet Collision");
    world_->SetTransform(streetCollider, {{0, -0.25f, 0}, {}, {1, 1, 1}});
    PlagC::PhysicsComponent streetPhysics;
    streetPhysics.body.motion = PlagC::MotionType::Static;
    streetPhysics.body.layer = PlagC::CollisionLayer::StaticWorld;
    streetPhysics.collider.type = PlagC::ColliderType::Box;
    streetPhysics.collider.dimensions = {11.5f, 0.25f, 14.0f};
    world_->SetPhysics(streetCollider, streetPhysics);

    player_ = ModelEntity(*world_, "Joel", playerAsset_,
                          {{0.0f, 0.20f, 4.0f}, {0, playerYaw_, 0}, {1, 1, 1}});

    auto graph = LocomotionGraph(*playerAsset_);
    PlagC::AnimatorComponent animator;
    animator.state.clip = graph.states[0].clip;
    animator.controller.emplace(graph, unsigned(playerAsset_->animations.size()));
    PlagC::AnimationUpperBody weaponLayer;
    weaponLayer.root = "spine_01";
    weaponLayer.pose = {Clip(*playerAsset_, "Pistol_Idle_Loop"), 0, 1, true, true};
    weaponLayer.weight = 1.0f;
    animator.upperBody = weaponLayer;
    world_->SetAnimator(player_, animator);

    pistol_ = ModelEntity(*world_, "Pistol", pistol,
                          {{0.0f, 1.2f, 4.0f}, {0, playerYaw_, 0}, {1, 1, 1}});

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
    world_->SetTransform(camera_, {{0, 3.2f, 8.5f}, {}, {1, 1, 1}});
    PlagC::CameraComponent camera;
    camera.target = {0.0f, 1.2f, 4.0f};
    camera.fov = 58.0f;
    camera.nearClip = 0.08f;
    camera.farClip = 350.0f;
    world_->SetCamera(camera_, camera);
    world_->SetActiveCamera(camera_);

    physics_->Synchronize();
    PlagC::CharacterDesc characterDesc;
    characterDesc.position = {0.0f, 1.15f, 4.0f};
    characterDesc.radius = 0.34f;
    characterDesc.halfHeight = 0.56f;
    characterDesc.mass = 78.0f;
    characterDesc.maxSlopeDegrees = 45.0f;
    characterDesc.stepHeight = 0.35f;
    character_ = physics_->CreateCharacter(characterDesc);

    key_ = engine.Events().Subscribe(PlagC::EventType::KeyPressed,
                                     [this, &engine](const PlagC::Event &event) {
        if (event.key == PlagC::KeyCode::Space)
            jumpQueued_ = true;
        else if (event.key == PlagC::KeyCode::R)
            reloadQueued_ = true;
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
        if (event.button == PlagC::MouseCode::Left)
        {
            if (!cursorCaptured_)
            {
                cursorCaptured_ = true;
                if (auto *window = engine.MainWindow())
                    window->SetCursorCaptured(true);
            }
            else
                fireQueued_ = true;
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
        window->SetTitle("PlagC | Survival Game | City Street");
        window->SetCursorCaptured(true);
    }

    assetScene_->Update(*world_, 0.0);
    sceneRenderer_->Sync(*world_);

    engine.Logger().Write(
        PlagC::Severity::Info, "WORLD",
        "CityStreet combat slice ready: WASD, Shift sprint, Ctrl crouch, Space jump, RMB aim, LMB fire, R reload");
}

void CityStreetWorld::Frame(PlagC::Engine &engine, double dt)
{
    if (!world_ || !physics_ || !assetScene_ || !sceneRenderer_ || !character_ || !playerAsset_)
        throw std::logic_error("CityStreetWorld frame before Start");
    if (!std::isfinite(dt) || dt < 0)
        throw std::invalid_argument("invalid CityStreetWorld timestep");

    dt = std::min(dt, 0.1);
    shotCooldown_ = std::max(0.0, shotCooldown_ - dt);
    fireAnimationTime_ = std::max(0.0, fireAnimationTime_ - dt);

    if (reloadTime_ > 0)
    {
        const double before = reloadTime_;
        reloadTime_ = std::max(0.0, reloadTime_ - dt);
        if (before > 0 && reloadTime_ == 0)
        {
            const int needed = MagazineSize - rounds_;
            const int loaded = std::min(needed, reserve_);
            rounds_ += loaded;
            reserve_ -= loaded;
        }
    }

    auto t = Clock::now();
    if (cursorCaptured_)
    {
        const auto mouse = PlagC::Input::GetMouseDelta();
        cameraYaw_ += float(mouse.x) * 0.12f;
        cameraPitch_ = std::clamp(cameraPitch_ - float(mouse.y) * 0.10f, -25.0f, 55.0f);
    }

    const bool aiming = cursorCaptured_ && PlagC::Input::IsMouseButtonPressed(PlagC::MouseCode::Right);
    crouched_ = PlagC::Input::IsKeyPressed(PlagC::KeyCode::LeftControl) ||
                PlagC::Input::IsKeyPressed(PlagC::KeyCode::RightControl);

    float forwardInput = 0.0f;
    float rightInput = 0.0f;
    if (PlagC::Input::IsKeyPressed(PlagC::KeyCode::W))
        forwardInput += 1.0f;
    if (PlagC::Input::IsKeyPressed(PlagC::KeyCode::S))
        forwardInput -= 1.0f;
    if (PlagC::Input::IsKeyPressed(PlagC::KeyCode::D))
        rightInput += 1.0f;
    if (PlagC::Input::IsKeyPressed(PlagC::KeyCode::A))
        rightInput -= 1.0f;

    const float yaw = cameraYaw_ * Rad;
    PlagC::Vec3 forward{std::sin(yaw), 0, -std::cos(yaw)};
    PlagC::Vec3 right{std::cos(yaw), 0, std::sin(yaw)};
    PlagC::Vec3 movement = Add(Scale(forward, forwardInput), Scale(right, rightInput));
    float length = LengthXZ(movement);
    if (length > 1.0f)
        movement = Scale(movement, 1.0f / length);

    const bool sprint = !crouched_ && !aiming &&
                        (PlagC::Input::IsKeyPressed(PlagC::KeyCode::LeftShift) ||
                         PlagC::Input::IsKeyPressed(PlagC::KeyCode::RightShift));
    const float speed = crouched_ ? 1.7f : aiming ? 2.3f : sprint ? 6.0f : 3.3f;
    movement = Scale(movement, speed);

    physics_->SetCharacterInput(character_, movement, jumpQueued_ && !crouched_);
    jumpQueued_ = false;
    engine.timings.physicsInputMs = Milliseconds(t);

    t = Clock::now();
    physics_->Advance(dt);
    engine.timings.physicsMs = Milliseconds(t);

    t = Clock::now();
    const auto character = physics_->ReadCharacter(character_);
    engine.timings.physicsReadbackMs = Milliseconds(t);

    t = Clock::now();
    if (aiming)
        playerYaw_ += std::remainder(180.0f + cameraYaw_ - playerYaw_, 360.0f) *
                      (1.0f - std::exp(-float(dt) * 16.0f));
    else if (LengthXZ(movement) > 0.05f)
    {
        const float desired = std::atan2(movement.x, movement.z) / Rad;
        playerYaw_ += std::remainder(desired - playerYaw_, 360.0f) *
                      (1.0f - std::exp(-float(dt) * 14.0f));
    }
    world_->SetTransform(player_,
                         {{character.position.x, character.position.y - 0.95f, character.position.z},
                          {0, playerYaw_, 0}, {1, 1, 1}});
    engine.timings.characterWorldMs = Milliseconds(t);

    t = Clock::now();
    const PlagC::Vec3 target{character.position.x, character.position.y + (crouched_ ? 0.30f : 0.55f),
                             character.position.z};
    const float pitch = cameraPitch_ * Rad;
    const float cp = std::cos(pitch);
    const PlagC::Vec3 lookDirection{std::sin(yaw) * cp, std::sin(pitch), -std::cos(yaw) * cp};
    const float distance = aiming ? 3.2f : 4.8f;
    const PlagC::Vec3 shoulder = Add(target, Scale(right, aiming ? 0.45f : 0.15f));
    const PlagC::Vec3 cameraPosition = Add(shoulder, Scale(lookDirection, -distance));
    world_->SetTransform(camera_, {cameraPosition, {}, {1, 1, 1}});
    auto cameraComponent = world_->Read(camera_).camera.value();
    cameraComponent.target = Add(target, Scale(lookDirection, aiming ? 10.0f : 0.0f));
    cameraComponent.fov = aiming ? 48.0f : 58.0f;
    world_->SetCamera(camera_, cameraComponent);
    engine.timings.cameraMs = Milliseconds(t);

    if (reloadQueued_)
    {
        reloadQueued_ = false;
        if (reloadTime_ <= 0 && rounds_ < MagazineSize && reserve_ > 0)
            reloadTime_ = playerAsset_->animations[Clip(*playerAsset_, "Pistol_Reload")].duration;
    }

    const bool fire = fireQueued_ && cursorCaptured_ && reloadTime_ <= 0 && shotCooldown_ <= 0 && rounds_ > 0;
    fireQueued_ = false;
    if (fire)
    {
        --rounds_;
        shotCooldown_ = 0.18;
        fireAnimationTime_ = playerAsset_->animations[Clip(*playerAsset_, "Pistol_Shoot")].duration;
        const PlagC::Vec3 origin = cameraPosition;
        const PlagC::Vec3 ray = Scale(lookDirection, 150.0f);
        if (auto hit = physics_->Raycast(origin, ray))
            engine.Logger().Write(PlagC::Severity::Debug, "COMBAT",
                                  "Pistol hit at " + std::to_string(hit->position.x) + ", " +
                                      std::to_string(hit->position.y) + ", " +
                                      std::to_string(hit->position.z));
    }

    t = Clock::now();
    auto animator = world_->Read(player_).animator.value();
    const float horizontalVelocity = LengthXZ(character.velocity);
    unsigned mode = 0;
    if (!character.grounded)
        mode = 4;
    else if (crouched_)
        mode = horizontalVelocity > 0.15f ? 6 : 5;
    else if (horizontalVelocity > 5.0f)
        mode = 3;
    else if (horizontalVelocity > 2.0f)
        mode = 2;
    else if (horizontalVelocity > 0.15f)
        mode = 1;
    animator.controller->SetFloat("Mode", float(mode));

    auto &upper = *animator.upperBody;
    upper.weight = 1.0f;
    upper.recoilRadians = fireAnimationTime_ > 0 ? -0.035f : 0.0f;
    if (reloadTime_ > 0)
    {
        const unsigned clip = Clip(*playerAsset_, "Pistol_Reload");
        const double duration = playerAsset_->animations[clip].duration;
        upper.pose = {clip, duration - reloadTime_, 1, false, false};
    }
    else if (fireAnimationTime_ > 0)
    {
        const unsigned clip = Clip(*playerAsset_, "Pistol_Shoot");
        const double duration = playerAsset_->animations[clip].duration;
        upper.pose = {clip, duration - fireAnimationTime_, 1, false, false};
    }
    else if (aiming)
        upper.pose = {Clip(*playerAsset_, "Pistol_Aim_Neutral"), 0, 1, false, false};
    else
        upper.pose = {Clip(*playerAsset_, "Pistol_Idle_Loop"), upper.pose.time, 1, true, true};
    world_->SetAnimator(player_, animator);
    engine.timings.animationSetupMs = Milliseconds(t);

    t = Clock::now();
    assetScene_->Update(*world_, dt);

    // Pistol follows the animated right hand. Orientation is still a simple authored
    // offset; position already comes from the final retargeted/blended skeleton pose.
    if (auto hand = assetScene_->AnimatedNodePoint(*world_, player_, "hand_r", {0.02f, 0.0f, 0.02f}))
    {
        world_->SetTransform(pistol_, {*hand, {0, playerYaw_ - 90.0f, 90.0f}, {0.82f, 0.82f, 0.82f}});
        assetScene_->Update(*world_, 0.0);
    }
    engine.timings.animationMs = Milliseconds(t);

    t = Clock::now();
    sceneRenderer_->Sync(*world_, physics_->Presentation());
    engine.timings.syncMs = Milliseconds(t);

    const auto stats = sceneRenderer_->Statistics();
    engine.timings.syncStructureMs = stats.structureMs;
    engine.timings.syncResourceMs = stats.resourceMs;
    engine.timings.syncTransformMs = stats.transformMs;
    engine.timings.syncPropertyMs = stats.propertyMs;
    engine.timings.syncLightCameraMs = stats.lightCameraMs;
    engine.timings.syncTraversalMs = stats.traversalMs;
    engine.timings.syncScanned = stats.scanned;
    engine.timings.syncTransforms = stats.transforms;
    engine.timings.syncPropertyCalls = stats.propertyCalls;
    engine.timings.syncLightCalls = stats.lightCalls;
    engine.timings.syncCameraCalls = stats.cameraCalls;
    engine.timings.syncCreated = stats.created;
    engine.timings.syncDestroyed = stats.destroyed;
}

void CityStreetWorld::Stop(PlagC::Engine &engine)
{
    key_.Reset();
    focus_.Reset();
    mouse_.Reset();

    if (auto *window = engine.MainWindow())
        window->SetCursorCaptured(false);

    if (physics_ && character_)
        physics_->DestroyCharacter(character_);
    character_ = {};

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
    environment_ = {};
    player_ = {};
    pistol_ = {};
    jeep_ = {};
    grass_ = {};
    vegetation_ = {};
    camera_ = {};
    sun_ = {};

    engine.Logger().Write(PlagC::Severity::Info, "WORLD", "CityStreetWorld stopped");
}
} // namespace PlagCGame
