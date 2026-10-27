#include "pgcpch.h"
#include "CityStreetWorld.h"
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
constexpr float Rad = 3.14159265358979323846f / 180.0f;
constexpr float JoelScale = 0.70f;
constexpr PlagC::Vec3 PlayerSpawn{0.0f, 1.20f, 0.0f};

double Milliseconds(Clock::time_point start)
{
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

PlagC::Vec3 Add(PlagC::Vec3 a, PlagC::Vec3 b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}
PlagC::Vec3 Sub(PlagC::Vec3 a, PlagC::Vec3 b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}
PlagC::Vec3 Scale(PlagC::Vec3 v, float s)
{
    return {v.x * s, v.y * s, v.z * s};
}
float LengthXZ(PlagC::Vec3 v)
{
    return std::sqrt(v.x * v.x + v.z * v.z);
}
PlagC::Vec3 NormalizeXZ(PlagC::Vec3 v)
{
    const float l = LengthXZ(v);
    return l > 1e-5f ? PlagC::Vec3{v.x / l, 0.0f, v.z / l} : PlagC::Vec3{};
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

PlagC::CharacterDesc PlayerCharacterDesc(PlagC::Vec3 position)
{
    PlagC::CharacterDesc character;
    character.position = position;
    character.radius = 0.30f;
    character.halfHeight = 0.52f;
    character.mass = 78.0f;
    character.maxSlopeDegrees = 45.0f;
    character.stepHeight = 0.30f;
    return character;
}
} // namespace

void CityStreetWorld::Start(PlagC::Engine &engine)
{
    auto cityStreet = engine.Assets().Get<PlagC::ModelAsset>(
        "Game://Environment/CityStreet/CityStreet.plasset");
    auto joel = engine.Assets().Get<PlagC::ModelAsset>(
        "Game://Characters/Player/Joel/Joel.plasset");
    auto jeep = engine.Assets().Get<PlagC::ModelAsset>(
        "Game://Environment/Props/Jeep/Jeep.plasset");
    auto pistol = engine.Assets().Get<PlagC::ModelAsset>(
        "Game://Weapons/Pistol/Pistol.plasset");

    if (!cityStreet || !joel || !jeep || !pistol)
        throw std::runtime_error("CityStreetWorld required assets are not ready");

    // Calibration build: use Joel's bind pose only. The current automatic rig is not
    // animation-safe, so animation and weapon posing stay disabled until the rig asset
    // itself is replaced. This makes scale/orientation/collision independently testable.
    playerAsset_ = joel;

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

    // Stable calibration collision. Do not use the full raw triangle mesh here: the
    // imported city contains overlapping/decorative geometry that can initially embed
    // a character capsule. Authored collision will be generated as a separate asset.
    auto street = world_->Create("Street Calibration Floor");
    world_->SetTransform(street, {{-2.0f, -0.70f, -3.5f}, {}, {1, 1, 1}});
    PlagC::PhysicsComponent floorPhysics;
    floorPhysics.body.motion = PlagC::MotionType::Static;
    floorPhysics.body.layer = PlagC::CollisionLayer::StaticWorld;
    floorPhysics.collider.type = PlagC::ColliderType::Box;
    floorPhysics.collider.dimensions = {7.5f, 0.25f, 11.5f};
    world_->SetPhysics(street, floorPhysics);

    // Simple perimeter keeps the calibration character inside the authored street
    // while we derive clean per-building collision proxies from the source scene.
    const struct Wall { PlagC::Vec3 p, e; } walls[] = {
        {{-9.4f, 1.5f, -3.5f}, {0.25f, 2.5f, 11.5f}},
        {{ 5.4f, 1.5f, -3.5f}, {0.25f, 2.5f, 11.5f}},
        {{-2.0f, 1.5f,-15.0f}, {7.5f, 2.5f, 0.25f}},
        {{-2.0f, 1.5f,  8.0f}, {7.5f, 2.5f, 0.25f}},
    };
    for (const auto &wall : walls)
    {
        auto e = world_->Create("Calibration Boundary");
        world_->SetTransform(e, {wall.p, {}, {1,1,1}});
        PlagC::PhysicsComponent p;
        p.body.motion = PlagC::MotionType::Static;
        p.body.layer = PlagC::CollisionLayer::StaticWorld;
        p.collider.type = PlagC::ColliderType::Box;
        p.collider.dimensions = wall.e;
        world_->SetPhysics(e, p);
    }

    player_ = ModelEntity(*world_, "Joel", playerAsset_,
                          {PlayerSpawn, {0, 90.0f, 0}, {JoelScale, JoelScale, JoelScale}});

    jeep_ = ModelEntity(*world_, "Jeep", jeep,
                        {{-4.0f, 0.60f, -3.0f}, {0, 28, 0}, {1, 1, 1}});

    // Keep the pistol loaded but invisible in this calibration build. It will return
    // after a real weapon socket is authored against the replacement character rig.
    pistol_ = ModelEntity(*world_, "Pistol", pistol,
                          {PlayerSpawn, {}, {0.0045f, 0.0045f, 0.0045f}});
    auto pistolModel = world_->Read(pistol_).model.value();
    pistolModel.visible = false;
    world_->SetModel(pistol_, pistolModel);

    sun_ = world_->Create("Sun");
    world_->SetTransform(sun_, {{0, 12, 0}, {}, {1, 1, 1}});
    PlagC::LightDesc sun;
    sun.type = PlagC::LightType::Directional;
    sun.direction = {-0.42f, -0.86f, -0.28f};
    sun.colour = {1.0f, 0.94f, 0.84f};
    sun.power = 4.2f;
    world_->SetLight(sun_, {sun});

    camera_ = world_->Create("Gameplay Camera");
    world_->SetTransform(camera_, {{0.0f, 2.6f, 5.0f}, {}, {1, 1, 1}});
    PlagC::CameraComponent camera;
    camera.target = {PlayerSpawn.x, PlayerSpawn.y + 0.45f, PlayerSpawn.z};
    camera.fov = 58.0f;
    camera.nearClip = 0.08f;
    camera.farClip = 350.0f;
    world_->SetCamera(camera_, camera);
    world_->SetActiveCamera(camera_);

    physics_->Synchronize();
    character_ = physics_->CreateCharacter(PlayerCharacterDesc(PlayerSpawn));

    key_ = engine.Events().Subscribe(PlagC::EventType::KeyPressed,
                                     [this, &engine](const PlagC::Event &event) {
        if (event.key == PlagC::KeyCode::Space)
            jumpQueued_ = true;
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
        window->SetTitle("PlagC | Survival Game | City Street | Calibration");
        window->SetCursorCaptured(true);
    }

    assetScene_->Update(*world_, 0.0);
    sceneRenderer_->Sync(*world_);

    engine.Logger().Write(PlagC::Severity::Warning, "WORLD",
                          "Calibration build: static Joel, no weapon/animation; testing scale, input and stable collision only");
}

void CityStreetWorld::Frame(PlagC::Engine &engine, double dt)
{
    if (!world_ || !physics_ || !assetScene_ || !sceneRenderer_ || !character_)
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

    auto t = Clock::now();

    // Derive movement directly from the visible camera rather than duplicating yaw
    // conventions. W is always toward the centre of the screen; D is screen-right.
    const auto cameraTransform = world_->ReadTransform(camera_);
    const auto cameraComponent = world_->Read(camera_).camera.value();
    PlagC::Vec3 forward = NormalizeXZ(Sub(cameraComponent.target, cameraTransform.position));
    if (LengthXZ(forward) < 0.1f)
        forward = {0, 0, -1};
    const PlagC::Vec3 right{-forward.z, 0, forward.x};

    float forwardInput = 0.0f;
    float rightInput = 0.0f;
    if (PlagC::Input::IsKeyPressed(PlagC::KeyCode::W)) forwardInput += 1.0f;
    if (PlagC::Input::IsKeyPressed(PlagC::KeyCode::S)) forwardInput -= 1.0f;
    if (PlagC::Input::IsKeyPressed(PlagC::KeyCode::D)) rightInput += 1.0f;
    if (PlagC::Input::IsKeyPressed(PlagC::KeyCode::A)) rightInput -= 1.0f;

    PlagC::Vec3 movement = Add(Scale(forward, forwardInput), Scale(right, rightInput));
    const float movementLength = LengthXZ(movement);
    if (movementLength > 1.0f)
        movement = Scale(movement, 1.0f / movementLength);

    const bool sprint = PlagC::Input::IsKeyPressed(PlagC::KeyCode::LeftShift) ||
                        PlagC::Input::IsKeyPressed(PlagC::KeyCode::RightShift);
    movement = Scale(movement, sprint ? 5.5f : 3.0f);

    physics_->SetCharacterInput(character_, movement, jumpQueued_);
    jumpQueued_ = false;
    engine.timings.physicsInputMs = Milliseconds(t);

    t = Clock::now();
    physics_->Advance(dt);
    engine.timings.physicsMs = Milliseconds(t);

    t = Clock::now();
    auto character = physics_->ReadCharacter(character_);
    if (character.position.y < -3.0f)
    {
        physics_->DestroyCharacter(character_);
        character_ = physics_->CreateCharacter(PlayerCharacterDesc(PlayerSpawn));
        character = physics_->ReadCharacter(character_);
    }
    engine.timings.physicsReadbackMs = Milliseconds(t);

    t = Clock::now();
    if (LengthXZ(movement) > 0.05f)
    {
        // Joel's source model is authored with its visual forward along +X.
        const float desiredYaw = std::atan2(-movement.z, movement.x) / Rad;
        playerYaw_ += std::remainder(desiredYaw - playerYaw_, 360.0f) *
                      (1.0f - std::exp(-float(dt) * 14.0f));
    }
    world_->SetTransform(player_,
                         {{character.position.x, character.position.y, character.position.z},
                          {0, playerYaw_, 0}, {JoelScale, JoelScale, JoelScale}});
    engine.timings.characterWorldMs = Milliseconds(t);

    t = Clock::now();
    const PlagC::Vec3 target{character.position.x, character.position.y + 0.45f,
                             character.position.z};
    const float yaw = cameraYaw_ * Rad;
    const float pitch = cameraPitch_ * Rad;
    const float cp = std::cos(pitch);
    const PlagC::Vec3 lookDirection{std::sin(yaw) * cp, std::sin(pitch), -std::cos(yaw) * cp};
    const PlagC::Vec3 shoulderRight{std::cos(yaw), 0, std::sin(yaw)};
    const PlagC::Vec3 shoulder = Add(target, Scale(shoulderRight, 0.12f));
    const PlagC::Vec3 desiredCamera = Add(shoulder, Scale(lookDirection, -4.2f));
    world_->SetTransform(camera_, {desiredCamera, {}, {1, 1, 1}});
    auto nextCamera = world_->Read(camera_).camera.value();
    nextCamera.target = target;
    world_->SetCamera(camera_, nextCamera);
    engine.timings.cameraMs = Milliseconds(t);

    t = Clock::now();
    // No AnimatorComponent in calibration mode: render the bind pose only.
    assetScene_->Update(*world_, dt);
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
