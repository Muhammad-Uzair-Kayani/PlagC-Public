#include "pgcpch.h"
#include "CityScaleLabWorld.h"
#include <PlagC/Assets/AssetManager.h>
#include <PlagC/Input/Input.h>
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <stdexcept>

namespace PlagCGame
{
namespace
{
constexpr float Pi = 3.14159265358979323846f;
constexpr float Rad = Pi / 180.0f;

float WrapDegrees(float value)
{
    while (value > 180.0f)
        value -= 360.0f;
    while (value < -180.0f)
        value += 360.0f;
    return value;
}
} // namespace

void CityScaleLabWorld::ApplyCityTransform()
{
    if (world_ && city_)
        world_->SetTransform(city_, {{0.0f, cityYOffset_, 0.0f}, {},
                                     {cityScale_, cityScale_, cityScale_}});
}

void CityScaleLabWorld::ResetCamera()
{
    cameraPosition_ = {referencePosition_.x, 2.25f, referencePosition_.z - 5.0f};
    cameraYaw_ = 0.0f;
    cameraPitch_ = 8.0f;
}

void CityScaleLabWorld::Start(PlagC::Engine &engine)
{
    if (!engine.Assets().HasMount("Game"))
        engine.Assets().Mount("Game", "GameAssets/Default/Runtime");

    cityAsset_ = engine.Assets().Load<PlagC::ModelAsset>(
        "Game://Environment/City/City.plasset");
    characterAsset_ = engine.Assets().Load<PlagC::ModelAsset>(
        "Game://Characters/BlackDragon/BlackDragon.plasset");
    if (!cityAsset_ || cityAsset_->meshes.size() != 100 || cityAsset_->skeleton.joints.size() != 1)
        throw std::runtime_error("CityScaleLab requires flattened City.plasset (100 material meshes, one root)");
    if (!characterAsset_)
        throw std::runtime_error("CityScaleLab requires BlackDragon.plasset as the 1.70m scale reference");

    world_ = std::make_unique<PlagC::World>();
    assetScene_ = std::make_unique<PlagC::AssetScene>(engine.Graphics());
    sceneRenderer_ = std::make_unique<PlagC::SceneRenderer>(engine.Graphics());
#ifdef PC_ENABLE_PROFILING
    sceneRenderer_->SetProfiling(true);
#endif

    city_ = world_->Create("City | 10x source calibration | metres");
    world_->SetModel(city_, {cityAsset_, true});
    ApplyCityTransform();

    // Static BlackDragon bind/rest model only. UAL is intentionally excluded from this
    // phase so the unresolved retargeted spine pose cannot bias scale judgement.
    referenceCharacter_ = world_->Create("BlackDragon | 1.70m reference");
    world_->SetTransform(referenceCharacter_,
                         {referencePosition_, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}});
    world_->SetModel(referenceCharacter_, {characterAsset_, true});

    sun_ = world_->Create("City Scale Sun");
    world_->SetTransform(sun_, {{0.0f, 25.0f, 0.0f}, {}, {1.0f, 1.0f, 1.0f}});
    PlagC::LightDesc sun;
    sun.type = PlagC::LightType::Directional;
    sun.direction = {-0.32f, -0.88f, -0.34f};
    sun.colour = {1.0f, 0.96f, 0.90f};
    sun.power = 3.5f;
    world_->SetLight(sun_, {sun});

    fill_ = world_->Create("City Scale Fill");
    world_->SetTransform(fill_, {{referencePosition_.x - 3.0f, 4.0f, referencePosition_.z - 2.0f},
                                 {}, {1.0f, 1.0f, 1.0f}});
    PlagC::LightDesc fill;
    fill.type = PlagC::LightType::Point;
    fill.colour = {0.45f, 0.58f, 1.0f};
    fill.power = 8.0f;
    fill.radius = 12.0f;
    world_->SetLight(fill_, {fill});

