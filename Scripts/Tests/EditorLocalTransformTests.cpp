#include <PlagC/Editor/SceneDocument.h>
#include <PlagC/Thread.h>
#include <cassert>
#include <cmath>

using namespace PlagC;
using namespace PlagC::Editor;

namespace {

bool Near(float a, float b, float epsilon = 1.0e-3f) {
    return std::fabs(a - b) <= epsilon;
}

} // namespace

int main() {
    Thread::InitializeMain();

    auto world = std::make_unique<World>();
    const Entity root = world->Create("Root");
    const Entity child = world->Create("Child");

    Transform rootWorld;
    rootWorld.position = {10.0f, 0.0f, 0.0f};
    world->SetTransform(root, rootWorld);
    Transform childWorld;
    childWorld.position = {12.0f, 0.0f, 0.0f};
    world->SetTransform(child, childWorld);
    world->SetParent(child, root);

    SceneDocument document(std::move(world));
    const std::uint64_t childPersistentID = document.PersistentID(child);
    const Transform initialLocal = document.LocalTransform(child);
    assert(Near(initialLocal.position.x, 2.0f));

    Transform editedLocal = initialLocal;
    editedLocal.position.x = 5.0f;
    document.SetLocalTransform(child, editedLocal);
    assert(Near(document.LocalTransform(child).position.x, 5.0f));
    assert(Near(document.Scene().ReadTransform(child).position.x, 15.0f));
    assert(document.CanUndo());
    assert(document.UndoLabel() == "Local transform entity");

    assert(document.Undo());
    assert(Near(document.LocalTransform(child).position.x, 2.0f));
    assert(Near(document.Scene().ReadTransform(child).position.x, 12.0f));

    assert(document.Redo());
    assert(Near(document.LocalTransform(child).position.x, 5.0f));
    assert(Near(document.Scene().ReadTransform(child).position.x, 15.0f));

    // Delete/undo recreates a runtime entity handle. The older local-transform
    // command must still resolve by persistent ID when it becomes the next undo.
    document.Destroy(child);
    assert(document.Undo());
    const Entity restored = document.FindByPersistentID(childPersistentID);
    assert(restored);
    assert(restored != child);
    assert(Near(document.LocalTransform(restored).position.x, 5.0f));

    assert(document.Undo());
    assert(Near(document.LocalTransform(restored).position.x, 2.0f));
    assert(Near(document.Scene().ReadTransform(restored).position.x, 12.0f));

    assert(document.Redo());
    assert(Near(document.LocalTransform(restored).position.x, 5.0f));
    assert(Near(document.Scene().ReadTransform(restored).position.x, 15.0f));

    return 0;
}
