#include <PlagC/Editor/SceneDocument.h>
#include <PlagC/Thread.h>
#include <cassert>
#include <exception>
#include <string>
using namespace PlagC;

template <class Fn> static bool Rejected(Fn&& fn) {
    try { fn(); } catch (const std::exception&) { return true; }
    return false;
}

static std::uint64_t ParentID(const Editor::SceneDocument& document, std::uint64_t id) {
    const auto entity = document.FindByPersistentID(id);
    assert(entity);
    return document.ParentPersistentID(entity);
}

int main() {
    Thread::InitializeMain();
    Editor::SceneDocument document(std::make_unique<World>());

    Editor::EntityTemplate rootTemplate; rootTemplate.name = "Root";
    Editor::EntityTemplate childTemplate; childTemplate.name = "Child";
    Editor::EntityTemplate grandTemplate; grandTemplate.name = "Grandchild";
    auto root = document.Create(rootTemplate);
    auto child = document.Create(childTemplate);
    auto grandchild = document.Create(grandTemplate);
    const auto rootID = document.PersistentID(root);
    const auto childID = document.PersistentID(child);
    const auto grandID = document.PersistentID(grandchild);

    document.SetParent(child, root);
    document.SetParent(grandchild, child);
    assert(ParentID(document, childID) == rootID);
    assert(ParentID(document, grandID) == childID);

    auto snapshot = document.Snapshot();
    bool sawChild = false, sawGrand = false;
    for (const auto& item : snapshot) {
        if (item.persistentID == childID) {
            sawChild = true;
            assert(item.parentPersistentID == rootID);
        }
        if (item.persistentID == grandID) {
            sawGrand = true;
            assert(item.parentPersistentID == childID);
        }
    }
    assert(sawChild && sawGrand);

    document.MarkSaved();
    assert(Rejected([&] { document.SetParent(root, grandchild); }));
    assert(!document.Dirty());
    assert(!ParentID(document, rootID));

    document.SetParent(grandchild, root);
    assert(ParentID(document, grandID) == rootID);
    assert(document.Undo());
    assert(ParentID(document, grandID) == childID);
    assert(!document.Dirty());
    assert(document.Redo());
    assert(ParentID(document, grandID) == rootID);
    assert(document.Undo());

    auto duplicate = document.Duplicate(child);
    const auto duplicateID = document.PersistentID(duplicate);
    assert(duplicateID != childID);
    assert(ParentID(document, duplicateID) == rootID);

    document.Destroy(root);
    assert(!document.FindByPersistentID(rootID));
    assert(!document.Scene().Parent(document.FindByPersistentID(childID)));
    assert(!document.Scene().Parent(document.FindByPersistentID(duplicateID)));

    assert(document.Undo());
    root = document.FindByPersistentID(rootID);
    child = document.FindByPersistentID(childID);
    duplicate = document.FindByPersistentID(duplicateID);
    assert(root && child && duplicate);
    assert(document.Scene().Parent(child) == root);
    assert(document.Scene().Parent(duplicate) == root);
    assert(ParentID(document, grandID) == childID);

    assert(document.Redo());
    assert(!document.FindByPersistentID(rootID));
    assert(document.Undo());
    root = document.FindByPersistentID(rootID);

    auto runtime = document.CloneForRuntime();
    const auto runtimeRoot = runtime->FindByPersistentID(rootID);
    const auto runtimeChild = runtime->FindByPersistentID(childID);
    const auto runtimeGrand = runtime->FindByPersistentID(grandID);
    assert(runtimeRoot && runtimeChild && runtimeGrand);
    assert(runtime->Scene().Parent(runtimeChild) == runtimeRoot);
    assert(runtime->ParentPersistentID(runtimeGrand) == childID);

    const std::string serialized = document.Serialize();
    assert(serialized.find("\"version\":2") != std::string::npos);
    auto loaded = Editor::SceneDocument::Deserialize(serialized);
    assert(ParentID(*loaded, childID) == rootID);
    assert(ParentID(*loaded, grandID) == childID);
    assert(ParentID(*loaded, duplicateID) == rootID);

    const std::string legacy =
        R"({"format":"PlagC.Scene","version":1,"activeCamera":null,"entities":[)"
        R"({"id":"10","name":"LegacyRoot","transform":{"position":[0,0,0],"rotation":[0,0,0],"scale":[1,1,1]}})"
        R"(,{"id":"11","name":"LegacyChild","transform":{"position":[1,0,0],"rotation":[0,0,0],"scale":[1,1,1]}}]})";
    auto legacyDocument = Editor::SceneDocument::Deserialize(legacy);
    assert(!ParentID(*legacyDocument, 10));
    assert(!ParentID(*legacyDocument, 11));

    const std::string missingParent =
        R"({"format":"PlagC.Scene","version":2,"activeCamera":null,"entities":[)"
        R"({"id":"1","parent":"99","name":"A","transform":{"position":[0,0,0],"rotation":[0,0,0],"scale":[1,1,1]}}]})";
    assert(Rejected([&] { (void)Editor::SceneDocument::Deserialize(missingParent); }));

    const std::string cyclic =
        R"({"format":"PlagC.Scene","version":2,"activeCamera":null,"entities":[)"
        R"({"id":"1","parent":"2","name":"A","transform":{"position":[0,0,0],"rotation":[0,0,0],"scale":[1,1,1]}},)"
        R"({"id":"2","parent":"1","name":"B","transform":{"position":[0,0,0],"rotation":[0,0,0],"scale":[1,1,1]}}]})";
    assert(Rejected([&] { (void)Editor::SceneDocument::Deserialize(cyclic); }));

    return 0;
}
