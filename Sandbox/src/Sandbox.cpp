#include "pgcpch.h"
#include <PlagC/Application.h>
#include <PlagC/Debug/FpsOverlay.h>
#include <cmath>

namespace
{
class Sandbox final : public PlagC::Application
{
  public:
    void OnStart(PlagC::Engine &engine) override
    {
        if (auto *window = engine.MainWindow())
            window->SetTitle("PlagC Sandbox | Engine Smoke");

        engine.SetFPSLocked(false);
        auto &renderer = engine.Graphics();

        PlagC::MaterialDesc material;
        material.colour = {.18f, .58f, .92f};
        material.roughness = .35f;
        material.metallic = .1f;

        mesh_ = renderer.CreateMesh(PlagC::MakePrimitive(PlagC::Primitive::Cube));
        material_ = renderer.CreateMaterial(material);
        object_ = renderer.CreateObject(mesh_, material_);

        PlagC::LightDesc light;
        light.type = PlagC::LightType::Directional;
        light.direction = {-1.0f, -1.5f, -1.0f};
        light.power = 3.0f;
        light_ = renderer.CreateLight(light);

        camera_.position = {4.0f, 3.0f, 6.0f};
        camera_.target = {0.0f, 0.0f, 0.0f};
        camera_.fov = 60.0f;
        renderer.SetCamera(camera_);

        auto settings = renderer.Settings();
        settings.background = {.025f, .035f, .055f};
        settings.hdr = false;
        settings.bloom = false;
        renderer.SetSettings(settings);

        escape_ = engine.Events().Subscribe(
            PlagC::EventType::KeyPressed,
            [&engine](const PlagC::Event &event) {
                if (event.key == PlagC::KeyCode::Escape)
                    engine.Stop();
            });

        engine.Logger().Write(
            PlagC::Severity::Info, "SANDBOX",
            "Generic engine smoke scene started; close the window or press Escape to exit.");
    }

    void OnFrame(PlagC::Engine &engine, double dt) override
    {
        if (!std::isfinite(dt) || dt < 0.0)
            return;

        angle_ = std::fmod(angle_ + dt * 35.0, 360.0);
        engine.Graphics().SetTransform(
            object_,
            {{0.0f, 0.0f, 0.0f},
             {static_cast<float>(angle_ * .35), static_cast<float>(angle_), 0.0f},
             {1.0f, 1.0f, 1.0f}});

        float aspect = 16.0f / 9.0f;
        if (auto *window = engine.MainWindow())
        {
            if (window->Height() > 0)
                aspect = static_cast<float>(window->Width()) /
                         static_cast<float>(window->Height());
        }
        camera_.aspect = aspect;
        engine.Graphics().SetCamera(camera_);
        if (engine.Graphics().Backend() != PlagC::RendererBackend::Null)
            overlay_.Frame(engine.Graphics(), camera_, dt, aspect);
    }

    void OnStop(PlagC::Engine &engine) override
    {
        escape_.Reset();
        overlay_.Clear(engine.Graphics());

        if (light_)
            engine.Graphics().DestroyLight(light_);
        if (object_)
            engine.Graphics().DestroyObject(object_);
        if (material_)
            engine.Graphics().DestroyMaterial(material_);
        if (mesh_)
            engine.Graphics().DestroyMesh(mesh_);

        light_ = {};
        object_ = {};
        material_ = {};
        mesh_ = {};
    }

  private:
    PlagC::MeshHandle mesh_{};
    PlagC::MaterialHandle material_{};
    PlagC::ObjectHandle object_{};
    PlagC::LightHandle light_{};
    PlagC::CameraDesc camera_{};
    PlagC::FpsOverlay overlay_;
    PlagC::EventBus::Connection escape_;
    double angle_ = 0.0;
};
} // namespace

std::unique_ptr<PlagC::Application>
PlagC::CreateApplication(const PlagC::EngineConfig &)
{
    return std::make_unique<Sandbox>();
}
