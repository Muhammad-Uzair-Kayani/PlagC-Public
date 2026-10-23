#pragma once
// Engine-only access shared by the World, transform system and render bridge.
#include <PlagC/World/World.h>
#include <PlagC/Thread.h>
#include <entt/entity/registry.hpp>
#include <unordered_map>
namespace PlagC {
struct WorldChanges {
    std::uint64_t transform=1, renderable=1, light=1, camera=1;
};
struct WorldLifetime {};
struct World::Impl {
    entt::registry registry;
    std::unordered_map<std::uint64_t, entt::entity> entities;
    Entity active;
    std::uint64_t identity, structure=1, topology=1, cameraSelection=1;
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
