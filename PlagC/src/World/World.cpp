#include "pgcpch.h"
#include <PlagC/World/World.h>
#include <PlagC/World/TransformGraph.h>
#include <PlagC/Thread.h>
#include "WorldInternal.h"
#include <functional>
#include <unordered_set>

namespace PlagC
{
namespace
{
std::atomic<std::uint64_t> nextEntity{1}, nextWorld{1};
struct Name
{
    std::string value;
};
bool Finite(Vec3 v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
void Require(bool v, const char *message)
{
    if (!v)
        throw std::invalid_argument(message);
}
template <class ImplT> Entity ParentOf(const ImplT &p, Entity child)
{
    const auto it = p.parentByChild.find(child.value);
    return it == p.parentByChild.end() ? Entity{} : Entity{it->second};
}
template <class ImplT> bool HasChildren(const ImplT &p, Entity parent)
{
    for (const auto &[child, candidate] : p.parentByChild)
    {
        (void)child;
        if (candidate == parent.value)
            return true;
    }
    return false;
}
template <class ImplT>
std::unordered_set<std::uint64_t> DescendantClosure(const ImplT &p,
                                                     std::unordered_set<std::uint64_t> roots)
{
    bool changed = true;
    while (changed)
    {
        changed = false;
        for (const auto &[child, parent] : p.parentByChild)
            if (roots.contains(parent) && roots.insert(child).second)
                changed = true;
    }
    return roots;
}
template <class ImplT> bool SubtreeHasPhysics(const ImplT &p, Entity root)
{
    const auto subtree = DescendantClosure(p, {root.value});
    for (const auto id : subtree)
        if (p.registry.all_of<PhysicsComponent>(p.entities.at(id)))
            return true;
    return false;
}
} // namespace
void ValidateWorldTransform(const Transform &t)
{
    Require(Finite(t.position) && Finite(t.rotationDegrees) && Finite(t.scale) && t.scale.x > 0 &&
                t.scale.y > 0 && t.scale.z > 0,
            "invalid world transform");
}
void ValidateWorldCamera(const CameraComponent &c, const Transform &t)
{
    const auto p = t.position;
    Require(std::isfinite(c.aspect) && c.aspect >= 0 && Finite(c.target) && std::isfinite(c.fov) &&
                c.fov > 1 && c.fov < 179 && std::isfinite(c.nearClip) && std::isfinite(c.farClip) &&
                c.nearClip > 0 && c.farClip > c.nearClip &&
                (c.target.x != p.x || c.target.y != p.y || c.target.z != p.z),
            "invalid world camera");
}
World::World() : impl_(std::make_unique<Impl>(nextWorld.fetch_add(1)))
{
    Thread::RequireMain();
}
World::~World()
{
    Thread::RequireMain();
}
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
        p.localByEntity.emplace(id.value, Transform{});
    }
    catch (...)
    {
        p.localByEntity.erase(id.value);
        p.entities.erase(id.value);
        p.registry.destroy(raw);
        throw;
    }
    ++p.topology;
    return id;
}
void World::Destroy(Entity e)
{
    auto &p = *impl_;
    const auto raw = p.Get(e);
    if (p.registry.any_of<RenderableComponent, LightComponent>(raw))
        ++p.structure;
    ++p.topology;
    if (p.registry.all_of<PhysicsComponent>(raw))
        ++p.physicsStructure;

    // Relationship deletion is non-recursive. Children are promoted to roots while
    // preserving their current resolved world transforms.
    p.parentByChild.erase(e.value);
    for (auto it = p.parentByChild.begin(); it != p.parentByChild.end();)
    {
        if (it->second == e.value)
        {
            const auto childRaw = p.entities.at(it->first);
            p.localByEntity.at(it->first) = p.registry.get<Transform>(childRaw);
            it = p.parentByChild.erase(it);
        }
        else
            ++it;
    }

    p.localByEntity.erase(e.value);
    p.registry.destroy(raw);
    p.entities.erase(e.value);
    if (p.active == e)
    {
        p.active = {};
        ++p.cameraSelection;
    }
}
void World::Clear()
{
    Thread::RequireMain();
    ++impl_->structure;
    ++impl_->topology;
    ++impl_->cameraSelection;
    ++impl_->physicsStructure;
    impl_->registry.clear();
    impl_->entities.clear();
    impl_->localByEntity.clear();
    impl_->parentByChild.clear();
    impl_->active = {};
}
bool World::Alive(Entity e) const
{
    Thread::RequireMain();
    return impl_->entities.contains(e.value);
}
std::size_t World::Size() const
{
    Thread::RequireMain();
    return impl_->entities.size();
}
Transform World::ReadTransform(Entity e) const
{
    return impl_->registry.get<Transform>(impl_->Get(e));
}
Transform World::ReadLocalTransform(Entity e) const
{
    impl_->Get(e);
    const auto it = impl_->localByEntity.find(e.value);
    if (it == impl_->localByEntity.end())
        throw std::logic_error("world lost an entity local transform");
    return it->second;
}
EntityRecord World::Read(Entity e) const
{
    const auto raw = impl_->Get(e);
    const auto &r = impl_->registry;
    EntityRecord result{e, r.get<Name>(raw).value, r.get<Transform>(raw), {}, {}, {}, {}, {}, {}};
    if (auto *v = r.try_get<RenderableComponent>(raw))
        result.renderable = *v;
    if (auto *v = r.try_get<LightComponent>(raw))
        result.light = *v;
    if (auto *v = r.try_get<CameraComponent>(raw))
        result.camera = *v;
    if (auto *v = r.try_get<PhysicsComponent>(raw))
        result.physics = *v;
    if (auto *v = r.try_get<ModelComponent>(raw))
        result.model = *v;
    if (auto *v = r.try_get<AnimatorComponent>(raw))
        result.animator = *v;
    return result;
}
std::vector<EntityRecord> World::Snapshot() const
{
    Thread::RequireMain();
    std::vector<EntityRecord> result;
    result.reserve(Size());
    for (auto [id, raw] : impl_->entities)
    {
        (void)raw;
        result.push_back(Read({id}));
    }
    std::sort(result.begin(), result.end(),
              [](const auto &a, const auto &b) { return a.entity.value < b.entity.value; });
    return result;
}
void World::Rename(Entity e, std::string name)
{
    impl_->registry.get<Name>(impl_->Get(e)).value = std::move(name);
}
void World::SetTransform(Entity e, const Transform &t)
{
    const TransformUpdate update{e, t};
    ApplyTransforms(std::span<const TransformUpdate>(&update, 1));
}
void World::SetLocalTransform(Entity e, const Transform &local)
{
    Thread::RequireMain();
    auto &p = *impl_;
    const auto raw = p.Get(e);
    if (auto *pc = p.registry.try_get<PhysicsComponent>(raw))
        Require(pc->body.motion != MotionType::Dynamic,
                "dynamic transform is physics-owned; use PhysicsSystem::Teleport");
    ValidateWorldTransform(local);

    auto affected = DescendantClosure(p, {e.value});
    std::unordered_map<std::uint64_t, Transform> resolved;
    std::function<Transform(Entity)> resolve = [&](Entity current) -> Transform {
        if (const auto found = resolved.find(current.value); found != resolved.end())
            return found->second;
        const Transform currentLocal = current == e ? local : p.localByEntity.at(current.value);
        const Entity parent = ParentOf(p, current);
        const Transform world = parent
            ? ComposeTransform(affected.contains(parent.value) ? resolve(parent)
                                                               : p.registry.get<Transform>(p.Get(parent)),
                               currentLocal)
            : currentLocal;
        resolved.emplace(current.value, world);
        return world;
    };

    for (const auto id : affected)
    {
        const auto entityRaw = p.entities.at(id);
        const Transform world = resolve(Entity{id});
        ValidateWorldTransform(world);
        if (auto *camera = p.registry.try_get<CameraComponent>(entityRaw))
            ValidateWorldCamera(*camera, world);
    }

    p.localByEntity.at(e.value) = local;
    for (const auto id : affected)
    {
        const auto entityRaw = p.entities.at(id);
        p.registry.replace<Transform>(entityRaw, resolved.at(id));
        ++p.registry.get<WorldChanges>(entityRaw).transform;
    }
}
Entity World::Parent(Entity child) const
{
    impl_->Get(child);
    const auto it = impl_->parentByChild.find(child.value);
    if (it == impl_->parentByChild.end())
        return {};
    if (!impl_->entities.contains(it->second))
        throw std::logic_error("world hierarchy contains a stale parent");
    return Entity{it->second};
}
std::vector<Entity> World::Children(Entity parent) const
{
    impl_->Get(parent);
    std::vector<Entity> result;
    for (const auto &[child, candidate] : impl_->parentByChild)
        if (candidate == parent.value)
        {
            if (!impl_->entities.contains(child))
                throw std::logic_error("world hierarchy contains a stale child");
            result.push_back(Entity{child});
        }
    std::sort(result.begin(), result.end(),
              [](Entity a, Entity b) { return a.value < b.value; });
    return result;
}
void World::SetParent(Entity child, Entity parent)
{
    auto &p = *impl_;
    const auto childRaw = p.Get(child);
    const auto parentRaw = p.Get(parent);
    Require(child != parent, "entity cannot parent itself");

    if (const auto current = Parent(child); current == parent)
        return;

    // Hierarchy propagation and physics body ownership are intentionally disjoint until
    // authored rigid-body/local-transform semantics are defined together.
    Require(!SubtreeHasPhysics(p, child), "physics entity cannot participate in hierarchy");
    Entity physicsCursor = parent;
    while (physicsCursor)
    {
        Require(!p.registry.all_of<PhysicsComponent>(p.Get(physicsCursor)),
                "physics entity cannot participate in hierarchy");
        physicsCursor = ParentOf(p, physicsCursor);
    }
    (void)childRaw;
    (void)parentRaw;

    // Walk the proposed parent's chain before mutation. A repeated node indicates
    // existing corruption; reaching child would introduce a cycle.
    std::unordered_set<std::uint64_t> seen;
    Entity cursor = parent;
    while (cursor)
    {
        Require(seen.insert(cursor.value).second, "world hierarchy is cyclic");
        Require(cursor != child, "world hierarchy cycle");
        cursor = ParentOf(p, cursor);
    }

    const Transform world = p.registry.get<Transform>(p.Get(child));
    const Transform parentWorld = p.registry.get<Transform>(p.Get(parent));
    const Transform local = RelativeTransform(parentWorld, world);
    p.parentByChild[child.value] = parent.value;
    p.localByEntity.at(child.value) = local;
    ++p.topology;
}
void World::ClearParent(Entity child)
{
    auto &p = *impl_;
    const auto raw = p.Get(child);
    if (p.parentByChild.erase(child.value))
    {
        p.localByEntity.at(child.value) = p.registry.get<Transform>(raw);
        ++p.topology;
    }
}
void World::SetPhysics(Entity e, const PhysicsComponent &c)
{
    auto &p = *impl_;
    auto raw = p.Get(e);
    Require(!p.registry.all_of<CameraComponent>(raw), "physics camera ownership is unsupported");
    Require(!ParentOf(p, e) && !HasChildren(p, e),
            "physics entity cannot participate in hierarchy");
    ValidatePhysicsBody({c, p.registry.get<Transform>(raw)});
    p.registry.emplace_or_replace<PhysicsComponent>(raw, c);
    ++p.registry.get<WorldChanges>(raw).physics;
    ++p.physicsStructure;
    ++p.topology;
}
void World::RemovePhysics(Entity e)
{
    auto &p = *impl_;
    auto raw = p.Get(e);
    if (p.registry.remove<PhysicsComponent>(raw))
    {
        ++p.physicsStructure;
        ++p.topology;
    }
}
void World::SetRenderable(Entity e, const RenderableComponent &c)
{
    const auto raw = impl_->Get(e);
    Require(bool(c.mesh) && bool(c.material) && c.queue > 0 && c.queue < 200,
            "invalid world renderable");
    if (!impl_->registry.all_of<RenderableComponent>(raw))
        ++impl_->structure;
    impl_->registry.emplace_or_replace<RenderableComponent>(raw, c);
    ++impl_->registry.get<WorldChanges>(raw).renderable;
}
void World::RemoveRenderable(Entity e)
{
    const auto raw = impl_->Get(e);
    if (impl_->registry.remove<RenderableComponent>(raw))
        ++impl_->structure;
}
void World::SetLight(Entity e, const LightComponent &c)
{
    const auto raw = impl_->Get(e);
    const auto &d = c.light;
    Require((d.type == LightType::Directional || d.type == LightType::Point ||
             d.type == LightType::Spot) &&
                Finite(d.direction) && Finite(d.colour) &&
                (d.direction.x != 0 || d.direction.y != 0 || d.direction.z != 0) &&
                std::isfinite(d.power) && d.power >= 0 && std::isfinite(d.radius) && d.radius > 0 &&
                std::isfinite(d.innerAngle) && std::isfinite(d.outerAngle) && d.innerAngle > 0 &&
                d.outerAngle >= d.innerAngle && d.outerAngle < 180,
            "invalid world light");
    if (!impl_->registry.all_of<LightComponent>(raw))
        ++impl_->structure;
    impl_->registry.emplace_or_replace<LightComponent>(raw, c);
    ++impl_->registry.get<WorldChanges>(raw).light;
}
void World::RemoveLight(Entity e)
{
    const auto raw = impl_->Get(e);
    if (impl_->registry.remove<LightComponent>(raw))
        ++impl_->structure;
}
void World::SetCamera(Entity e, const CameraComponent &c)
{
    const auto raw = impl_->Get(e);
    Require(!impl_->registry.all_of<PhysicsComponent>(raw),
            "physics camera ownership is unsupported");
    ValidateWorldCamera(c, impl_->registry.get<Transform>(raw));
    if (!impl_->registry.all_of<CameraComponent>(raw))
        ++impl_->topology;
    impl_->registry.emplace_or_replace<CameraComponent>(raw, c);
    ++impl_->registry.get<WorldChanges>(raw).camera;
}
void World::RemoveCamera(Entity e)
{
    if (impl_->registry.remove<CameraComponent>(impl_->Get(e)))
        ++impl_->topology;
    if (impl_->active == e)
    {
        impl_->active = {};
        ++impl_->cameraSelection;
    }
}
void World::SetActiveCamera(Entity e)
{
    Require(impl_->registry.all_of<CameraComponent>(impl_->Get(e)), "entity has no camera");
    impl_->active = e;
    ++impl_->cameraSelection;
}
void World::ClearActiveCamera()
{
    Thread::RequireMain();
    impl_->active = {};
    ++impl_->cameraSelection;
}
Entity World::ActiveCamera() const
{
    Thread::RequireMain();
    return impl_->active;
}
std::vector<TransformUpdate> World::TransformSnapshot() const
{
    Thread::RequireMain();
    std::vector<TransformUpdate> result;
    result.reserve(Size());
    for (auto [id, raw] : impl_->entities)
        result.push_back({{id}, impl_->registry.get<Transform>(raw)});
    std::sort(result.begin(), result.end(),
              [](const auto &a, const auto &b) { return a.entity.value < b.entity.value; });
    return result;
}
void World::ApplyTransforms(std::span<const TransformUpdate> updates)
{
    Thread::RequireMain();
    if (updates.empty())
        return;
    auto &p = *impl_;

    std::unordered_map<std::uint64_t, Transform> desired;
    desired.reserve(updates.size());
    for (const auto &u : updates)
    {
        const auto raw = p.Get(u.entity);
        Require(desired.emplace(u.entity.value, u.transform).second,
                "duplicate entity in transform batch");
        if (auto *pc = p.registry.try_get<PhysicsComponent>(raw))
            Require(pc->body.motion != MotionType::Dynamic,
                    "dynamic transform is physics-owned; use PhysicsSystem::Teleport");
        ValidateWorldTransform(u.transform);
    }

    std::unordered_set<std::uint64_t> roots;
    roots.reserve(desired.size());
    for (const auto &[id, transform] : desired)
    {
        (void)transform;
        roots.insert(id);
    }
    const auto affected = DescendantClosure(p, std::move(roots));

    // Resolve the world each entity would have if explicit updates override inherited motion.
    // This gives the final parent world used to derive local values for explicit children.
    std::unordered_map<std::uint64_t, Transform> targetWorld;
    std::function<Transform(Entity)> resolveTarget = [&](Entity entity) -> Transform {
        if (const auto found = targetWorld.find(entity.value); found != targetWorld.end())
            return found->second;
        if (const auto explicitValue = desired.find(entity.value); explicitValue != desired.end())
        {
            targetWorld.emplace(entity.value, explicitValue->second);
            return explicitValue->second;
        }
        if (!affected.contains(entity.value))
            return p.registry.get<Transform>(p.Get(entity));
        const Entity parent = ParentOf(p, entity);
        const Transform world = parent
            ? ComposeTransform(resolveTarget(parent), p.localByEntity.at(entity.value))
            : p.localByEntity.at(entity.value);
        targetWorld.emplace(entity.value, world);
        return world;
    };

    std::unordered_map<std::uint64_t, Transform> explicitLocals;
    explicitLocals.reserve(desired.size());
    for (const auto &[id, world] : desired)
    {
        const Entity entity{id};
        const Entity parent = ParentOf(p, entity);
        explicitLocals.emplace(id, parent ? RelativeTransform(resolveTarget(parent), world) : world);
    }

    std::unordered_map<std::uint64_t, Transform> finalWorld;
    std::function<Transform(Entity)> resolveFinal = [&](Entity entity) -> Transform {
        if (const auto found = finalWorld.find(entity.value); found != finalWorld.end())
            return found->second;
        const auto explicitLocal = explicitLocals.find(entity.value);
        const Transform local = explicitLocal != explicitLocals.end()
                                    ? explicitLocal->second
                                    : p.localByEntity.at(entity.value);
        const Entity parent = ParentOf(p, entity);
        const Transform world = parent
            ? ComposeTransform(affected.contains(parent.value)
                                   ? resolveFinal(parent)
                                   : p.registry.get<Transform>(p.Get(parent)),
                               local)
            : local;
        finalWorld.emplace(entity.value, world);
        return world;
    };

    for (const auto id : affected)
    {
        const auto raw = p.entities.at(id);
        const Transform world = resolveFinal(Entity{id});
        ValidateWorldTransform(world);
        if (auto *camera = p.registry.try_get<CameraComponent>(raw))
            ValidateWorldCamera(*camera, world);
    }

    for (const auto &[id, local] : explicitLocals)
        p.localByEntity.at(id) = local;
    for (const auto id : affected)
    {
        const auto raw = p.entities.at(id);
        p.registry.replace<Transform>(raw, finalWorld.at(id));
        ++p.registry.get<WorldChanges>(raw).transform;
    }
}
void World::SetModel(Entity e, const ModelComponent &c)
{
    auto raw = impl_->Get(e);
    if (!c.asset)
        throw std::invalid_argument("model asset is empty");
    impl_->registry.emplace_or_replace<ModelComponent>(raw, c);
}
void World::RemoveModel(Entity e)
{
    auto raw = impl_->Get(e);
    impl_->registry.remove<ModelComponent, AnimatorComponent>(raw);
}
void World::SetAnimator(Entity e, const AnimatorComponent &c)
{
    auto raw = impl_->Get(e);
    auto *model = impl_->registry.try_get<ModelComponent>(raw);
    const ModelAsset *clips = c.animationSource ? c.animationSource.Shared().get() :
                                                 (model ? model->asset.Shared().get() : nullptr);
    if (!model || !clips || clips->animations.empty() ||
        c.state.clip >= clips->animations.size() || !std::isfinite(c.state.time) ||
        !std::isfinite(c.state.speed))
        throw std::invalid_argument("invalid animator/model binding");
    if (!std::isfinite(c.fadeElapsed) || c.fadeElapsed < 0 || !std::isfinite(c.fadeDuration) ||
        c.fadeDuration < 0)
        throw std::invalid_argument("invalid animation fade");
    if (c.fadeDuration > 0 && (c.previous.clip >= clips->animations.size() ||
                               !std::isfinite(c.previous.time) || !std::isfinite(c.previous.speed)))
        throw std::invalid_argument("invalid previous animation state");
    if (c.upperBody)
    {
        const auto &layer = *c.upperBody;
        if (layer.pose.clip >= clips->animations.size() || !std::isfinite(layer.pose.time) ||
            !std::isfinite(layer.pose.speed) || !std::isfinite(layer.weight) || layer.weight < 0 ||
            layer.weight > 1 || !std::isfinite(layer.recoilRadians) ||
            std::none_of(model->asset->skeleton.joints.begin(), model->asset->skeleton.joints.end(),
                         [&](const auto &j) { return j.name == layer.root; }))
            throw std::invalid_argument("invalid upper body animation binding");
    }
    if (c.controller)
        c.controller->Graph().Validate(unsigned(clips->animations.size()));
    impl_->registry.emplace_or_replace<AnimatorComponent>(raw, c);
}
void World::RemoveAnimator(Entity e)
{
    impl_->registry.remove<AnimatorComponent>(impl_->Get(e));
}
} // namespace PlagC