    ResetCamera();
    camera_ = world_->Create("City Scale Free Camera");
    world_->SetTransform(camera_, {cameraPosition_, {}, {1.0f, 1.0f, 1.0f}});
    PlagC::CameraComponent camera;
    camera.target = {referencePosition_.x, 1.0f, referencePosition_.z};
    camera.fov = 58.0f;
    camera.nearClip = 0.05f;
    camera.farClip = 600.0f;
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
            return;
        }

        bool cityChanged = false;
        if (event.key == PlagC::KeyCode::PageUp)
        {
            cityScale_ = std::min(1.50f, cityScale_ + 0.05f);
            cityChanged = true;
        }
        else if (event.key == PlagC::KeyCode::PageDown)
        {
            cityScale_ = std::max(0.50f, cityScale_ - 0.05f);
            cityChanged = true;
        }
        else if (event.key == PlagC::KeyCode::Up)
        {
            cityYOffset_ += 0.10f;
            cityChanged = true;
        }
        else if (event.key == PlagC::KeyCode::Down)
        {
            cityYOffset_ -= 0.10f;
            cityChanged = true;
        }
        else if (event.key == PlagC::KeyCode::Home)
        {
            cityScale_ = 1.0f;
            cityYOffset_ = 0.0f;
            cityChanged = true;
        }
        else if (event.key == PlagC::KeyCode::R)
        {
            ResetCamera();
        }

        if (cityChanged)
        {
            ApplyCityTransform();
            std::ostringstream line;
            line << std::fixed << std::setprecision(2)
                 << "City calibration | runtime scale=" << cityScale_
                 << " | Y offset=" << cityYOffset_ << "m"
                 << " | effective source conversion=" << cityScale_ * 10.0f << "x";
            engine.Logger().Write(PlagC::Severity::Info, "CITY", line.str());
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
        window->SetTitle("PlagC | City Scale Lab | 1.70m BlackDragon Reference");
        window->SetCursorCaptured(true);
    }

    assetScene_->Update(*world_, 0.0);
    sceneRenderer_->Sync(*world_);

    engine.Logger().Write(
        PlagC::Severity::Info, "CITY",
        "CityScaleLab | baked city 146.53m x 239.00m x 21.64m | street reference Y=0 | 204218 triangles | 100 material-merged meshes");
    engine.Logger().Write(
        PlagC::Severity::Info, "CITY",
        "Controls: mouse look, WASD free camera, Q/E down/up, Shift fast, PageUp/PageDown city +/-5%, Up/Down city Y +/-0.10m, Home reset calibration, R reset camera, Esc cursor");
}

void CityScaleLabWorld::Frame(PlagC::Engine &engine, double dt)
{
    if (!world_ || !assetScene_ || !sceneRenderer_ || !cityAsset_ || !characterAsset_)
        throw std::logic_error("CityScaleLab frame before Start");
    if (!std::isfinite(dt) || dt < 0.0)
        throw std::invalid_argument("invalid CityScaleLab timestep");
    dt = std::min(dt, 0.1);

    if (cursorCaptured_)
    {
        const auto mouse = PlagC::Input::GetMouseDelta();
        cameraYaw_ = WrapDegrees(cameraYaw_ - float(mouse.x) * 0.12f);
        cameraPitch_ = std::clamp(cameraPitch_ - float(mouse.y) * 0.10f, -80.0f, 80.0f);
    }

    const float yaw = cameraYaw_ * Rad;
    const float pitch = cameraPitch_ * Rad;
    const float cp = std::cos(pitch);
    const PlagC::Vec3 viewForward{std::sin(yaw) * cp, std::sin(pitch), std::cos(yaw) * cp};
    const PlagC::Vec3 flatForward{std::sin(yaw), 0.0f, std::cos(yaw)};
    const PlagC::Vec3 right{-std::cos(yaw), 0.0f, std::sin(yaw)};

    const float inputX = float(PlagC::Input::IsKeyPressed(PlagC::KeyCode::D)) -
                         float(PlagC::Input::IsKeyPressed(PlagC::KeyCode::A));
    const float inputZ = float(PlagC::Input::IsKeyPressed(PlagC::KeyCode::W)) -
                         float(PlagC::Input::IsKeyPressed(PlagC::KeyCode::S));
    const float inputY = float(PlagC::Input::IsKeyPressed(PlagC::KeyCode::E)) -
                         float(PlagC::Input::IsKeyPressed(PlagC::KeyCode::Q));
    float mx = right.x * inputX + flatForward.x * inputZ;
    float mz = right.z * inputX + flatForward.z * inputZ;
    const float planarLength = std::sqrt(mx * mx + mz * mz);
    if (planarLength > 1.0f)
    {
        mx /= planarLength;
        mz /= planarLength;
    }
    const bool fast = PlagC::Input::IsKeyPressed(PlagC::KeyCode::LeftShift) ||
                      PlagC::Input::IsKeyPressed(PlagC::KeyCode::RightShift);
    const float speed = fast ? 24.0f : 7.0f;
    cameraPosition_.x += mx * speed * float(dt);
    cameraPosition_.z += mz * speed * float(dt);
    cameraPosition_.y += inputY * speed * float(dt);
    cameraPosition_.y = std::clamp(cameraPosition_.y, -1.0f, 80.0f);

    world_->SetTransform(camera_, {cameraPosition_, {}, {1.0f, 1.0f, 1.0f}});
    auto camera = world_->Read(camera_).camera.value();
    camera.target = {cameraPosition_.x + viewForward.x * 8.0f,
                     cameraPosition_.y + viewForward.y * 8.0f,
                     cameraPosition_.z + viewForward.z * 8.0f};
    world_->SetCamera(camera_, camera);

    // Both assets are static in this calibration phase; Update still owns resource lifetime.
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

void CityScaleLabWorld::Stop(PlagC::Engine &engine)
{
    key_.Reset();
    focus_.Reset();
    mouse_.Reset();
    if (auto *window = engine.MainWindow())
        window->SetCursorCaptured(false);

    sceneRenderer_.reset();
    assetScene_.reset();
    world_.reset();
    cityAsset_ = {};
    characterAsset_ = {};
}
} // namespace PlagCGame
