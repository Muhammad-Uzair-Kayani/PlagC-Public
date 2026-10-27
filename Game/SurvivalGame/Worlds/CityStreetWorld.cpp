#include "pgcpch.h"
#include "CityStreetWorld.h"
#include <PlagC/Assets/AssetManager.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>

namespace PlagCGame
{
namespace
{
using Clock = std::chrono::steady_clock;

double Milliseconds(Clock::time_point start)
{
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
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
    assetScene_ = std::make_unique<PlagC::AssetScene>(engine.Graphics());
    sceneRenderer_ = std::make_unique<PlagC::SceneRenderer>(engine.Graphics());
#ifdef PC_ENABLE_PROFILING
    sceneRenderer_->SetProfiling(true);
#endif

    // The authored CityStreet asset is approximately 23 x 5.5 x 28 metres and
    // already uses PlagC's Y-up/metre convention after cooking.
    environment_ = ModelEntity(*world_, "CityStreet Environment", cityStreet,
                               {{0, 0, 0}, {0, 0, 0}, {1, 1, 1}});

    // Joel's cooked rig is centred around the origin and is approximately 2.5 m tall.
    // Raising the entity by half-height puts the feet near the street surface.
    player_ = ModelEntity(*world_, "Joel", joel,
                          {{0.0f, 1.25f, 4.0f}, {0, 180, 0}, {1, 1, 1}});

    // First authored prop placement. More world dressing will move to world data later.
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
    world_->SetTransform(camera_, {{8.5f, 5.3f, 12.5f}, {}, {1, 1, 1}});
    PlagC::CameraComponent camera;
    camera.target = {0.0f, 1.4f, 2.2f};
    camera.fov = 58.0f;
    camera.nearClip = 0.08f;
    camera.farClip = 350.0f;
    world_->SetCamera(camera_, camera);
    world_->SetActiveCamera(camera_);

    auto settings = engine.Graphics().Settings();
    settings.hdr = true;
    settings.bloom = true;
    settings.exposure = 1.0f;
    settings.fogDensity = 0.0015f;
    settings.fogColour = {0.40f, 0.44f, 0.46f};
    settings.background = {0.08f, 0.10f, 0.12f};
    engine.Graphics().SetSettings(settings);

    if (auto *window = engine.MainWindow())
        window->SetTitle("PlagC | Survival Game | City Street");

    // Build renderer-side resources immediately so the first presented frame already
    // contains the selected world rather than showing an empty bootstrap frame.
    assetScene_->Update(*world_, 0.0);
    sceneRenderer_->Sync(*world_);

    engine.Logger().Write(PlagC::Severity::Info, "WORLD",
                          "CityStreetWorld ready: environment + Joel + Jeep + camera + sun");
}

void CityStreetWorld::Frame(PlagC::Engine &engine, double dt)
{
    if (!world_ || !assetScene_ || !sceneRenderer_)
        throw std::logic_error("CityStreetWorld frame before Start");
    if (!std::isfinite(dt) || dt < 0)
        throw std::invalid_argument("invalid CityStreetWorld timestep");

    dt = std::min(dt, 0.1);

    auto start = Clock::now();
    assetScene_->Update(*world_, dt);
    engine.timings.animationMs = Milliseconds(start);

    start = Clock::now();
    sceneRenderer_->Sync(*world_);
    engine.timings.syncMs = Milliseconds(start);

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
    if (sceneRenderer_)
        sceneRenderer_->Clear();
    if (assetScene_)
        assetScene_->Clear();

    sceneRenderer_.reset();
    assetScene_.reset();
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
