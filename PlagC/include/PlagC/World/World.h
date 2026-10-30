#pragma once
#include <PlagC/Renderer/Renderer.h>
#include <PlagC/Physics/PhysicsTypes.h>
#include <PlagC/Animation/Animation.h>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace PlagC
{
// Session identity. Values are never reused, including across different worlds.
struct Entity
{
    std::uint64_t value = 0;
    explicit operator bool() const { return value != 0; }
    bool operator==(const Entity &) const = default;
};
struct ModelComponent
{
    AssetHandle<ModelAsset> asset;
    bool visible = true;
    bool castShadows = true;
    // Per imported material slot. Empty handles preserve the model's imported material.
    std::vector<AssetHandle<MaterialAsset>> materialOverrides;
};
struct RenderableComponent
{
    MeshHandle mesh;
    MaterialHandle material;
    bool visible = true, castShadows = true;
    std::uint8_t queue = 10;
};
struct LightComponent
{
    // World-space direction. Position is supplied by the entity's resolved Transform.
    LightDesc light;
};
struct CameraComponent
{
    // Look-at target remains world-space; position comes from the resolved Transform.
    Vec3 target{0, 0, 0};
    float fov = 60, nearClip = .1f, farClip = 1000;
    float aspect = 0; // Zero follows the render viewport; positive values override it.
};
struct TransformUpdate { Entity entity; Transform transform; };
struct EntityRecord
{
    Entity entity;
    std::string name;
    Transform transform; // Resolved world-space transform.
    std::optional<RenderableComponent> renderable;
    std::optional<LightComponent> light;
    std::optional<CameraComponent> camera;
    std::optional<PhysicsComponent> physics;
    std::optional<ModelComponent> model;
    std::optional<AnimatorComponent> animator;
};
// Main-thread ownership. Public reads return copies, never EnTT references.
// Registry mutation is not thread safe; jobs may operate on owned snapshots.
class World
{
  public:
    World();
    ~World();
    World(const World &) = delete;
    World &operator=(const World &) = delete;
    Entity Create(std::string name = "Entity");
    void Destroy(Entity);
    void Clear();
    bool Alive(Entity) const;
    std::size_t Size() const;
    EntityRecord Read(Entity) const;

    // Existing transform API remains world-space for compatibility. Parented entities
    // derive their local transform from world writes; descendant world transforms propagate.
    Transform ReadTransform(Entity) const;
    void SetTransform(Entity, const Transform &world);
    Transform ReadLocalTransform(Entity) const;
    void SetLocalTransform(Entity, const Transform &local);

    std::vector<EntityRecord> Snapshot() const;
    void Rename(Entity, std::string);

    // Scene graph. Reparenting preserves the child's current world-space transform.
    Entity Parent(Entity) const;
    std::vector<Entity> Children(Entity) const;
    void SetParent(Entity child, Entity parent);
    void ClearParent(Entity child);

    // Physics and hierarchy ownership are intentionally disjoint in this phase. Attach/remove
    // physics only while the entity is a hierarchy root with no children.
    void SetPhysics(Entity, const PhysicsComponent &);
    void RemovePhysics(Entity);
    void SetModel(Entity, const ModelComponent &);
    void RemoveModel(Entity);
    void SetAnimator(Entity, const AnimatorComponent &);
    void RemoveAnimator(Entity);
    void SetRenderable(Entity, const RenderableComponent &);
    void RemoveRenderable(Entity);
    void SetLight(Entity, const LightComponent &);
    void RemoveLight(Entity);
    void SetCamera(Entity, const CameraComponent &);
    void RemoveCamera(Entity);
    void SetActiveCamera(Entity); // Entity must have a camera component.
    void ClearActiveCamera();
    Entity ActiveCamera() const;
    std::vector<TransformUpdate> TransformSnapshot() const;
    // World-space batch. Validates the complete final hierarchy before any mutation.
    // Duplicate/stale IDs are rejected and explicit child targets win over propagation.
    void ApplyTransforms(std::span<const TransformUpdate>);

  private:
    friend class SceneRenderer;
    friend class AssetScene;
    friend class TransformSystem;
    friend class PhysicsSystem;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace PlagC
