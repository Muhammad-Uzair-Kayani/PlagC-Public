// Frozen baseline from 152b668 for the CPU plumbing benchmark only.
#include "pgcpch.h"
#include "LegacySceneRenderer.h"
#include <PlagC/Thread.h>
#include <unordered_set>
namespace PlagC
{
struct LegacySceneRenderer::Impl
{
    Renderer &renderer;
    struct Object { ObjectHandle handle; MeshHandle mesh; MaterialHandle material; };
    std::unordered_map<std::uint64_t, Object> objects;
    std::unordered_map<std::uint64_t, LightHandle> lights;
    explicit Impl(Renderer &r) : renderer(r) {}
};
LegacySceneRenderer::LegacySceneRenderer(Renderer &r) : impl_(std::make_unique<Impl>(r)) { Thread::RequireMain(); }
LegacySceneRenderer::~LegacySceneRenderer() { Clear(); }
void LegacySceneRenderer::Clear()
{
    Thread::RequireMain();
    for (auto [id, o] : impl_->objects) impl_->renderer.DestroyObject(o.handle);
    for (auto [id, l] : impl_->lights) impl_->renderer.DestroyLight(l);
    impl_->objects.clear();
    impl_->lights.clear();
}
std::size_t LegacySceneRenderer::Objects() const { Thread::RequireMain(); return impl_->objects.size(); }
std::size_t LegacySceneRenderer::Lights() const { Thread::RequireMain(); return impl_->lights.size(); }
void LegacySceneRenderer::Sync(const World &world)
{
    Thread::RequireMain();
    auto &p = *impl_;
    const auto snapshot = world.Snapshot();
    std::unordered_set<std::uint64_t> objects, lights;
    for (const auto &e : snapshot)
    {
        if (e.renderable)
        {
            const auto &c = *e.renderable;
            objects.insert(e.entity.value);
            auto it = p.objects.find(e.entity.value);
            if (it == p.objects.end() || it->second.mesh.value != c.mesh.value ||
                it->second.material.value != c.material.value)
            {
                const auto handle = p.renderer.CreateObject(c.mesh, c.material, e.transform);
                try
                {
                    if (it == p.objects.end())
                        it = p.objects.emplace(e.entity.value, Impl::Object{handle, c.mesh, c.material}).first;
                    else
                    {
                        p.renderer.DestroyObject(it->second.handle);
                        it->second = {handle, c.mesh, c.material};
                    }
                }
                catch (...) { p.renderer.DestroyObject(handle); throw; }
            }
            p.renderer.SetTransform(it->second.handle, e.transform);
            p.renderer.SetVisible(it->second.handle, c.visible);
            p.renderer.SetCastShadows(it->second.handle, c.castShadows);
            p.renderer.SetRenderQueue(it->second.handle, c.queue);
        }
        if (e.light)
        {
            lights.insert(e.entity.value);
            auto desc = e.light->light;
            desc.position = e.transform.position;
            auto it = p.lights.find(e.entity.value);
            if (it == p.lights.end())
            {
                auto handle = p.renderer.CreateLight(desc);
                try { p.lights.emplace(e.entity.value, handle); }
                catch (...) { p.renderer.DestroyLight(handle); throw; }
            }
            else p.renderer.UpdateLight(it->second, desc);
        }
        if (e.camera && e.entity == world.ActiveCamera())
        {
            const auto &c = *e.camera;
            p.renderer.SetCamera({e.transform.position, c.target, c.fov, c.nearClip, c.farClip});
        }
    }
    for (auto it = p.objects.begin(); it != p.objects.end();)
        if (!objects.contains(it->first))
        {
            p.renderer.DestroyObject(it->second.handle);
            it = p.objects.erase(it);
        }
        else ++it;
    for (auto it = p.lights.begin(); it != p.lights.end();)
        if (!lights.contains(it->first))
        {
            p.renderer.DestroyLight(it->second);
            it = p.lights.erase(it);
        }
        else ++it;
}
}

