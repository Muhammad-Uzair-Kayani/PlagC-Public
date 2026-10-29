#include <PlagC/Editor/SceneDocument.h>
#include <PlagC/Thread.h>
#include <cassert>
#include <thread>
using namespace PlagC;

static bool Same(Vec3 a, Vec3 b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

int main() {
    Thread::InitializeMain();

    auto world = std::make_unique<World>();
    auto original = world->Create("Cube");
    Editor::SceneDocument document(std::move(world));

    const auto originalID = document.PersistentID(original);
    document.Select(original);
    assert(document.Selection() == original && !document.Dirty());

    document.Rename(original, "Cube");
    assert(!document.Undo());

    document.Rename(original, "Prop");
    assert(document.Scene().Read(original).name == "Prop");

    // Commands resolve through stable IDs, so delete/restore can replace the
    // runtime Entity handle without invalidating earlier history.
    document.Destroy(original);
    assert(!document.FindByPersistentID(originalID));
    assert(document.Undo());
    auto restored = document.FindByPersistentID(originalID);
    assert(restored && restored != original);
    assert(document.Scene().Read(restored).name == "Prop");
    assert(document.Undo());
    restored = document.FindByPersistentID(originalID);
    assert(document.Scene().Read(restored).name == "Cube");
    assert(document.Redo());
    assert(document.Scene().Read(document.FindByPersistentID(originalID)).name == "Prop");

    // Create is one undoable authoring command and keeps its persistent ID
    // across undo/redo even though the runtime handle changes.
    Editor::EntityTemplate cameraTemplate;
    cameraTemplate.name = "Camera";
    cameraTemplate.transform.position = {0, 1, 5};
    CameraComponent camera;
    camera.target = {0, 1, 0};
    cameraTemplate.camera = camera;

    auto cameraEntity = document.Create(cameraTemplate);
    const auto cameraID = document.PersistentID(cameraEntity);
    assert(document.Selection() == cameraEntity);
    document.SetActiveCamera(cameraEntity);
    assert(document.Scene().ActiveCamera() == cameraEntity);

    // Duplicate receives a new stable ID.
    auto duplicate = document.Duplicate(cameraEntity);
    const auto duplicateID = document.PersistentID(duplicate);
    assert(duplicateID != cameraID);
    assert(document.Scene().Read(duplicate).camera.has_value());
    assert(document.Undo());
    assert(!document.FindByPersistentID(duplicateID));
    assert(document.Redo());
    duplicate = document.FindByPersistentID(duplicateID);
    assert(duplicate);

    // Persistent IDs are a high-water mark, not recyclable UI row IDs.
    Editor::EntityTemplate tempTemplate;
    tempTemplate.name = "Temporary";
    auto temporary = document.Create(tempTemplate);
    const auto temporaryID = document.PersistentID(temporary);
    document.Destroy(temporary);
    auto later = document.Create(tempTemplate);
    const auto laterID = document.PersistentID(later);
    assert(laterID > temporaryID);

    // One continuous gizmo/inspector drag produces one history entry.
    document.MarkSaved();
    auto before = document.Scene().ReadTransform(duplicate);
    {
        auto edit = document.BeginTransformEdit(duplicate);
        auto preview = before;
        preview.position.x = 2;
        edit.Preview(preview);
        preview.position.x = 4;
        edit.Preview(preview);
        assert(document.HasTransientEdit() && document.Dirty());
        edit.Commit();
    }
    assert(!document.HasTransientEdit() && document.Dirty());
    assert(document.Undo());
    assert(!document.Dirty());
    assert(Same(document.Scene().ReadTransform(
                    document.FindByPersistentID(duplicateID)).position,
                before.position));
    assert(document.Redo());

    // Component edits are undoable and active camera restoration is safe.
    auto duplicateHandle = document.FindByPersistentID(duplicateID);
    document.RemoveCamera(duplicateHandle);
    assert(!document.Scene().Read(duplicateHandle).camera);
    assert(document.Undo());
    duplicateHandle = document.FindByPersistentID(duplicateID);
    assert(document.Scene().Read(duplicateHandle).camera);

    // Runtime clone keeps current components/IDs but mutations cannot leak back
    // into the authoring document.
    auto runtime = document.CloneForRuntime();
    auto runtimeEntity = runtime->FindByPersistentID(originalID);
    assert(runtimeEntity);
    runtime->Rename(runtimeEntity, "RuntimeOnly");
    assert(runtime->Scene().Read(runtimeEntity).name == "RuntimeOnly");
    assert(document.Scene().Read(document.FindByPersistentID(originalID)).name == "Prop");

    const auto snapshot = document.Snapshot();
    assert(snapshot.size() == document.Scene().Size());
    for (std::size_t i = 1; i < snapshot.size(); ++i)
        assert(snapshot[i - 1].persistentID < snapshot[i].persistentID);

    // Foreign/stale selection remains rejected.
    World other;
    auto foreign = other.Create();
    try { document.Select(foreign); assert(false); }
    catch (const std::invalid_argument&) {}

    bool rejected = false;
    std::thread worker([&] {
        try { document.Snapshot(); }
        catch (const std::logic_error&) { rejected = true; }
    });
    worker.join();
    assert(rejected);

    document.SelectPersistent(originalID);
    assert(document.PersistentID(document.Selection()) == originalID);
}
