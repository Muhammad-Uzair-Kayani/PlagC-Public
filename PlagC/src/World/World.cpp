#include "pgcpch.h"
#include <PlagC/World/World.h>
#include <PlagC/Thread.h>
#include "WorldInternal.h"
#include <unordered_set>

namespace PlagC
{
namespace
{
std::atomic<std::uint64_t> nextEntity{1}, nextWorld{1};
struct Name { std::string value; };
bool Finite(Vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
void Require(bool v, const char *message) { if (!v) throw std::invalid_argument(message); }
}
void ValidateWorldTransform(const Transform &t)
{
    Require(Finite(t.position) && Finite(t.rotationDegrees) && Finite(t.scale) &&
            t.scale.x > 0 && t.scale.y > 0 && t.scale.z > 0, "invalid world transform");
}
void ValidateWorldCamera(const CameraComponent &c, const Transform &t)
{
    const auto p = t.position;
    Require(Finite(c.target) && std::isfinite(c.fov) && c.fov > 1 && c.fov < 179 &&
            std::isfinite(c.nearClip) && std::isfinite(c.farClip) && c.nearClip > 0 &&
            c.farClip > c.nearClip &&
            (c.target.x != p.x || c.target.y != p.y || c.target.z != p.z), "invalid world camera");
}
World::World() : impl_(std::make_unique<Impl>(nextWorld.fetch_add(1))) { Thread::RequireMain(); }
World::~World() { Thread::RequireMain(); }
Entity World::Create(std::string name)
{
    Thread::RequireMain();
    const Entity id{nextEntity.fetch_add(1)};
    auto &p = *impl_;
    const auto raw = p.registry.create();
    try
    {
        p.registry.emplace<Name>(raw, std::move(name));
        p.registry.emplace<Transform>(raw);
        p.registry.emplace<Entity>(raw, id);
        p.registry.emplace<WorldChanges>(raw);
        p.entities.emplace(id.value, raw);
    }
    catch (...) { p.registry.destroy(raw); throw; }
    ++p.topology;
    return id;
}
void World::Destroy(Entity e)
{
    auto &p = *impl_;
    const auto raw=p.Get(e);
    if(p.registry.any_of<RenderableComponent,LightComponent>(raw)) ++p.structure;
    ++p.topology;
    p.registry.destroy(raw);
    p.entities.erase(e.value);
    if (p.active == e) { p.active = {}; ++p.cameraSelection; }
}
void World::Clear()
{
    Thread::RequireMain();
    ++impl_->structure; ++impl_->topology; ++impl_->cameraSelection;
    impl_->registry.clear();
    impl_->entities.clear();
    impl_->active = {};
}
bool World::Alive(Entity e) const
{
    Thread::RequireMain();
    return impl_->entities.contains(e.value);
}
std::size_t World::Size() const { Thread::RequireMain(); return impl_->entities.size(); }
EntityRecord World::Read(Entity e) const
{
    const auto raw = impl_->Get(e);
    const auto &r = impl_->registry;
    EntityRecord result{e, r.get<Name>(raw).value, r.get<Transform>(raw), {}, {}, {}};
    if (auto *v = r.try_get<RenderableComponent>(raw)) result.renderable = *v;
    if (auto *v = r.try_get<LightComponent>(raw)) result.light = *v;
    if (auto *v = r.try_get<CameraComponent>(raw)) result.camera = *v;
    return result;
}
std::vector<EntityRecord> World::Snapshot() const
{
    Thread::RequireMain();
    std::vector<EntityRecord> result;
    result.reserve(Size());
    for (auto [id, raw] : impl_->entities) result.push_back(Read({id}));
    std::sort(result.begin(), result.end(), [](const auto &a, const auto &b) {
        return a.entity.value < b.entity.value;
    });
    return result;
}
void World::Rename(Entity e, std::string name)
{
    impl_->registry.get<Name>(impl_->Get(e)).value = std::move(name);
}
void World::SetTransform(Entity e, const Transform &t)
{
    const auto raw = impl_->Get(e);
    ValidateWorldTransform(t);
    if (auto *c = impl_->registry.try_get<CameraComponent>(raw)) ValidateWorldCamera(*c, t);
    impl_->registry.replace<Transform>(raw, t);
    ++impl_->registry.get<WorldChanges>(raw).transform;
}
void World::SetRenderable(Entity e, const RenderableComponent &c)
{
    const auto raw = impl_->Get(e);
    Require(bool(c.mesh) && bool(c.material) && c.queue > 0 && c.queue < 200,
            "invalid world renderable");
    if(!impl_->registry.all_of<RenderableComponent>(raw)) ++impl_->structure;
    impl_->registry.emplace_or_replace<RenderableComponent>(raw, c);
    ++impl_->registry.get<WorldChanges>(raw).renderable;
}
void World::RemoveRenderable(Entity e) {
    const auto raw=impl_->Get(e);
    if(impl_->registry.remove<RenderableComponent>(raw)) ++impl_->structure;
}
void World::SetLight(Entity e, const LightComponent &c)
{
    const auto raw = impl_->Get(e);
    const auto &d = c.light;
    Require((d.type == LightType::Directional || d.type == LightType::Point || d.type == LightType::Spot) &&
            Finite(d.direction) && Finite(d.colour) &&
            (d.direction.x != 0 || d.direction.y != 0 || d.direction.z != 0) &&
            std::isfinite(d.power) && d.power >= 0 && std::isfinite(d.radius) && d.radius > 0 &&
            std::isfinite(d.innerAngle) && std::isfinite(d.outerAngle) &&
            d.innerAngle > 0 && d.outerAngle >= d.innerAngle && d.outerAngle < 180,
            "invalid world light");
    if(!impl_->registry.all_of<LightComponent>(raw)) ++impl_->structure;
    impl_->registry.emplace_or_replace<LightComponent>(raw, c);
    ++impl_->registry.get<WorldChanges>(raw).light;
}
void World::RemoveLight(Entity e) {
    const auto raw=impl_->Get(e);
    if(impl_->registry.remove<LightComponent>(raw)) ++impl_->structure;
}
void World::SetCamera(Entity e, const CameraComponent &c)
{
    const auto raw = impl_->Get(e);
    ValidateWorldCamera(c, impl_->registry.get<Transform>(raw));
    if(!impl_->registry.all_of<CameraComponent>(raw)) ++impl_->topology;
    impl_->registry.emplace_or_replace<CameraComponent>(raw, c);
    ++impl_->registry.get<WorldChanges>(raw).camera;
}
void World::RemoveCamera(Entity e)
{
    if(impl_->registry.remove<CameraComponent>(impl_->Get(e))) ++impl_->topology;
    if (impl_->active == e) { impl_->active = {}; ++impl_->cameraSelection; }
}
void World::SetActiveCamera(Entity e)
{
    Require(impl_->registry.all_of<CameraComponent>(impl_->Get(e)), "entity has no camera");
    impl_->active = e; ++impl_->cameraSelection;
}
void World::ClearActiveCamera() { Thread::RequireMain(); impl_->active = {}; ++impl_->cameraSelection; }
Entity World::ActiveCamera() const { Thread::RequireMain(); return impl_->active; }
std::vector<TransformUpdate> World::TransformSnapshot() const
{
    Thread::RequireMain();
    std::vector<TransformUpdate> result;
    result.reserve(Size());
    for (auto [id, raw] : impl_->entities)
        result.push_back({{id}, impl_->registry.get<Transform>(raw)});
    std::sort(result.begin(), result.end(), [](const auto &a, const auto &b) {
        return a.entity.value < b.entity.value;
    });
    return result;
}
void World::ApplyTransforms(std::span<const TransformUpdate> updates)
{
    Thread::RequireMain();
    std::unordered_set<std::uint64_t> seen;
    for (const auto &u : updates)
    {
        const auto raw = impl_->Get(u.entity);
        Require(seen.insert(u.entity.value).second, "duplicate entity in transform batch");
        ValidateWorldTransform(u.transform);
        if (auto *c = impl_->registry.try_get<CameraComponent>(raw)) ValidateWorldCamera(*c, u.transform);
    }
    for (const auto &u : updates) {
        const auto raw=impl_->Get(u.entity);
        impl_->registry.replace<Transform>(raw, u.transform);
        ++impl_->registry.get<WorldChanges>(raw).transform;
    }
}
} // namespace PlagC

