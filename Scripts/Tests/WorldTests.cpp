#include "pgcpch.h"
#include <PlagC/World/SceneRenderer.h>
#include <PlagC/Jobs.h>
#include <limits>
using namespace PlagC;
namespace
{
void Check(bool ok, const char *what)
{
    if (!ok) throw std::runtime_error(what);
    std::cout << "[PASS] " << what << '\n';
}
template<class Fn> bool Rejected(Fn fn)
{
    try { fn(); } catch (const std::exception &) { return true; }
    return false;
}
std::vector<char> CaptureWorld(Renderer &r, TargetHandle target, const char *path)
{
    r.Render(0);
    r.CaptureTarget(target, path);
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    auto size = file.tellg();
    if (size < 640 * 360 * 3) throw std::runtime_error("incomplete ECS capture");
    std::vector<char> result(static_cast<std::size_t>(size));
    file.seekg(0);
    file.read(result.data(), std::streamsize(result.size()));
    if (!file) throw std::runtime_error("failed to read ECS capture");
    return result;
}
bool Different(const std::vector<char> &a, const std::vector<char> &b)
{
    if (a.size() != b.size()) return false;
    unsigned changed = 0;
    for (std::size_t i = 0; i < a.size(); ++i)
        changed += std::abs(int(static_cast<unsigned char>(a[i])) -
                            int(static_cast<unsigned char>(b[i]))) > 4;
    return changed > 200;
}
// Jobs touch only distinct elements of this owned buffer. EnTT and Ogre stay on main.
void Rotate(JobSystem &jobs, std::vector<TransformUpdate> &updates, float delta)
{
    struct Chunk { std::vector<TransformUpdate> *updates; std::size_t begin, end; float delta; };
    std::array<Chunk, 4> chunks;
    JobGroup group;
    for (std::size_t i = 0; i < chunks.size(); ++i)
    {
        chunks[i] = {&updates, updates.size() * i / 4, updates.size() * (i + 1) / 4, delta};
        while (true)
        {
            auto result = jobs.TrySubmit(group, [](void *data) {
                auto &c = *static_cast<Chunk *>(data);
                for (auto j = c.begin; j < c.end; ++j)
                    c.updates->at(j).transform.rotationDegrees.y += c.delta;
            }, &chunks[i]);
            if (result == SubmitResult::Accepted) break;
            jobs.Wait(group);
            if (result == SubmitResult::Stopped) throw std::runtime_error("world jobs stopped");
        }
    }
    jobs.Wait(group);
    if (group.failed.load()) throw std::runtime_error("world transform job failed");
}
}
void TestWorld(Renderer &renderer, JobSystem &jobs)
{
    World world, other;
    auto e = world.Create("Original");
    const auto foreign = other.Create("Other world");
    Check(world.Size() == 1 && world.Alive(e) && !world.Alive(foreign), "world scopes entity identities");
    Check(Rejected([&] { world.Destroy(foreign); }) && Rejected([&] { world.Read({}); }),
          "foreign and null entities rejected");
    world.Rename(e, "Renamed");
    Check(world.Read(e).name == "Renamed", "entity names support editor-facing rename");
    auto detached = world.Read(e);
    detached.name = "Detached";
    detached.transform.position.x = 99;
    Check(world.Read(e).name == "Renamed" && world.Read(e).transform.position.x == 0,
          "world reads are independent values");
    auto t = Transform{{2, 3, 4}, {0, 30, 0}, {2, 2, 2}};
    world.SetTransform(e, t);
    auto bad = t; bad.scale.x = 0;
    Check(Rejected([&] { world.SetTransform(e, bad); }) && world.Read(e).transform.scale.x == 2,
          "invalid transform leaves component unchanged");
    bad = t; bad.position.x = std::numeric_limits<float>::quiet_NaN();
    Check(Rejected([&] { world.SetTransform(e, bad); }), "world rejects nonfinite transforms");
    Check(Rejected([&] { world.SetActiveCamera(e); }), "active camera requires camera component");
    world.SetCamera(e, {}); world.SetActiveCamera(e);
    Check(world.ActiveCamera() == e, "world selects active camera");
    auto badCamera = CameraComponent{}; badCamera.farClip = .01f;
    Check(Rejected([&] { world.SetCamera(e, badCamera); }), "world rejects invalid camera lens");
    auto coincident = t; coincident.position = {};
    Check(Rejected([&] { world.SetTransform(e, coincident); }), "camera position cannot equal target");
    world.RemoveCamera(e);
    Check(!world.ActiveCamera() && !world.Read(e).camera, "removing active camera clears selection");
    world.SetLight(e, {});
    auto badLight = LightComponent{}; badLight.light.direction = {};
    Check(Rejected([&] { world.SetLight(e, badLight); }), "world validates light direction");
    world.RemoveLight(e); world.RemoveLight(e);
    Check(!world.Read(e).light, "optional component removal is idempotent");
    Check(Rejected([&] { world.SetRenderable(e, {}); }), "world rejects empty render resource handles");
    std::atomic<unsigned> denied{0};
    std::thread worker([&] {
        denied += Rejected([&] { world.Create(); });
        denied += Rejected([&] { world.Snapshot(); });
        denied += Rejected([&] { world.SetTransform(e, t); });
    });
    worker.join();
    Check(denied == 3, "world rejects worker-thread reads and mutations");
    world.Destroy(e);
    auto replacement = world.Create();
    Check(replacement != e && !world.Alive(e) && Rejected([&] { world.Read(e); }),
          "destroy and EnTT slot reuse never revive public entity handles");
    world.Clear();
    Check(!world.Size() && !world.Alive(replacement), "world clear invalidates all entities");
    for (int i = 0; i < 256; ++i) world.Create("Job entity");
    auto transforms = world.TransformSnapshot();
    Rotate(jobs, transforms, 15);
    Check(world.Read(transforms[0].entity).transform.rotationDegrees.y == 0,
          "worker snapshots do not mutate live ECS storage");
    world.ApplyTransforms(transforms);
    bool rotated = true;
    for (auto &v : world.TransformSnapshot()) rotated &= v.transform.rotationDegrees.y == 15;
    Check(rotated, "job-produced transform batch updates every entity exactly once");
    auto invalid = transforms;
    invalid[0].transform.position.x = 10;
    invalid.back().entity = e;
    Check(Rejected([&] { world.ApplyTransforms(invalid); }) &&
              world.Read(transforms[0].entity).transform.position.x == 0,
          "stale entity rejects the whole transform batch without partial writes");
    invalid = transforms; invalid.back().entity = invalid.front().entity;
    Check(Rejected([&] { world.ApplyTransforms(invalid); }), "duplicate batch entities rejected");
    invalid = transforms; invalid.back().transform.scale.z = -1;
    Check(Rejected([&] { world.ApplyTransforms(invalid); }), "invalid batch transform rejected");
    world.ApplyTransforms({});
    auto snapshot = world.Snapshot();
    Check(snapshot.size() == 256 && snapshot.front().entity.value < snapshot.back().entity.value,
          "world snapshots have deterministic identity order");
    world.Clear();

    const auto initial = renderer.Statistics();
    auto mesh = renderer.CreateMesh(MakePrimitive(Primitive::Cube));
    auto material = renderer.CreateMaterial({});
    auto secondMaterial = renderer.CreateMaterial({});
    {
        SceneRenderer bridge(renderer);
        e = world.Create("Rendered entity");
        world.SetTransform(e, t);
        world.SetRenderable(e, {mesh, material});
        world.SetLight(e, {});
        world.SetCamera(e, {}); world.SetActiveCamera(e);
        bridge.Sync(world);
        Check(bridge.Objects() == 1 && bridge.Lights() == 1 &&
                  renderer.Statistics().objects == initial.objects + 1,
              "ECS synchronization creates Ogre objects, lights and active camera");
        bridge.Sync(world);
        Check(renderer.Statistics().objects == initial.objects + 1,
              "repeated ECS sync does not duplicate renderer instances");
        Check(Rejected([&] { renderer.DestroyMesh(mesh); }), "bridge keeps borrowed mesh in use");
        world.SetRenderable(e, {mesh, secondMaterial, false, false, 20});
        bridge.Sync(world);
        Check(bridge.Objects() == 1, "renderable replacement updates flags and preserves instance count");
        auto stale = renderer.CreateMaterial({}); renderer.DestroyMaterial(stale);
        world.SetRenderable(e, {mesh, stale});
        Check(Rejected([&] { bridge.Sync(world); }) && bridge.Objects() == 1 &&
                  renderer.Statistics().objects == initial.objects + 1,
              "invalid borrowed material leaves existing renderer instance intact");
        world.SetRenderable(e, {mesh, material}); bridge.Sync(world);
        world.RemoveRenderable(e); world.RemoveLight(e); bridge.Sync(world);
        Check(!bridge.Objects() && !bridge.Lights(), "component removal releases renderer instances");
        world.SetRenderable(e, {mesh, material}); bridge.Sync(world);
        world.Destroy(e); bridge.Sync(world);
        Check(!bridge.Objects() && !world.ActiveCamera(), "entity deletion removes rendering and active camera");
        e = world.Create(); world.SetRenderable(e, {mesh, material}); bridge.Sync(world);
        bridge.Sync(other);
        Check(!bridge.Objects(), "syncing another world removes previous instances");
        bridge.Sync(world); // Destructor must clean this live object.
    }
    Check(renderer.Statistics().objects == initial.objects && renderer.Statistics().lights == initial.lights,
          "scene bridge RAII releases all owned renderer instances");
    renderer.DestroyMaterial(secondMaterial);
    renderer.DestroyMaterial(material);
    renderer.DestroyMesh(mesh);
    Check(renderer.Statistics().meshes == initial.meshes && renderer.Statistics().materials == initial.materials,
          "ECS tests release their borrowed render resources");
}
