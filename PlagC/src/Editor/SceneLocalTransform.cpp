#include "pgcpch.h"
#include <PlagC/Editor/SceneDocument.h>
#include <PlagC/Thread.h>

namespace PlagC::Editor {
namespace {

bool Equal(Vec3 a, Vec3 b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

bool Equal(const Transform& a, const Transform& b) {
    return Equal(a.position, b.position) &&
           Equal(a.rotationDegrees, b.rotationDegrees) &&
           Equal(a.scale, b.scale);
}

class LocalTransformCommand final : public Command {
public:
    LocalTransformCommand(World& world, const SceneDocument& document,
                          std::uint64_t persistentID,
                          Transform before, Transform after)
        : world_(world), document_(document), persistentID_(persistentID),
          before_(before), after_(after) {}

    std::string_view Label() const noexcept override { return "Local transform entity"; }

    void Apply() override {
        world_.SetLocalTransform(Resolve(), after_);
    }

    void Revert() override {
        world_.SetLocalTransform(Resolve(), before_);
    }

private:
    Entity Resolve() const {
        const Entity entity = document_.FindByPersistentID(persistentID_);
        if (!entity)
            throw std::logic_error("Editor command refers to a missing persistent entity");
        return entity;
    }

    World& world_;
    const SceneDocument& document_;
    std::uint64_t persistentID_ = 0;
    Transform before_{};
    Transform after_{};
};

} // namespace

Transform SceneDocument::LocalTransform(Entity entity) const {
    Thread::RequireMain();
    return world_->ReadLocalTransform(entity);
}

void SceneDocument::SetLocalTransform(Entity entity, const Transform& transform) {
    Thread::RequireMain();
    RequireStableEditState();
    const Transform before = world_->ReadLocalTransform(entity);
    if (Equal(before, transform))
        return;

    const auto id = PersistentID(entity);
    history_.Execute(std::make_unique<LocalTransformCommand>(
        *world_, *this, id, before, transform));
    BumpRevision();
}

} // namespace PlagC::Editor
