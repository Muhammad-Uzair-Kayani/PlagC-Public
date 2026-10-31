#include "pgcpch.h"
#include <PlagC/Editor/SceneDocument.h>
#include <PlagC/Thread.h>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace PlagC::Editor {
namespace {

class ComponentCommand final : public Command {
public:
    ComponentCommand(std::string label, std::function<void()> apply,
                     std::function<void()> revert)
        : label_(std::move(label)), apply_(std::move(apply)), revert_(std::move(revert))
    {
        if (!apply_ || !revert_)
            throw std::invalid_argument("Component command requires apply and revert callbacks");
    }

    std::string_view Label() const noexcept override { return label_; }
    void Apply() override { apply_(); }
    void Revert() override { revert_(); }

private:
    std::string label_;
    std::function<void()> apply_;
    std::function<void()> revert_;
};

template <class Component>
std::string ComponentLabel(std::string_view name,
                           const std::optional<Component>& before,
                           const std::optional<Component>& after)
{
    if (!before && after)
        return "Add " + std::string(name);
    if (before && !after)
        return "Remove " + std::string(name);
    return "Edit " + std::string(name);
}

template <class Component, class Setter, class Remover>
std::unique_ptr<Command> MakeComponentCommand(
    SceneDocument* document, std::uint64_t persistentID, std::string label,
    std::optional<Component> before, std::optional<Component> after,
    Setter set, Remover remove)
{
    auto write = [document, persistentID, set, remove](const std::optional<Component>& value) {
        const Entity entity = document->FindByPersistentID(persistentID);
        if (!entity)
            throw std::logic_error("Editor component command refers to a missing entity");
        if (value)
            set(entity, *value);
        else
            remove(entity);
    };

    return std::make_unique<ComponentCommand>(
        std::move(label),
        [write, after = std::move(after)]() { write(after); },
        [write, before = std::move(before)]() { write(before); });
}

} // namespace

void SceneDocument::SetRenderable(Entity entity, const RenderableComponent& renderable)
{
    Thread::RequireMain();
    RequireStableEditState();
    const auto record = world_->Read(entity);
    const auto id = PersistentID(entity);
    const std::optional<RenderableComponent> after(renderable);
    history_.Execute(MakeComponentCommand<RenderableComponent>(
        this, id, ComponentLabel("renderable", record.renderable, after), record.renderable, after,
        [this](Entity target, const RenderableComponent& value) { world_->SetRenderable(target, value); },
        [this](Entity target) { world_->RemoveRenderable(target); }));
    BumpRevision();
}

void SceneDocument::RemoveRenderable(Entity entity)
{
    Thread::RequireMain();
    RequireStableEditState();
    const auto record = world_->Read(entity);
    if (!record.renderable)
        return;
    const auto id = PersistentID(entity);
    history_.Execute(MakeComponentCommand<RenderableComponent>(
        this, id, "Remove renderable", record.renderable, std::nullopt,
        [this](Entity target, const RenderableComponent& value) { world_->SetRenderable(target, value); },
        [this](Entity target) { world_->RemoveRenderable(target); }));
    BumpRevision();
}

void SceneDocument::SetPhysics(Entity entity, const PhysicsComponent& physics)
{
    Thread::RequireMain();
    RequireStableEditState();
    const auto record = world_->Read(entity);
    const auto id = PersistentID(entity);
    const std::optional<PhysicsComponent> after(physics);
    history_.Execute(MakeComponentCommand<PhysicsComponent>(
        this, id, ComponentLabel("physics", record.physics, after), record.physics, after,
        [this](Entity target, const PhysicsComponent& value) { world_->SetPhysics(target, value); },
        [this](Entity target) { world_->RemovePhysics(target); }));
    BumpRevision();
}

void SceneDocument::RemovePhysics(Entity entity)
{
    Thread::RequireMain();
    RequireStableEditState();
    const auto record = world_->Read(entity);
    if (!record.physics)
        return;
    const auto id = PersistentID(entity);
    history_.Execute(MakeComponentCommand<PhysicsComponent>(
        this, id, "Remove physics", record.physics, std::nullopt,
        [this](Entity target, const PhysicsComponent& value) { world_->SetPhysics(target, value); },
        [this](Entity target) { world_->RemovePhysics(target); }));
    BumpRevision();
}

void SceneDocument::SetAnimator(Entity entity, const AnimatorComponent& animator)
{
    Thread::RequireMain();
    RequireStableEditState();
    const auto record = world_->Read(entity);
    const auto id = PersistentID(entity);
    const std::optional<AnimatorComponent> after(animator);
    history_.Execute(MakeComponentCommand<AnimatorComponent>(
        this, id, ComponentLabel("animator", record.animator, after), record.animator, after,
        [this](Entity target, const AnimatorComponent& value) { world_->SetAnimator(target, value); },
        [this](Entity target) { world_->RemoveAnimator(target); }));
    BumpRevision();
}

void SceneDocument::RemoveAnimator(Entity entity)
{
    Thread::RequireMain();
    RequireStableEditState();
    const auto record = world_->Read(entity);
    if (!record.animator)
        return;
    const auto id = PersistentID(entity);
    history_.Execute(MakeComponentCommand<AnimatorComponent>(
        this, id, "Remove animator", record.animator, std::nullopt,
        [this](Entity target, const AnimatorComponent& value) { world_->SetAnimator(target, value); },
        [this](Entity target) { world_->RemoveAnimator(target); }));
    BumpRevision();
}

} // namespace PlagC::Editor
