#include "pgcpch.h"
#include <PlagC/World/TransformSystem.h>
#include "WorldInternal.h"
#include <unordered_set>
namespace PlagC {
struct TransformSystem::Impl {
    World::Impl* world=nullptr;
    std::weak_ptr<WorldLifetime> lifetime;
    std::uint64_t topology=0;
    struct Binding { Transform* transform; Transform* local; WorldChanges* changes; CameraComponent* camera; };
    std::vector<Binding> bindings;
    std::vector<Transform> values;
    std::vector<std::uint64_t> versions, cameraVersions;
    bool prepared=false;
};
TransformSystem::TransformSystem():impl_(std::make_unique<Impl>()) { Thread::RequireMain(); }
TransformSystem::~TransformSystem()=default;
void TransformSystem::Bind(World& world,std::span<const Entity> entities) {
    Thread::RequireMain();
    auto next=std::make_unique<Impl>();
    auto& w=*world.impl_;
    next->world=&w;next->lifetime=w.lifetime;next->topology=w.topology;
    next->bindings.reserve(entities.size());
    std::unordered_set<std::uint64_t> seen;seen.reserve(entities.size());
    for(auto e:entities) {
        auto raw=w.Get(e);
        if(auto* pc=w.registry.try_get<PhysicsComponent>(raw);pc&&pc->body.motion==MotionType::Dynamic)
            throw std::invalid_argument("dynamic transforms are physics-owned");
        if(w.parentByChild.contains(e.value))
            throw std::invalid_argument("hierarchical transforms require World transform APIs");
        for(const auto& [child,parent]:w.parentByChild) {
            (void)child;
            if(parent==e.value)
                throw std::invalid_argument("hierarchical transforms require World transform APIs");
        }
        if(!seen.insert(e.value).second)throw std::invalid_argument("duplicate transform system entity");
        auto local=w.localByEntity.find(e.value);
        if(local==w.localByEntity.end())throw std::logic_error("world lost an entity local transform");
        next->bindings.push_back({&w.registry.get<Transform>(raw), &local->second,
                                  &w.registry.get<WorldChanges>(raw),
                                  w.registry.try_get<CameraComponent>(raw)});
    }
    next->values.resize(entities.size());next->versions.resize(entities.size());next->cameraVersions.resize(entities.size());
    impl_=std::move(next);
}
bool TransformSystem::IsBindingValid() const {
    Thread::RequireMain();
    return !impl_->lifetime.expired() && impl_->world->topology==impl_->topology;
}
std::span<Transform> TransformSystem::Prepare() {
    if(!IsBindingValid())throw std::logic_error("transform system binding expired; rebind after topology changes");
    auto& p=*impl_;
    for(std::size_t i=0;i<p.bindings.size();++i) {
        const auto& b=p.bindings[i];p.values[i]=*b.transform;
        p.versions[i]=b.changes->transform;p.cameraVersions[i]=b.changes->camera;
    }
    p.prepared=true;return p.values;
}
void TransformSystem::Commit() {
    if(!IsBindingValid() || !impl_->prepared)throw std::logic_error("transform system requires valid prepared batch");
    auto& p=*impl_;
    for(std::size_t i=0;i<p.values.size();++i) {
        const auto& b=p.bindings[i];
        if(p.versions[i]!=b.changes->transform || p.cameraVersions[i]!=b.changes->camera)
            throw std::logic_error("transform changed since Prepare");
        ValidateWorldTransform(p.values[i]);
        if(b.camera)ValidateWorldCamera(*b.camera,p.values[i]);
    }
    // Bound entities are hierarchy-isolated roots, so local == resolved world.
    for(std::size_t i=0;i<p.values.size();++i) {
        *p.bindings[i].transform=p.values[i];
        *p.bindings[i].local=p.values[i];
        ++p.bindings[i].changes->transform;
    }
    p.prepared=false;
}
}
