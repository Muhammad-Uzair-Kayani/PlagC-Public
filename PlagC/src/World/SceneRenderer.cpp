#include "pgcpch.h"
#include <PlagC/Camera/CameraSystem.h>
#include <PlagC/World/SceneRenderer.h>
#include "WorldInternal.h"
namespace PlagC
{
namespace
{
using Clock = std::chrono::steady_clock;
double Milliseconds(Clock::time_point start)
{
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}
// Disabled detailed profiling does not read the clock per object.
struct Measure
{
    double &result;
    bool enabled;
    Clock::time_point start;
    Measure(double &r, bool on) : result(r), enabled(on)
    {
        if (enabled)
            start = Clock::now();
    }
    ~Measure()
    {
        if (enabled)
            result += Milliseconds(start);
    }
};
} // namespace
struct SceneRenderer::Impl
{
    Renderer &renderer;
    struct Object
    {
        entt::entity raw{};
        ObjectHandle handle;
        RenderableComponent component;
        std::uint64_t transform = 0, property = 0, seen = 0;
        std::optional<Transform> presentation;
    };
    struct Light
    {
        entt::entity raw{};
        LightHandle handle;
        std::uint64_t transform = 0, property = 0, seen = 0;
    };
    std::unordered_map<std::uint64_t, Object> objects;
    std::unordered_map<std::uint64_t, Light> lights;
    std::vector<Object *> objectBindings;
    std::vector<Light *> lightBindings;
    std::uint64_t world = 0, structure = 0, scan = 0, cameraSelection = 0, cameraTransform = 0,
                  cameraProperty = 0;
    bool profiling = false;
    SceneSyncStats stats;
    explicit Impl(Renderer &r) : renderer(r)
    {
    }
};
SceneRenderer::SceneRenderer(Renderer &r) : impl_(std::make_unique<Impl>(r))
{
    Thread::RequireMain();
}
SceneRenderer::~SceneRenderer()
{
    Clear();
}
void SceneRenderer::Clear()
{
    Thread::RequireMain();
    auto &p = *impl_;
    for (auto &[id, o] : p.objects)
        if (o.handle)
            p.renderer.DestroyObject(o.handle);
    for (auto &[id, l] : p.lights)
        if (l.handle)
            p.renderer.DestroyLight(l.handle);
    p.objectBindings.clear();
    p.lightBindings.clear();
    p.objects.clear();
    p.lights.clear();
    p.world = p.structure = p.cameraSelection = p.cameraTransform = p.cameraProperty = 0;
}
std::size_t SceneRenderer::Objects() const
{
    Thread::RequireMain();
    std::size_t n = 0;
    for (auto &[id, o] : impl_->objects)
        n += bool(o.handle);
    return n;
}
std::size_t SceneRenderer::Lights() const
{
    Thread::RequireMain();
    std::size_t n = 0;
    for (auto &[id, l] : impl_->lights)
        n += bool(l.handle);
    return n;
}
void SceneRenderer::SetProfiling(bool value)
{
    Thread::RequireMain();
    impl_->profiling = value;
}
SceneSyncStats SceneRenderer::Statistics() const
{
    Thread::RequireMain();
    return impl_->stats;
}
void SceneRenderer::Sync(const World &world, std::span<const TransformUpdate> presentation)
{
    Thread::RequireMain();
    auto &p = *impl_;
    const auto &w = *world.impl_;
    const auto &registry = w.registry;
    const auto start = Clock::now();
    p.stats = {};
    auto &s = p.stats;
    s.detailed = p.profiling;
    if (p.world != w.identity)
    {
        Measure measure(s.resourceMs, p.profiling);
        s.destroyed = Objects() + Lights();
        Clear();
        p.world = w.identity;
    }
    if (p.structure != w.structure)
    {
        const auto structuralStart = Clock::now();
        s.structuralRebuild = true;
        const auto scan = ++p.scan;
        // Maps are touched only on structural changes; pointers remain stable across rehash.
        std::vector<Impl::Object *> objects;
        std::vector<Impl::Light *> lights;
        const auto objectsView =
            registry.view<const Entity, const Transform, const RenderableComponent>();
        for (auto raw : objectsView)
        {
            const auto id = objectsView.get<const Entity>(raw).value;
            auto &o = p.objects[id];
            o.raw = raw;
            o.seen = scan;
            objects.push_back(&o);
        }
        const auto lightsView =
            registry.view<const Entity, const Transform, const LightComponent>();
        for (auto raw : lightsView)
        {
            const auto id = lightsView.get<const Entity>(raw).value;
            auto &l = p.lights[id];
            l.raw = raw;
            l.seen = scan;
            lights.push_back(&l);
        }
        for (auto it = p.objects.begin(); it != p.objects.end();)
        {
            if (it->second.seen != scan)
            {
                if (it->second.handle)
                {
                    p.renderer.DestroyObject(it->second.handle);
                    ++s.destroyed;
                }
                it = p.objects.erase(it);
            }
            else
                ++it;
        }
        for (auto it = p.lights.begin(); it != p.lights.end();)
        {
            if (it->second.seen != scan)
            {
                if (it->second.handle)
                {
                    p.renderer.DestroyLight(it->second.handle);
                    ++s.destroyed;
                }
                it = p.lights.erase(it);
            }
            else
                ++it;
        }
        p.objectBindings.swap(objects);
        p.lightBindings.swap(lights);
        p.structure = w.structure;
        s.structureMs = Milliseconds(structuralStart);
    }
    for (const auto &u : presentation)
        ValidateWorldTransform(u.transform);
    for (const auto &u : presentation)
    {
        if (auto it = p.objects.find(u.entity.value); it != p.objects.end())
            it->second.presentation = u.transform;
    }
    for (auto *o : p.objectBindings)
    {
        ++s.scanned;
        const auto &changes = registry.get<WorldChanges>(o->raw);
        if (o->property != changes.renderable)
        {
            const auto &c = registry.get<RenderableComponent>(o->raw);
            const bool replace = !o->handle || o->component.mesh.value != c.mesh.value ||
                                 o->component.material.value != c.material.value;
            if (replace)
            {
                Measure measure(s.resourceMs, p.profiling);
                // Create first: an invalid borrowed resource leaves the existing object alive.
                auto handle =
                    p.renderer.CreateObject(c.mesh, c.material, registry.get<Transform>(o->raw));
                if (o->handle)
                {
                    p.renderer.DestroyObject(o->handle);
                    ++s.destroyed;
                }
                o->handle = handle;
                ++s.created;
                o->transform = changes.transform;
            }
            {
                Measure measure(s.propertyMs, p.profiling);
                if (replace || c.visible != o->component.visible)
                {
                    p.renderer.SetVisible(o->handle, c.visible);
                    ++s.propertyCalls;
                }
                if (replace || c.castShadows != o->component.castShadows)
                {
                    p.renderer.SetCastShadows(o->handle, c.castShadows);
                    ++s.propertyCalls;
                }
                if (replace || c.queue != o->component.queue)
                {
                    p.renderer.SetRenderQueue(o->handle, c.queue);
                    ++s.propertyCalls;
                }
            }
            o->component = c;
            o->property = changes.renderable;
        }
        if (o->presentation || o->transform != changes.transform)
        {
            Measure measure(s.transformMs, p.profiling);
            p.renderer.SetTransform(o->handle,
                                    o->presentation.value_or(registry.get<Transform>(o->raw)));
            ++s.transforms;
            o->transform = o->presentation ? 0 : changes.transform;
            o->presentation.reset();
        }
    }
    {
        Measure measure(s.lightCameraMs, p.profiling);
        for (auto *l : p.lightBindings)
        {
            ++s.scanned;
            const auto &changes = registry.get<WorldChanges>(l->raw);
            if (l->transform == changes.transform && l->property == changes.light)
                continue;
            auto desc = registry.get<LightComponent>(l->raw).light;
            desc.position = registry.get<Transform>(l->raw).position;
            if (!l->handle)
            {
                l->handle = p.renderer.CreateLight(desc);
                ++s.created;
            }
            else
                p.renderer.UpdateLight(l->handle, desc);
            ++s.lightCalls;
            l->transform = changes.transform;
            l->property = changes.light;
        }
        if (w.active)
        {
            const auto raw = w.Get(w.active);
            const auto &changes = registry.get<WorldChanges>(raw);
            if (p.cameraSelection != w.cameraSelection || p.cameraTransform != changes.transform ||
                p.cameraProperty != changes.camera)
            {
                CameraSystem{}.Sync(world, p.renderer);
                ++s.cameraCalls;
                p.cameraTransform = changes.transform;
                p.cameraProperty = changes.camera;
            }
        }
        p.cameraSelection = w.cameraSelection;
    }
    s.totalMs = Milliseconds(start);
    if (p.profiling)
        s.traversalMs = std::max(0.0, s.totalMs - s.structureMs - s.resourceMs - s.transformMs -
                                          s.propertyMs - s.lightCameraMs);
}
} // namespace PlagC
