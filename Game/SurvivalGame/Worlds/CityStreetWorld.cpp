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
constexpr float Pi = 3.14159265358979323846f;
constexpr float Rad = Pi / 180.0f;

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
    auto joel = engine.Assets().Get<PlagC::ModelAsset>(
        "Game://Characters/Player/Joel/Joel.plasset");
    auto jeep = engine.Assets().Get<PlagC::ModelAsset>(
        "Game://Environment/Props/Jeep/Jeep.plasset");

    if (!cityStreet || !joel || !jeep)
        throw std::runtime_error("CityStreetWorld required assets are not ready");

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

    // Physics baseline for the playable slice. Environment mesh collision will replace
    // this broad street collider once authored collision extraction is introduced.
    auto streetCollider = world_->Create("CityStreet Collision");
    world_->SetTransform(streetCollider, {{0, -0.25f, 0}, {}, {1, 1, 1}});
    PlagC::PhysicsComponent streetPhysics;
    streetPhysics.body.motion = PlagC::MotionType::Static;
    streetPhysics.body.layer = PlagC::CollisionLayer::StaticWorld;
    streetPhysics.collider.type = PlagC::ColliderType::Box;
    streetPhysics.collider.dimensions = {11.5f, 0.25f, 14.0f};
    world_->SetPhysics(streetCollider, streetPhysics);

    player_ = ModelEntity(*world_, "Joel", joel,
                          {{0.0f, 0.20f, 4.0f}, {0, playerYaw_, 0}, {1, 1, 1}});

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

    key_ = engine.Events().Subscribe(PlagC::EventType::KeyPressed, [this, &engine](const PlagC::Event &event) {
        if (event.key == PlagC::KeyCode::Space)
            jumpQueued_ = true;
        else if (event.key == PlagC::KeyCode::Escape)
        {
            cursorCaptured_ = !cursorCaptured_;
            if (auto *window = engine.MainWindow())
                window->SetCursorCaptured(cursorCaptured_);
        }
    });
    focus_ = engine.Events().Subscribe(PlagC::EventType::Focus, [this, &engine](const PlagC::Event &event) {
        if (!event.code)
        {
            cursorCaptured_ = false;
            if (auto *window = engine.MainWindow())
                window->SetCursorCaptured(false);
        }
    });
    mouse_ = engine.Events().Subscribe(PlagC::EventType::MousePressed, [this, &engine](const PlagC::Event &event) {
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
        window->SetTitle("PlagC | Survival Game | City Street");
        window->SetCursorCaptured(true);
    }

    assetScene_->Update(*world_, 0.0);
    sceneRenderer_->Sync(*world_);

    engine.Logger().Write(PlagC::Severity::Info, "WORLD",
                          "CityStreet playable: WASD move, Shift sprint, Space jump, mouse orbit, Escape cursor");
}

void CityStreetWorld::Frame(PlagC::Engine &engine, double dt)
{
    if (!world_ || !physics_ || !assetScene_ || !sceneRenderer_ || !character_)
        throw std::logic_error("CityStreetWorld frame before Start");
    if (!std::isfinite(dt) || dt < 0)
        throw std::invalid_argument("invalid CityStreetWorld timestep");

    dt = std::min(dt, 0.1);
    auto t = Clock::now();

    if (cursorCaptured_)
    {
        const auto mouse = PlagC::Input::GetMouseDelta();
        cameraYaw_ += float(mouse.x) * 0.12f;
        cameraPitch_ = std::clamp(cameraPitch_ - float(mouse.y) * 0.10f, -25.0f, 55.0f);
    }

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

    const bool sprint = PlagC::Input::IsKeyPressed(PlagC::KeyCode::LeftShift) ||
                        PlagC::Input::IsKeyPressed(PlagC::KeyCode::RightShift);
    movement = Scale(movement, sprint ? 6.0f : 3.3f);

    physics_->SetCharacterInput(character_, movement, jumpQueued_);
    jumpQueued_ = false;
    engine.timings.physicsInputMs = Milliseconds(t);

    t = Clock::now();
    physics_->Advance(dt);
    engine.timings.physicsMs = Milliseconds(t);

    t = Clock::now();
    const auto character = physics_->ReadCharacter(character_);
    engine.timings.physicsReadbackMs = Milliseconds(t);

    t = Clock::now();
    if (LengthXZ(movement) > 0.05f)
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
    const PlagC::Vec3 target{character.position.x, character.position.y + 0.55f,
                             character.position.z};
    const float pitch = cameraPitch_ * Rad;
    const float cp = std::cos(pitch);
    const PlagC::Vec3 lookDirection{std::sin(yaw) * cp, std::sin(pitch), -std::cos(yaw) * cp};
    const float distance = 4.8f;
    const PlagC::Vec3 cameraPosition = Add(target, Scale(lookDirection, -distance));
    world_->SetTransform(camera_, {cameraPosition, {}, {1, 1, 1}});
    auto cameraComponent = world_->Read(camera_).camera.value();
    cameraComponent.target = target;
    world_->SetCamera(camera_, cameraComponent);
    engine.timings.cameraMs = Milliseconds(t);

    t = Clock::now();
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

    environment_ = {};
    player_ = {};
    jeep_ = {};
    grass_ = {};
    vegetation_ = {};
    camera_ = {};
    sun_ = {};

    engine.Logger().Write(PlagC::Severity::Info, "WORLD", "CityStreetWorld stopped");
}
} // namespace PlagCGame
