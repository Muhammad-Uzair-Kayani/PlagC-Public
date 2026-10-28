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
constexpr float PlayerVisualScale = 100.0f;
constexpr float TerrainHalfExtent = 29.0f;
constexpr PlagC::Vec3 InitialPlayerPosition{0.0f, 0.0f, 0.0f};

float LengthXZ(PlagC::Vec3 v)
{
    return std::sqrt(v.x * v.x + v.z * v.z);
}

float TerrainHeight(float x, float z)
{
    const float r = std::max(std::abs(x), std::abs(z));
    if (r <= 12.0f)
        return 0.0f;

    float t = std::clamp((r - 12.0f) / 12.0f, 0.0f, 1.0f);
    t = t * t * (3.0f - 2.0f * t);
    const float wave = 0.30f * std::sin(x * 0.17f) +
                       0.22f * std::cos(z * 0.21f) +
                       0.12f * std::sin((x + z) * 0.11f);
    return t * wave;
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
    auto terrain = engine.Assets().Get<PlagC::ModelAsset>(
        "Game://Environment/Terrain/OpenTerrain/OpenTerrain.plasset");
    auto joker = engine.Assets().Get<PlagC::ModelAsset>(
        "Game://Characters/Player/Joker/Joker.plasset");
    auto grass = engine.Assets().Get<PlagC::ModelAsset>(
        "Game://Environment/Vegetation/GrassPatches/GrassPatches.plasset");
    auto vegetation = engine.Assets().Get<PlagC::ModelAsset>(
        "Game://Environment/Vegetation/GroundVegetation/GroundVegetation.plasset");
    auto jeep = engine.Assets().Get<PlagC::ModelAsset>(
        "Game://Environment/Props/Jeep/Jeep.plasset");

    if (!terrain || !joker || !grass || !vegetation || !jeep)
        throw std::runtime_error("OpenTerrain test required assets are not ready");
    if (joker->animations.empty())
        throw std::runtime_error("Joker reference character contains no native animations");

    playerAsset_ = joker;
    requestedClip_ = 0;
    activeClip_ = ~0u;
    playerPosition_ = InitialPlayerPosition;
    playerYaw_ = 180.0f;

    world_ = std::make_unique<PlagC::World>();
    assetScene_ = std::make_unique<PlagC::AssetScene>(engine.Graphics());
    sceneRenderer_ = std::make_unique<PlagC::SceneRenderer>(engine.Graphics());
#ifdef PC_ENABLE_PROFILING
    sceneRenderer_->SetProfiling(true);
#endif

    environment_ = ModelEntity(*world_, "Open Terrain", terrain,
                               {{0, 0, 0}, {0, 0, 0}, {1, 1, 1}});

    player_ = ModelEntity(*world_, "Joker", playerAsset_,
                          {playerPosition_, {0, playerYaw_, 0},
                           {PlayerVisualScale, PlayerVisualScale, PlayerVisualScale}});
    PlagC::AnimatorComponent animator;
    animator.state = {requestedClip_, 0, 1, true, true};
    world_->SetAnimator(player_, animator);

    // Keep the middle 24x24m completely clear. Vegetation is intentionally placed
    // around the outer ring so spawn/movement testing always starts unobstructed.
    const PlagC::Vec3 vegetationSpots[] = {
        {-18.0f, 0.0f, -18.0f}, {-8.0f, 0.0f, -20.0f}, {8.0f, 0.0f, -20.0f},
        {18.0f, 0.0f, -18.0f}, {-20.0f, 0.0f, -7.0f}, {20.0f, 0.0f, -6.0f},
        {-20.0f, 0.0f, 8.0f}, {20.0f, 0.0f, 8.0f}, {-17.0f, 0.0f, 18.0f},
        {-7.0f, 0.0f, 20.0f}, {8.0f, 0.0f, 20.0f}, {18.0f, 0.0f, 17.0f}};

    for (unsigned i = 0; i < std::size(vegetationSpots); ++i)
    {
        auto p = vegetationSpots[i];
        p.y = TerrainHeight(p.x, p.z);
        const bool useGrass = (i % 2) == 0;
        auto asset = useGrass ? grass : vegetation;
        const float scale = useGrass ? 0.22f : 0.85f;
        ModelEntity(*world_, useGrass ? "Grass Patch" : "Ground Vegetation", asset,
                    {p, {0, float((i * 37) % 360), 0}, {scale, scale, scale}});
    }

    jeep_ = ModelEntity(*world_, "Jeep", jeep,
                        {{-15.0f, TerrainHeight(-15.0f, 10.0f) + 0.55f, 10.0f},
                         {0, 28, 0}, {1, 1, 1}});

    sun_ = world_->Create("Sun");
    world_->SetTransform(sun_, {{0, 12, 0}, {}, {1, 1, 1}});
    PlagC::LightDesc sun;
    sun.type = PlagC::LightType::Directional;
    sun.direction = {-0.42f, -0.86f, -0.28f};
    sun.colour = {1.0f, 0.95f, 0.86f};
    sun.power = 4.4f;
    world_->SetLight(sun_, {sun});

    camera_ = world_->Create("Gameplay Camera");
    world_->SetTransform(camera_, {{0.0f, 2.2f, 5.0f}, {}, {1, 1, 1}});
    PlagC::CameraComponent camera;
    camera.target = {playerPosition_.x, playerPosition_.y + 1.0f, playerPosition_.z};
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
    settings.fogDensity = 0.0010f;
    settings.fogColour = {0.32f, 0.37f, 0.40f};
    settings.background = {0.07f, 0.10f, 0.13f};
    engine.Graphics().SetSettings(settings);

    engine.SetFPSLocked(false);
    if (auto *window = engine.MainWindow())
    {
        window->SetTitle("PlagC | Survival Game | Open Terrain Test");
        window->SetCursorCaptured(true);
    }

    assetScene_->Update(*world_, 0.0);
    sceneRenderer_->Sync(*world_);
    activeClip_ = requestedClip_;

    engine.Logger().Write(PlagC::Severity::Info, "WORLD",
                          "OpenTerrain ready: 60x60m terrain, 24x24m clear spawn zone, WASD movement");
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
    }

    const float yaw = cameraYaw_ * Rad;
    const PlagC::Vec3 forward{std::sin(yaw), 0.0f, -std::cos(yaw)};
    const PlagC::Vec3 right{std::cos(yaw), 0.0f, std::sin(yaw)};

    float f = 0.0f;
    float r = 0.0f;
    if (PlagC::Input::IsKeyPressed(PlagC::KeyCode::W)) f += 1.0f;
    if (PlagC::Input::IsKeyPressed(PlagC::KeyCode::S)) f -= 1.0f;
    if (PlagC::Input::IsKeyPressed(PlagC::KeyCode::D)) r += 1.0f;
    if (PlagC::Input::IsKeyPressed(PlagC::KeyCode::A)) r -= 1.0f;

    PlagC::Vec3 move{forward.x * f + right.x * r, 0.0f,
                     forward.z * f + right.z * r};
    const float moveLength = LengthXZ(move);
    if (moveLength > 1.0f)
    {
        move.x /= moveLength;
        move.z /= moveLength;
    }

    if (moveLength > 0.001f)
    {
        const bool fast = PlagC::Input::IsKeyPressed(PlagC::KeyCode::LeftShift) ||
                          PlagC::Input::IsKeyPressed(PlagC::KeyCode::RightShift);
        const float speed = fast ? 7.0f : 3.0f;
        playerPosition_.x = std::clamp(playerPosition_.x + move.x * speed * float(dt),
                                       -TerrainHalfExtent, TerrainHalfExtent);
        playerPosition_.z = std::clamp(playerPosition_.z + move.z * speed * float(dt),
                                       -TerrainHalfExtent, TerrainHalfExtent);

        const float desiredYaw = std::atan2(move.x, move.z) / Rad;
        playerYaw_ += std::remainder(desiredYaw - playerYaw_, 360.0f) *
                      (1.0f - std::exp(-float(dt) * 12.0f));
    }

    playerPosition_.y = TerrainHeight(playerPosition_.x, playerPosition_.z);
    world_->SetTransform(player_,
                         {playerPosition_, {0, playerYaw_, 0},
                          {PlayerVisualScale, PlayerVisualScale, PlayerVisualScale}});

    const float pitch = cameraPitch_ * Rad;
    const float cp = std::cos(pitch);
    const PlagC::Vec3 lookDirection{std::sin(yaw) * cp, std::sin(pitch), -std::cos(yaw) * cp};
    const PlagC::Vec3 target{playerPosition_.x, playerPosition_.y + 1.0f, playerPosition_.z};
    const float distance = 4.0f;
    const PlagC::Vec3 cameraPosition{target.x - lookDirection.x * distance,
                                     target.y - lookDirection.y * distance,
                                     target.z - lookDirection.z * distance};
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

    engine.Logger().Write(PlagC::Severity::Info, "WORLD", "OpenTerrain test stopped");
}
} // namespace PlagCGame
