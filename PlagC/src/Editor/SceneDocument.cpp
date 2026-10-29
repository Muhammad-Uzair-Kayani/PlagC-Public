#include "pgcpch.h"
#include <PlagC/Editor/SceneDocument.h>
#include <PlagC/Thread.h>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace PlagC::Editor {
namespace {

using PersistentMap = std::unordered_map<std::uint64_t, std::uint64_t>;

bool Equal(Vec3 a, Vec3 b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

bool Equal(const Transform& a, const Transform& b) {
    return Equal(a.position, b.position) &&
           Equal(a.rotationDegrees, b.rotationDegrees) &&
           Equal(a.scale, b.scale);
}

bool Equal(const CameraComponent& a, const CameraComponent& b) {
    return Equal(a.target, b.target) && a.fov == b.fov &&
           a.nearClip == b.nearClip && a.farClip == b.farClip &&
           a.aspect == b.aspect;
}

bool Equal(const LightDesc& a, const LightDesc& b) {
    return a.type == b.type && Equal(a.position, b.position) &&
           Equal(a.direction, b.direction) && Equal(a.colour, b.colour) &&
           a.power == b.power && a.radius == b.radius &&
           a.innerAngle == b.innerAngle && a.outerAngle == b.outerAngle;
}

bool Equal(const LightComponent& a, const LightComponent& b) {
    return Equal(a.light, b.light);
}

bool Equal(const ModelComponent& a, const ModelComponent& b) {
    return a.asset.Shared() == b.asset.Shared() && a.visible == b.visible;
}

EntityRecord ToRecord(const EntityTemplate& value) {
    EntityRecord record{};
    record.name = value.name;
    record.transform = value.transform;
    record.renderable = value.renderable;
    record.light = value.light;
    record.camera = value.camera;
    record.physics = value.physics;
    record.model = value.model;
    record.animator = value.animator;
    return record;
}

Entity RestoreRecord(World& world, const EntityRecord& record, bool activeCamera) {
    const Entity entity = world.Create(record.name);
    try {
        world.SetTransform(entity, record.transform);
        if (record.renderable)
            world.SetRenderable(entity, *record.renderable);
        if (record.light)
            world.SetLight(entity, *record.light);
        if (record.model)
            world.SetModel(entity, *record.model);
        if (record.animator)
            world.SetAnimator(entity, *record.animator);
        if (record.camera)
            world.SetCamera(entity, *record.camera);
        if (record.physics)
            world.SetPhysics(entity, *record.physics);
        if (activeCamera)
            world.SetActiveCamera(entity);
    } catch (...) {
        world.Destroy(entity);
        throw;
    }
    return entity;
}

void Bind(PersistentMap& persistent, std::uint64_t persistentID, Entity entity) {
    auto [it, inserted] = persistent.emplace(entity.value, persistentID);
    if (!inserted)
        throw std::logic_error("Runtime entity already has a persistent ID");
    (void)it;
}

void Unbind(PersistentMap& persistent, Entity entity) noexcept {
    persistent.erase(entity.value);
}

std::uint64_t PersistentOf(const PersistentMap& persistent, Entity entity) {
    if (!entity)
        return 0;
    const auto it = persistent.find(entity.value);
    if (it == persistent.end())
        throw std::logic_error("Scene document lost an entity persistent ID");
    return it->second;
}

Entity Resolve(const PersistentMap& persistent, std::uint64_t persistentID) {
    for (const auto& [runtimeID, id] : persistent)
        if (id == persistentID)
            return Entity{runtimeID};
    throw std::logic_error("Editor command refers to a missing persistent entity");
}

class RenameCommand final : public Command {
public:
    RenameCommand(World& world, PersistentMap& persistent, std::uint64_t id,
                  std::string before, std::string after)
        : world_(world), persistent_(persistent), id_(id),
          before_(std::move(before)), after_(std::move(after)) {}

    std::string_view Label() const noexcept override { return "Rename entity"; }
    void Apply() override { world_.Rename(Resolve(persistent_, id_), after_); }
    void Revert() override { world_.Rename(Resolve(persistent_, id_), before_); }

private:
    World& world_;
    PersistentMap& persistent_;
    std::uint64_t id_;
    std::string before_;
    std::string after_;
};

class TransformCommand final : public Command {
public:
    TransformCommand(World& world, PersistentMap& persistent, std::uint64_t id,
                     Transform before, Transform after)
        : world_(world), persistent_(persistent), id_(id),
          before_(before), after_(after) {}

    std::string_view Label() const noexcept override { return "Transform entity"; }
    void Apply() override { world_.SetTransform(Resolve(persistent_, id_), after_); }
    void Revert() override { world_.SetTransform(Resolve(persistent_, id_), before_); }

private:
    World& world_;
    PersistentMap& persistent_;
    std::uint64_t id_;
    Transform before_;
    Transform after_;
};

class ReparentCommand final : public Command {
public:
    ReparentCommand(World& world, PersistentMap& persistent,
                    std::uint64_t child, std::uint64_t before, std::uint64_t after)
        : world_(world), persistent_(persistent), child_(child), before_(before), after_(after) {}

    std::string_view Label() const noexcept override { return "Reparent entity"; }
    void Apply() override { Set(after_); }
    void Revert() override { Set(before_); }

private:
    void Set(std::uint64_t parent) {
        const Entity child = Resolve(persistent_, child_);
        if (parent)
            world_.SetParent(child, Resolve(persistent_, parent));
        else
            world_.ClearParent(child);
    }

    World& world_;
    PersistentMap& persistent_;
    std::uint64_t child_ = 0;
    std::uint64_t before_ = 0;
    std::uint64_t after_ = 0;
};

class CreateEntityCommand final : public Command {
public:
    CreateEntityCommand(World& world, PersistentMap& persistent,
                        Entity& selection, std::uint64_t id, EntityRecord record)
        : world_(world), persistent_(persistent),
          selection_(selection), previousSelection_(selection),
          id_(id), record_(std::move(record)) {}

    std::string_view Label() const noexcept override { return "Create entity"; }

    void Apply() override {
        const Entity created = RestoreRecord(world_, record_, false);
        try {
            Bind(persistent_, id_, created);
        } catch (...) {
            world_.Destroy(created);
            throw;
        }
        entity_ = created;
        selection_ = entity_;
    }

    void Revert() override {
        if (!entity_ || !world_.Alive(entity_))
            throw std::logic_error("Created entity no longer exists");
        if (selection_ == entity_)
            selection_ = previousSelection_ && world_.Alive(previousSelection_)
                             ? previousSelection_ : Entity{};
        world_.Destroy(entity_);
        Unbind(persistent_, entity_);
        entity_ = {};
    }

    Entity Current() const noexcept { return entity_; }

private:
    World& world_;
    PersistentMap& persistent_;
    Entity& selection_;
    Entity previousSelection_{};
    std::uint64_t id_;
    EntityRecord record_;
    Entity entity_{};
};

class DeleteEntityCommand final : public Command {
public:
    DeleteEntityCommand(World& world, PersistentMap& persistent,
                        Entity& selection, Entity entity, std::uint64_t id)
        : world_(world), persistent_(persistent),
          selection_(selection), id_(id), entity_(entity),
          record_(world.Read(entity)),
          activeCamera_(world.ActiveCamera() == entity),
          selected_(selection == entity),
          parentID_(PersistentOf(persistent, world.Parent(entity))) {
        for (const Entity child : world.Children(entity))
            childIDs_.push_back(PersistentOf(persistent, child));
    }

    std::string_view Label() const noexcept override { return "Delete entity"; }

    void Apply() override {
        if (!entity_ || !world_.Alive(entity_))
            throw std::logic_error("Entity to delete no longer exists");
        if (selection_ == entity_)
            selection_ = {};
        world_.Destroy(entity_);
        Unbind(persistent_, entity_);
        entity_ = {};
    }

    void Revert() override {
        const Entity parent = parentID_ ? Resolve(persistent_, parentID_) : Entity{};
        std::vector<Entity> children;
        children.reserve(childIDs_.size());
        for (const auto childID : childIDs_)
            children.push_back(Resolve(persistent_, childID));

        const Entity restored = RestoreRecord(world_, record_, activeCamera_);
        try {
            Bind(persistent_, id_, restored);
            if (parent)
                world_.SetParent(restored, parent);
            for (const Entity child : children)
                world_.SetParent(child, restored);
        } catch (...) {
            for (const Entity child : children)
                if (world_.Alive(child) && world_.Parent(child) == restored)
                    world_.ClearParent(child);
            Unbind(persistent_, restored);
            if (world_.Alive(restored))
                world_.Destroy(restored);
            throw;
        }
        entity_ = restored;
        if (selected_)
            selection_ = entity_;
    }

private:
    World& world_;
    PersistentMap& persistent_;
    Entity& selection_;
    std::uint64_t id_;
    Entity entity_{};
    EntityRecord record_;
    bool activeCamera_ = false;
    bool selected_ = false;
    std::uint64_t parentID_ = 0;
    std::vector<std::uint64_t> childIDs_;
};

class DuplicateEntityCommand final : public Command {
public:
    DuplicateEntityCommand(World& world, PersistentMap& persistent,
                           Entity& selection, std::uint64_t id, EntityRecord record,
                           std::uint64_t parentID)
        : world_(world), persistent_(persistent),
          selection_(selection), previousSelection_(selection),
          id_(id), record_(std::move(record)), parentID_(parentID) {}

    std::string_view Label() const noexcept override { return "Duplicate entity"; }

    void Apply() override {
        const Entity parent = parentID_ ? Resolve(persistent_, parentID_) : Entity{};
        const Entity created = RestoreRecord(world_, record_, false);
        try {
            Bind(persistent_, id_, created);
            if (parent)
                world_.SetParent(created, parent);
        } catch (...) {
            Unbind(persistent_, created);
            world_.Destroy(created);
            throw;
        }
        entity_ = created;
        selection_ = entity_;
    }

    void Revert() override {
        if (!entity_ || !world_.Alive(entity_))
            throw std::logic_error("Duplicated entity no longer exists");
        if (selection_ == entity_)
            selection_ = previousSelection_ && world_.Alive(previousSelection_)
                           ? previousSelection_ : Entity{};
        world_.Destroy(entity_);
        Unbind(persistent_, entity_);
        entity_ = {};
    }

    Entity Current() const noexcept { return entity_; }

private:
    World& world_;
    PersistentMap& persistent_;
    Entity& selection_;
    Entity previousSelection_{};
    std::uint64_t id_;
    EntityRecord record_;
    std::uint64_t parentID_ = 0;
    Entity entity_{};
};

class CameraCommand final : public Command {
public:
    CameraCommand(World& world, PersistentMap& persistent, std::uint64_t id,
                  std::optional<CameraComponent> before,
                  std::optional<CameraComponent> after,
                  bool wasActive)
        : world_(world), persistent_(persistent), id_(id),
          before_(before), after_(after), wasActive_(wasActive) {}

    std::string_view Label() const noexcept override { return "Edit camera"; }

    void Apply() override { Set(after_, false); }
    void Revert() override { Set(before_, wasActive_); }

private:
    void Set(const std::optional<CameraComponent>& value, bool makeActive) {
        const Entity entity = Resolve(persistent_, id_);
        if (value)
            world_.SetCamera(entity, *value);
        else
            world_.RemoveCamera(entity);
        if (makeActive)
            world_.SetActiveCamera(entity);
    }

    World& world_;
    PersistentMap& persistent_;
    std::uint64_t id_;
    std::optional<CameraComponent> before_;
    std::optional<CameraComponent> after_;
    bool wasActive_ = false;
};

class LightCommand final : public Command {
public:
    LightCommand(World& world, PersistentMap& persistent, std::uint64_t id,
                 std::optional<LightComponent> before,
                 std::optional<LightComponent> after)
        : world_(world), persistent_(persistent), id_(id),
          before_(before), after_(after) {}

    std::string_view Label() const noexcept override { return "Edit light"; }
    void Apply() override { Set(after_); }
    void Revert() override { Set(before_); }

private:
    void Set(const std::optional<LightComponent>& value) {
        const Entity entity = Resolve(persistent_, id_);
        if (value)
            world_.SetLight(entity, *value);
        else
            world_.RemoveLight(entity);
    }

    World& world_;
    PersistentMap& persistent_;
    std::uint64_t id_;
    std::optional<LightComponent> before_;
    std::optional<LightComponent> after_;
};

class ModelCommand final : public Command {
public:
    ModelCommand(World& world, PersistentMap& persistent, std::uint64_t id,
                 std::optional<ModelComponent> before,
                 std::optional<ModelComponent> after,
                 std::optional<AnimatorComponent> beforeAnimator)
        : world_(world), persistent_(persistent), id_(id),
          before_(std::move(before)), after_(std::move(after)),
          beforeAnimator_(std::move(beforeAnimator)) {}

    std::string_view Label() const noexcept override { return "Edit model"; }
    void Apply() override { Set(after_, false); }
    void Revert() override { Set(before_, true); }

private:
    void Set(const std::optional<ModelComponent>& value, bool restoringBefore) {
        const Entity entity = Resolve(persistent_, id_);
        if (value) {
            world_.SetModel(entity, *value);
            if (restoringBefore && beforeAnimator_)
                world_.SetAnimator(entity, *beforeAnimator_);
        } else {
            world_.RemoveModel(entity);
        }
    }

    World& world_;
    PersistentMap& persistent_;
    std::uint64_t id_;
    std::optional<ModelComponent> before_;
    std::optional<ModelComponent> after_;
    std::optional<AnimatorComponent> beforeAnimator_;
};

class ActiveCameraCommand final : public Command {
public:
    ActiveCameraCommand(World& world, PersistentMap& persistent,
                        std::uint64_t before, std::uint64_t after)
        : world_(world), persistent_(persistent), before_(before), after_(after) {}

    std::string_view Label() const noexcept override { return "Set active camera"; }
    void Apply() override { Set(after_); }
    void Revert() override { Set(before_); }

private:
    void Set(std::uint64_t id) {
        if (!id)
            world_.ClearActiveCamera();
        else
            world_.SetActiveCamera(Resolve(persistent_, id));
    }

    World& world_;
    PersistentMap& persistent_;
    std::uint64_t before_ = 0;
    std::uint64_t after_ = 0;
};

} // namespace

SceneDocument::TransformEdit::TransformEdit(SceneDocument& document, Entity entity, Transform before)
    : document_(&document), entity_(entity), before_(before), current_(before) {}

SceneDocument::TransformEdit::~TransformEdit() {
    if (!document_)
        return;
    try {
        Cancel();
    } catch (...) {
        // Destructors cannot report editor cancellation failures. The explicit
        // Cancel/Commit paths remain exception-reporting.
    }
}

SceneDocument::TransformEdit::TransformEdit(TransformEdit&& other) noexcept
    : document_(other.document_), entity_(other.entity_),
      before_(other.before_), current_(other.current_) {
    other.document_ = nullptr;
    other.entity_ = {};
}

void SceneDocument::TransformEdit::Preview(const Transform& value) {
    if (!document_)
        throw std::logic_error("Transform edit is no longer active");
    document_->PreviewTransform(entity_, value);
    current_ = value;
}

void SceneDocument::TransformEdit::Commit() {
    if (!document_)
        throw std::logic_error("Transform edit is no longer active");
    document_->CommitTransformEdit(entity_, before_, current_);
    document_ = nullptr;
    entity_ = {};
}

void SceneDocument::TransformEdit::Cancel() {
    if (!document_)
        return;
    document_->CancelTransformEdit(entity_, before_);
    document_ = nullptr;
    entity_ = {};
}

SceneDocument::SceneDocument(std::unique_ptr<World> world, std::size_t capacity)
    : world_(std::move(world)), history_(capacity) {
    Thread::RequireMain();
    if (!world_)
        throw std::invalid_argument("Scene document requires a world");

    std::uint64_t persistentID = 1;
    for (const auto& record : world_->Snapshot())
        Bind(persistentIDs_, persistentID++, record.entity);
    nextPersistentID_ = persistentID;
}

SceneDocument::~SceneDocument() = default;

const World& SceneDocument::Scene() const {
    Thread::RequireMain();
    return *world_;
}

std::vector<AuthoringEntity> SceneDocument::Snapshot() const {
    Thread::RequireMain();
    std::vector<AuthoringEntity> result;
    const auto records = world_->Snapshot();
    result.reserve(records.size());
    const Entity active = world_->ActiveCamera();

    for (const auto& record : records) {
        const auto it = persistentIDs_.find(record.entity.value);
        if (it == persistentIDs_.end())
            throw std::logic_error("Scene document lost an entity persistent ID");
        const Entity parent = world_->Parent(record.entity);
        const std::uint64_t parentID = parent ? PersistentID(parent) : 0;
        result.push_back({it->second, record.entity, record,
                          selection_ == record.entity, active == record.entity, parentID});
    }

    std::sort(result.begin(), result.end(),
              [](const AuthoringEntity& a, const AuthoringEntity& b) {
                  return a.persistentID < b.persistentID;
              });
    return result;
}

std::uint64_t SceneDocument::Revision() const {
    Thread::RequireMain();
    return revision_;
}

Entity SceneDocument::Selection() const {
    Thread::RequireMain();
    return selection_;
}

void SceneDocument::Select(Entity entity) {
    Thread::RequireMain();
    RequireStableEditState();
    if (entity && !world_->Alive(entity))
        throw std::invalid_argument("Invalid editor selection");
    if (selection_ == entity)
        return;
    selection_ = entity;
    BumpRevision();
}

void SceneDocument::SelectPersistent(std::uint64_t persistentID) {
    Thread::RequireMain();
    if (!persistentID) {
        Select({});
        return;
    }
    const Entity entity = FindByPersistentID(persistentID);
    if (!entity)
        throw std::invalid_argument("Unknown persistent editor selection");
    Select(entity);
}

std::uint64_t SceneDocument::PersistentID(Entity entity) const {
    Thread::RequireMain();
    if (!world_->Alive(entity))
        throw std::invalid_argument("Invalid persistent entity reference");
    const auto it = persistentIDs_.find(entity.value);
    if (it == persistentIDs_.end())
        throw std::logic_error("Scene document lost an entity persistent ID");
    return it->second;
}

std::uint64_t SceneDocument::ParentPersistentID(Entity entity) const {
    Thread::RequireMain();
    const Entity parent = world_->Parent(entity);
    return parent ? PersistentID(parent) : 0;
}

Entity SceneDocument::FindByPersistentID(std::uint64_t persistentID) const {
    Thread::RequireMain();
    for (const auto& [runtimeID, id] : persistentIDs_)
        if (id == persistentID)
            return Entity{runtimeID};
    return {};
}

Entity SceneDocument::Create(EntityTemplate value) {
    Thread::RequireMain();
    RequireStableEditState();

    const std::uint64_t persistentID = AllocatePersistentID();
    auto command = std::make_unique<CreateEntityCommand>(
        *world_, persistentIDs_, selection_, persistentID, ToRecord(value));
    auto* raw = command.get();
    history_.Execute(std::move(command));
    BumpRevision();
    return raw->Current();
}

void SceneDocument::Destroy(Entity entity) {
    Thread::RequireMain();
    RequireStableEditState();
    const std::uint64_t persistentID = PersistentID(entity);
    history_.Execute(std::make_unique<DeleteEntityCommand>(
        *world_, persistentIDs_, selection_, entity, persistentID));
    BumpRevision();
}

Entity SceneDocument::Duplicate(Entity entity) {
    Thread::RequireMain();
    RequireStableEditState();
    if (!world_->Alive(entity))
        throw std::invalid_argument("Invalid entity to duplicate");

    const std::uint64_t persistentID = AllocatePersistentID();
    auto command = std::make_unique<DuplicateEntityCommand>(
        *world_, persistentIDs_, selection_, persistentID,
        world_->Read(entity), ParentPersistentID(entity));
    auto* raw = command.get();
    history_.Execute(std::move(command));
    BumpRevision();
    return raw->Current();
}

void SceneDocument::Rename(Entity entity, std::string name) {
    Thread::RequireMain();
    RequireStableEditState();
    const auto before = world_->Read(entity).name;
    if (before == name)
        return;
    const auto id = PersistentID(entity);
    history_.Execute(std::make_unique<RenameCommand>(
        *world_, persistentIDs_, id, before, std::move(name)));
    BumpRevision();
}

void SceneDocument::SetTransform(Entity entity, const Transform& transform) {
    Thread::RequireMain();
    RequireStableEditState();
    const auto before = world_->ReadTransform(entity);
    if (Equal(before, transform))
        return;
    const auto id = PersistentID(entity);
    history_.Execute(std::make_unique<TransformCommand>(
        *world_, persistentIDs_, id, before, transform));
    BumpRevision();
}

SceneDocument::TransformEdit SceneDocument::BeginTransformEdit(Entity entity) {
    Thread::RequireMain();
    RequireStableEditState();
    if (!world_->Alive(entity))
        throw std::invalid_argument("Invalid transform edit entity");
    const auto record = world_->Read(entity);
    if (record.physics && record.physics->body.motion == MotionType::Dynamic)
        throw std::invalid_argument("Dynamic transforms are physics-owned");
    transformEditActive_ = true;
    transformEditEntity_ = entity;
    return TransformEdit(*this, entity, world_->ReadTransform(entity));
}

void SceneDocument::SetParent(Entity child, Entity parent) {
    Thread::RequireMain();
    RequireStableEditState();
    if (!world_->Alive(child))
        throw std::invalid_argument("Invalid hierarchy child");
    if (parent && !world_->Alive(parent))
        throw std::invalid_argument("Invalid hierarchy parent");

    const std::uint64_t before = ParentPersistentID(child);
    const std::uint64_t after = parent ? PersistentID(parent) : 0;
    if (before == after)
        return;

    const auto childID = PersistentID(child);
    history_.Execute(std::make_unique<ReparentCommand>(
        *world_, persistentIDs_, childID, before, after));
    BumpRevision();
}

void SceneDocument::ClearParent(Entity child) {
    SetParent(child, {});
}

void SceneDocument::SetCamera(Entity entity, const CameraComponent& camera) {
    Thread::RequireMain();
    RequireStableEditState();
    const auto record = world_->Read(entity);
    if (record.camera && Equal(*record.camera, camera))
        return;
    const auto id = PersistentID(entity);
    history_.Execute(std::make_unique<CameraCommand>(
        *world_, persistentIDs_, id, record.camera,
        std::optional<CameraComponent>(camera),
        world_->ActiveCamera() == entity));
    BumpRevision();
}

void SceneDocument::RemoveCamera(Entity entity) {
    Thread::RequireMain();
    RequireStableEditState();
    const auto record = world_->Read(entity);
    if (!record.camera)
        return;
    const auto id = PersistentID(entity);
    history_.Execute(std::make_unique<CameraCommand>(
        *world_, persistentIDs_, id, record.camera,
        std::optional<CameraComponent>{},
        world_->ActiveCamera() == entity));
    BumpRevision();
}

void SceneDocument::SetLight(Entity entity, const LightComponent& light) {
    Thread::RequireMain();
    RequireStableEditState();
    const auto record = world_->Read(entity);
    if (record.light && Equal(*record.light, light))
        return;
    const auto id = PersistentID(entity);
    history_.Execute(std::make_unique<LightCommand>(
        *world_, persistentIDs_, id, record.light,
        std::optional<LightComponent>(light)));
    BumpRevision();
}

void SceneDocument::RemoveLight(Entity entity) {
    Thread::RequireMain();
    RequireStableEditState();
    const auto record = world_->Read(entity);
    if (!record.light)
        return;
    const auto id = PersistentID(entity);
    history_.Execute(std::make_unique<LightCommand>(
        *world_, persistentIDs_, id, record.light,
        std::optional<LightComponent>{}));
    BumpRevision();
}

void SceneDocument::SetModel(Entity entity, const ModelComponent& model) {
    Thread::RequireMain();
    RequireStableEditState();
    const auto record = world_->Read(entity);
    if (record.model && Equal(*record.model, model))
        return;
    if (record.animator && record.model &&
        record.model->asset.Shared() != model.asset.Shared())
        throw std::logic_error("Remove the animator before replacing its model asset");
    const auto id = PersistentID(entity);
    history_.Execute(std::make_unique<ModelCommand>(
        *world_, persistentIDs_, id, record.model,
        std::optional<ModelComponent>(model), record.animator));
    BumpRevision();
}

void SceneDocument::RemoveModel(Entity entity) {
    Thread::RequireMain();
    RequireStableEditState();
    const auto record = world_->Read(entity);
    if (!record.model)
        return;
    const auto id = PersistentID(entity);
    history_.Execute(std::make_unique<ModelCommand>(
        *world_, persistentIDs_, id, record.model,
        std::optional<ModelComponent>{}, record.animator));
    BumpRevision();
}

void SceneDocument::SetModelVisible(Entity entity, bool visible) {
    Thread::RequireMain();
    RequireStableEditState();
    const auto record = world_->Read(entity);
    if (!record.model)
        throw std::invalid_argument("Entity has no model component");
    auto model = *record.model;
    if (model.visible == visible)
        return;
    model.visible = visible;
    SetModel(entity, model);
}

void SceneDocument::SetActiveCamera(Entity entity) {
    Thread::RequireMain();
    RequireStableEditState();
    const Entity before = world_->ActiveCamera();
    if (before == entity)
        return;
    if (!world_->Read(entity).camera)
        throw std::invalid_argument("Active camera entity has no camera component");

    const auto beforeID = before ? PersistentID(before) : 0;
    const auto afterID = PersistentID(entity);
    history_.Execute(std::make_unique<ActiveCameraCommand>(
        *world_, persistentIDs_, beforeID, afterID));
    BumpRevision();
}

void SceneDocument::ClearActiveCamera() {
    Thread::RequireMain();
    RequireStableEditState();
    const Entity before = world_->ActiveCamera();
    if (!before)
        return;
    const auto beforeID = PersistentID(before);
    history_.Execute(std::make_unique<ActiveCameraCommand>(
        *world_, persistentIDs_, beforeID, 0));
    BumpRevision();
}

bool SceneDocument::Undo() {
    Thread::RequireMain();
    RequireStableEditState();
    if (!history_.Undo())
        return false;
    BumpRevision();
    return true;
}

bool SceneDocument::Redo() {
    Thread::RequireMain();
    RequireStableEditState();
    if (!history_.Redo())
        return false;
    BumpRevision();
    return true;
}

bool SceneDocument::CanUndo() const {
    Thread::RequireMain();
    return !transformEditActive_ && history_.CanUndo();
}

bool SceneDocument::CanRedo() const {
    Thread::RequireMain();
    return !transformEditActive_ && history_.CanRedo();
}

bool SceneDocument::Dirty() const {
    Thread::RequireMain();
    return transformEditActive_ || history_.Dirty();
}

bool SceneDocument::HasTransientEdit() const {
    Thread::RequireMain();
    return transformEditActive_;
}

void SceneDocument::MarkSaved() {
    Thread::RequireMain();
    RequireStableEditState();
    history_.MarkSaved();
}

std::string_view SceneDocument::UndoLabel() const {
    Thread::RequireMain();
    return history_.UndoLabel();
}

std::string_view SceneDocument::RedoLabel() const {
    Thread::RequireMain();
    return history_.RedoLabel();
}

std::unique_ptr<SceneDocument> SceneDocument::CloneForRuntime() const {
    Thread::RequireMain();
    RequireStableEditState();

    auto clonedWorld = std::make_unique<World>();
    std::unordered_map<std::uint64_t, Entity> clonedEntities;
    const auto authoring = Snapshot();
    clonedEntities.reserve(authoring.size());

    const Entity active = world_->ActiveCamera();
    std::uint64_t activeID = 0;

    for (const auto& item : authoring) {
        const Entity cloned = RestoreRecord(*clonedWorld, item.record, false);
        clonedEntities.emplace(item.persistentID, cloned);
        if (item.entity == active)
            activeID = item.persistentID;
    }

    for (const auto& item : authoring)
        if (item.parentPersistentID)
            clonedWorld->SetParent(clonedEntities.at(item.persistentID),
                                   clonedEntities.at(item.parentPersistentID));

    if (activeID)
        clonedWorld->SetActiveCamera(clonedEntities.at(activeID));

    auto document = std::make_unique<SceneDocument>(std::move(clonedWorld));
    document->persistentIDs_.clear();

    for (const auto& [persistentID, entity] : clonedEntities)
        Bind(document->persistentIDs_, persistentID, entity);

    document->revision_ = revision_;
    document->nextPersistentID_ = nextPersistentID_;
    document->history_.Clear();
    document->selection_ = {};
    return document;
}

void SceneDocument::RequireStableEditState() const {
    if (transformEditActive_)
        throw std::logic_error("Finish or cancel the active transform edit first");
}

void SceneDocument::PreviewTransform(Entity entity, const Transform& transform) {
    Thread::RequireMain();
    if (!transformEditActive_ || transformEditEntity_ != entity)
        throw std::logic_error("Transform preview does not own the active edit");
    world_->SetTransform(entity, transform);
    BumpRevision();
}

void SceneDocument::CommitTransformEdit(Entity entity,
                                        const Transform& before,
                                        const Transform& after) {
    Thread::RequireMain();
    if (!transformEditActive_ || transformEditEntity_ != entity)
        throw std::logic_error("Transform commit does not own the active edit");
    if (!Equal(world_->ReadTransform(entity), after))
        throw std::logic_error("Transform edit current value is inconsistent");

    if (!Equal(before, after)) {
        const auto id = PersistentID(entity);
        history_.RecordApplied(std::make_unique<TransformCommand>(
            *world_, persistentIDs_, id, before, after));
    }

    transformEditActive_ = false;
    transformEditEntity_ = {};
    BumpRevision();
}

void SceneDocument::CancelTransformEdit(Entity entity, const Transform& before) {
    Thread::RequireMain();
    if (!transformEditActive_ || transformEditEntity_ != entity)
        throw std::logic_error("Transform cancel does not own the active edit");
    world_->SetTransform(entity, before);
    transformEditActive_ = false;
    transformEditEntity_ = {};
    BumpRevision();
}

std::uint64_t SceneDocument::AllocatePersistentID() {
    Thread::RequireMain();

    if (nextPersistentID_ == 0)
        throw std::overflow_error("Persistent entity ID space exhausted");

    for (const auto& [runtimeID, persistentID] : persistentIDs_) {
        (void)runtimeID;
        if (persistentID >= nextPersistentID_) {
            if (persistentID == std::numeric_limits<std::uint64_t>::max())
                throw std::overflow_error("Persistent entity ID space exhausted");
            nextPersistentID_ = persistentID + 1;
        }
    }

    const auto issued = nextPersistentID_;
    if (nextPersistentID_ == std::numeric_limits<std::uint64_t>::max())
        nextPersistentID_ = 0;
    else
        ++nextPersistentID_;
    return issued;
}

} // namespace PlagC::Editor
