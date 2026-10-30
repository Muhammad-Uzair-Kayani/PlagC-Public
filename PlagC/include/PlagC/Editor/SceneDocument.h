#pragma once

#include <PlagC/Editor/CommandHistory.h>
#include <PlagC/World/World.h>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace PlagC::Editor {

struct EntityTemplate {
    std::string name = "Entity";
    Transform transform{};
    std::optional<RenderableComponent> renderable;
    std::optional<LightComponent> light;
    std::optional<CameraComponent> camera;
    std::optional<PhysicsComponent> physics;
    std::optional<ModelComponent> model;
    std::optional<AnimatorComponent> animator;
};

struct AuthoringEntity {
    std::uint64_t persistentID = 0;
    Entity entity{};
    EntityRecord record{};
    bool selected = false;
    bool activeCamera = false;
    std::uint64_t parentPersistentID = 0;
};

class SceneDocument {
public:
    class TransformEdit {
    public:
        TransformEdit() = default;
        ~TransformEdit();

        TransformEdit(const TransformEdit&) = delete;
        TransformEdit& operator=(const TransformEdit&) = delete;
        TransformEdit(TransformEdit&&) noexcept;
        TransformEdit& operator=(TransformEdit&&) = delete;

        void Preview(const Transform&);
        void Commit();
        void Cancel();
        bool Active() const noexcept { return document_ != nullptr; }
        Entity Target() const noexcept { return entity_; }
        const Transform& Before() const noexcept { return before_; }
        const Transform& Current() const noexcept { return current_; }

    private:
        friend class SceneDocument;
        TransformEdit(SceneDocument&, Entity, Transform);

        SceneDocument* document_ = nullptr;
        Entity entity_{};
        Transform before_{};
        Transform current_{};
    };

    using ModelResolver = std::function<AssetHandle<ModelAsset>(const std::string&)>;

    explicit SceneDocument(std::unique_ptr<World>, std::size_t historyCapacity = 256);
    ~SceneDocument();

    SceneDocument(const SceneDocument&) = delete;
    SceneDocument& operator=(const SceneDocument&) = delete;

    const World& Scene() const;
    std::vector<AuthoringEntity> Snapshot() const;
    std::uint64_t Revision() const;

    Entity Selection() const;
    void Select(Entity);
    void SelectPersistent(std::uint64_t);
    std::uint64_t PersistentID(Entity) const;
    std::uint64_t ParentPersistentID(Entity) const;
    Entity FindByPersistentID(std::uint64_t) const;

    Entity Create(EntityTemplate entity = {});
    void Destroy(Entity);
    Entity Duplicate(Entity);

    void Rename(Entity, std::string);

    // World-space authoring remains the gizmo/runtime-facing contract.
    void SetTransform(Entity, const Transform&);
    TransformEdit BeginTransformEdit(Entity);

    // Local-space authoring is intended for the Inspector on parented entities.
    // It participates in the same persistent-ID-safe undo/redo history.
    Transform LocalTransform(Entity) const;
    void SetLocalTransform(Entity, const Transform&);

    // Undoable relationship editing. Parent {} means root; reparent preserves world-space pose.
    void SetParent(Entity child, Entity parent);
    void ClearParent(Entity child);

    void SetRenderable(Entity, const RenderableComponent&);
    void RemoveRenderable(Entity);
    void SetCamera(Entity, const CameraComponent&);
    void RemoveCamera(Entity);
    void SetLight(Entity, const LightComponent&);
    void RemoveLight(Entity);
    void SetPhysics(Entity, const PhysicsComponent&);
    void RemovePhysics(Entity);
    void SetModel(Entity, const ModelComponent&);
    void RemoveModel(Entity);
    void SetAnimator(Entity, const AnimatorComponent&);
    void RemoveAnimator(Entity);
    void SetModelVisible(Entity, bool);
    void SetActiveCamera(Entity);
    void ClearActiveCamera();

    bool Undo();
    bool Redo();
    bool CanUndo() const;
    bool CanRedo() const;
    bool Dirty() const;
    bool HasTransientEdit() const;
    void MarkSaved();
    std::string_view UndoLabel() const;
    std::string_view RedoLabel() const;

    std::unique_ptr<SceneDocument> CloneForRuntime() const;

    std::string Serialize() const;
    static std::unique_ptr<SceneDocument> Deserialize(std::string_view, ModelResolver = {});
    void Save(const std::filesystem::path&);
    static std::unique_ptr<SceneDocument> Load(const std::filesystem::path&, ModelResolver = {});

private:
    friend class PlaySession;

    void RequireStableEditState() const;
    void PreviewTransform(Entity, const Transform&);
    void CommitTransformEdit(Entity, const Transform& before, const Transform& after);
    void CancelTransformEdit(Entity, const Transform& before);
    void BumpRevision() noexcept { ++revision_; }
    std::uint64_t AllocatePersistentID();

    std::unique_ptr<World> world_;
    CommandHistory history_; // destroyed before referenced world
    Entity selection_{};

    std::unordered_map<std::uint64_t, std::uint64_t> persistentIDs_;
    std::uint64_t revision_ = 1;
    std::uint64_t nextPersistentID_ = 1;

    bool transformEditActive_ = false;
    Entity transformEditEntity_{};
};

} // namespace PlagC::Editor
