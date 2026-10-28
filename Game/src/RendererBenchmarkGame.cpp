#include "pgcpch.h"
#include "RendererBenchmarkGame.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace PlagCGame
{
namespace
{
unsigned ReadCount(const char *name, unsigned fallback, unsigned minimum, unsigned maximum)
{
    const char *text = std::getenv(name);
    if (!text || !*text)
        return fallback;
    char *end = nullptr;
    const unsigned long parsed = std::strtoul(text, &end, 10);
    if (end == text || *end != '\0' || parsed < minimum || parsed > maximum)
        throw std::invalid_argument(std::string(name) + " must be between " +
                                    std::to_string(minimum) + " and " +
                                    std::to_string(maximum));
    return static_cast<unsigned>(parsed);
}

float Channel(unsigned index, unsigned multiplier, unsigned add)
{
    return 0.15f + 0.80f *
                       (static_cast<float>((index * multiplier + add) % 97u) / 96.0f);
}
} // namespace

void RendererBenchmarkGame::Start(PlagC::Engine &engine, const PlagC::EngineConfig &,
                                  std::string_view world)
{
    if (world != DefaultWorld())
        throw std::invalid_argument("Renderer benchmark does not provide world: " +
                                    std::string(world));
    if (started_)
        throw std::logic_error("Renderer benchmark already started");

    auto &renderer = engine.Graphics();
    const unsigned objectCount = ReadCount("PC_RENDER_BENCH_OBJECTS", 2500u, 128u, 8000u);
    const unsigned materialCount = std::min(
        objectCount, ReadCount("PC_RENDER_BENCH_MATERIALS", objectCount, 1u, 8000u));

    previousFpsLocked_ = engine.IsFPSLocked();
    previousSettings_ = renderer.Settings();
    previousEnvironment_ = renderer.Environment();
    started_ = true;

    try
    {
        // Deliberately CPU/driver bound: remove expensive GPU features and all gameplay CPU
        // so backend submission cost is not hidden by shadows, post, animation or physics.
        PlagC::RenderSettings settings{};
        settings.vsync = false;
        settings.shadows = false;
        settings.hdr = false;
        settings.bloom = false;
        settings.fogDensity = 0.0f;
        settings.background = {0.012f, 0.016f, 0.024f};
        renderer.SetSettings(settings);
        renderer.SetVSync(false);
        renderer.SetEnvironment({"", false, false});
        engine.SetFPSLocked(false);

        const unsigned columns =
            static_cast<unsigned>(std::ceil(std::sqrt(static_cast<double>(objectCount))));
        const unsigned rows = (objectCount + columns - 1u) / columns;
        constexpr float spacing = 0.90f;
        const float halfWidth = 0.5f * static_cast<float>(columns - 1u) * spacing;
        const float halfHeight = 0.5f * static_cast<float>(rows - 1u) * spacing;
        const float largestHalfExtent = std::max(halfWidth, halfHeight);
        const float cameraDistance = std::max(55.0f, largestHalfExtent * 2.20f);

        PlagC::CameraDesc camera{};
        camera.position = {0.0f, 0.0f, cameraDistance};
        camera.target = {0.0f, 0.0f, 0.0f};
        camera.fov = 60.0f;
        camera.nearClip = 0.1f;
        camera.farClip = cameraDistance + 120.0f;
        renderer.SetCamera(camera);

        PlagC::LightDesc light{};
        light.type = PlagC::LightType::Directional;
        light.direction = {-0.35f, -0.75f, -0.55f};
        light.colour = {1.0f, 0.96f, 0.90f};
        light.power = 2.5f;
        light_ = renderer.CreateLight(light);

        materials_.reserve(materialCount);
        for (unsigned i = 0; i < materialCount; ++i)
        {
            PlagC::MaterialDesc material{};
            // Keep a representative PBS subset, but most objects are unlit so fragment shading
            // cannot dominate the driver/API comparison.
            material.model = (i % 4u == 0u) ? PlagC::MaterialModel::Pbr
                                             : PlagC::MaterialModel::Unlit;
            material.colour = {Channel(i, 37u, 7u), Channel(i, 53u, 23u),
                               Channel(i, 71u, 41u)};
            material.roughness = 0.20f + 0.70f * static_cast<float>(i % 11u) / 10.0f;
            material.metallic = static_cast<float>(i % 7u) / 6.0f;
            materials_.push_back(renderer.CreateMaterial(material));
        }

        const PlagC::MeshData cube = PlagC::MakePrimitive(PlagC::Primitive::Cube);
        const PlagC::MeshData cylinder = PlagC::MakePrimitive(PlagC::Primitive::Cylinder);
        meshes_.reserve(objectCount);
        objects_.reserve(objectCount);

        for (unsigned i = 0; i < objectCount; ++i)
        {
            // Every object gets its own mesh/VAO. Together with many materials, this prevents
            // the benchmark from collapsing into a tiny instanced/batched workload.
            const auto &shape = (i % 13u == 0u) ? cylinder : cube;
            const auto mesh = renderer.CreateMesh(shape, false);
            meshes_.push_back(mesh);

            const unsigned column = i % columns;
            const unsigned row = i / columns;
            PlagC::Transform transform{};
            transform.position = {static_cast<float>(column) * spacing - halfWidth,
                                  halfHeight - static_cast<float>(row) * spacing,
                                  -0.015f * static_cast<float>(i % 17u)};
            transform.rotationDegrees = {static_cast<float>((i * 17u) % 360u),
                                         static_cast<float>((i * 31u) % 360u),
                                         static_cast<float>((i * 7u) % 360u)};
            const float scale = (i % 13u == 0u) ? 0.27f : 0.33f;
            transform.scale = {scale, scale, scale};
            objects_.push_back(
                renderer.CreateObject(mesh, materials_[i % materials_.size()], transform));
        }

        engine.Logger().Write(
            PlagC::Severity::Info, "BENCH",
            std::string("Renderer DriverStress ready: backend=") +
                PlagC::BackendName(renderer.Backend()) + " objects=" +
                std::to_string(objectCount) + " uniqueMeshes=" + std::to_string(meshes_.size()) +
                " materials=" + std::to_string(materials_.size()) +
                " | fixed camera, FPS unlocked, VSync/shadows/HDR/bloom/environment disabled. "
                "Compare Renderer submit + Ogre renderOneFrame at equal RENDER STATS.");
    }
    catch (...)
    {
        Stop(engine);
        throw;
    }
}

void RendererBenchmarkGame::Frame(PlagC::Engine &, double)
{
    // Intentionally static. A backend comparison must not contain gameplay/animation CPU noise.
}

void RendererBenchmarkGame::Stop(PlagC::Engine &engine)
{
    if (!started_)
        return;

    auto &renderer = engine.Graphics();
    for (auto it = objects_.rbegin(); it != objects_.rend(); ++it)
        if (*it)
            renderer.DestroyObject(*it);
    objects_.clear();

    if (light_)
    {
        renderer.DestroyLight(light_);
        light_ = {};
    }

    for (auto it = materials_.rbegin(); it != materials_.rend(); ++it)
        if (*it)
            renderer.DestroyMaterial(*it);
    materials_.clear();

    for (auto it = meshes_.rbegin(); it != meshes_.rend(); ++it)
        if (*it)
            renderer.DestroyMesh(*it);
    meshes_.clear();

    renderer.SetEnvironment(previousEnvironment_);
    renderer.SetSettings(previousSettings_);
    renderer.SetVSync(previousSettings_.vsync);
    engine.SetFPSLocked(previousFpsLocked_);
    started_ = false;
}
} // namespace PlagCGame
