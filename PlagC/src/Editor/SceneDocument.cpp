#include "pgcpch.h"
#include <PlagC/Editor/SceneDocument.h>
#include <PlagC/Thread.h>
namespace PlagC::Editor {
namespace {
class RenameCommand final : public Command {
    World &world_; Entity entity_; std::string before_, after_;
public:
    RenameCommand(World &w, Entity e, std::string before, std::string after)
        : world_(w), entity_(e), before_(std::move(before)), after_(std::move(after)) {}
    std::string_view Label() const noexcept override { return "Rename entity"; }
    void Apply() override { world_.Rename(entity_, after_); }
    void Revert() override { world_.Rename(entity_, before_); }
};
class TransformCommand final : public Command {
    World &world_; Entity entity_; Transform before_, after_;
public:
    TransformCommand(World &w, Entity e, Transform before, Transform after)
        : world_(w), entity_(e), before_(before), after_(after) {}
    std::string_view Label() const noexcept override { return "Transform entity"; }
    void Apply() override { world_.SetTransform(entity_, after_); }
    void Revert() override { world_.SetTransform(entity_, before_); }
};
bool Equal(Vec3 a, Vec3 b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
}
SceneDocument::SceneDocument(std::unique_ptr<World> world, std::size_t capacity)
    : world_(std::move(world)), history_(capacity) {
    Thread::RequireMain();
    if (!world_) throw std::invalid_argument("Scene document requires a world");
    std::uint64_t next = 1;
    for (const auto &record : world_->Snapshot()) persistentIDs_.emplace(record.entity.value, next++);
}
SceneDocument::~SceneDocument() = default;
const World& SceneDocument::Scene() const { Thread::RequireMain(); return *world_; }
Entity SceneDocument::Selection() const { Thread::RequireMain(); return selection_; }
void SceneDocument::Select(Entity e) {
    Thread::RequireMain();
    if (e && !world_->Alive(e)) throw std::invalid_argument("Invalid editor selection");
    selection_ = e;
}
void SceneDocument::Rename(Entity e, std::string name) {
    Thread::RequireMain(); auto before = world_->Read(e).name;
    if (before == name) return;
    history_.Execute(std::make_unique<RenameCommand>(*world_, e, std::move(before), std::move(name)));
}
void SceneDocument::SetTransform(Entity e, const Transform &t) {
    Thread::RequireMain(); auto before = world_->ReadTransform(e);
    if (Equal(before.position,t.position) && Equal(before.rotationDegrees,t.rotationDegrees) && Equal(before.scale,t.scale)) return;
    history_.Execute(std::make_unique<TransformCommand>(*world_, e, before, t));
}
bool SceneDocument::Undo() { Thread::RequireMain(); return history_.Undo(); }
bool SceneDocument::Redo() { Thread::RequireMain(); return history_.Redo(); }
bool SceneDocument::Dirty() const { Thread::RequireMain(); return history_.Dirty(); }
void SceneDocument::MarkSaved() { Thread::RequireMain(); history_.MarkSaved(); }
std::string_view SceneDocument::UndoLabel() const { Thread::RequireMain(); return history_.UndoLabel(); }
std::string_view SceneDocument::RedoLabel() const { Thread::RequireMain(); return history_.RedoLabel(); }
std::uint64_t SceneDocument::PersistentID(Entity entity) const {
    Thread::RequireMain();
    if (!world_->Alive(entity)) throw std::invalid_argument("Invalid persistent entity reference");
    return persistentIDs_.at(entity.value);
}
}
