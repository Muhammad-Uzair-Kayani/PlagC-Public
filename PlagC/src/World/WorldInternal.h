#pragma once
// Engine-only access shared by the World, transform system and render/physics bridges.
#include <PlagC/World/World.h>
#include <PlagC/Thread.h>
#include <entt/entity/registry.hpp>
#include <unordered_map>
#include <vector>
namespace PlagC {
struct WorldChanges {
    std::uint64_t transform=1, renderable=1, light=1, camera=1, physics=1;
};
struct WorldLifetime {};
struct World::Impl {
    entt::registry registry;
    std::unordered_map<std::uint64_t, entt::entity> entities;
    // Resolved world transforms live in the ECS Transform component. Every entity also owns
    // a local transform here; roots keep local == world.
    std::unordered_map<std::uint64_t, Transform> localByEntity;
    // Relationship indices are kept in sync. childrenByParent vectors are sorted by Entity ID,
    // making traversal deterministic without a scene-wide scan per hierarchy update.
    std::unordered_map<std::uint64_t, std::uint64_t> parentByChild;
    std::unordered_map<std::uint64_t, std::vector<std::uint64_t>> childrenByParent;
    Entity active;
    std::uint64_t identity, structure=1, topology=1, cameraSelection=1, physicsStructure=1;
    bool physicsAttached=false;
    std::shared_ptr<WorldLifetime> lifetime=std::make_shared<WorldLifetime>();
    explicit Impl(std::uint64_t id):identity(id) {}
    entt::entity Get(Entity e) const {
        Thread::RequireMain();
        auto it=entities.find(e.value);
        if(it==entities.end())throw std::invalid_argument("invalid, stale or foreign entity");
        return it->second;
    }
};
void ValidateWorldTransform(const Transform&);
void ValidateWorldCamera(const CameraComponent&,const Transform&);
}
