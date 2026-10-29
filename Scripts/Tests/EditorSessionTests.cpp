#include <PlagC/Editor/EditorSession.h>
#include <PlagC/Thread.h>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
using namespace PlagC;
using namespace PlagC::Editor;

int main() {
    Thread::InitializeMain();

    auto world = std::make_unique<World>();
    auto entity = world->Create("Authoring");
    auto document = std::make_unique<SceneDocument>(std::move(world));
    const auto id = document->PersistentID(entity);
    EditorSession session(std::move(document));

    assert(!session.HasPath() && session.EditingEnabled());
    session.EditDocument().Rename(entity, "Dirty");
    assert(session.Dirty());

    bool dirtyRejected = false;
    try { session.NewEmpty(); }
    catch (const std::logic_error&) { dirtyRejected = true; }
    assert(dirtyRejected);
    assert(session.Document().FindByPersistentID(id));

    const auto root = std::filesystem::temp_directory_path() /
        ("plagc-editor-session-" +
         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    const auto firstPath = root / "first.plscene";
    const auto secondPath = root / "second.plscene";
    const auto brokenPath = root / "broken.plscene";

    session.SaveAs(firstPath);
    assert(session.HasPath() && session.Path() == firstPath && !session.Dirty());

    session.EditDocument().Rename(session.Document().FindByPersistentID(id), "PlaySource");
    assert(session.Dirty());
    session.StartPlay();
    assert(session.Playing() && &session.ViewWorld() == &session.RuntimeWorld());
    assert(session.ViewDocument().FindByPersistentID(id));

    bool editRejected = false;
    try { (void)session.EditDocument(); }
    catch (const std::logic_error&) { editRejected = true; }
    assert(editRejected && !session.CanUndo());

    const auto runtimeEntity = session.RuntimeDocument().FindByPersistentID(id);
    assert(runtimeEntity);
    session.RuntimeWorld().Rename(runtimeEntity, "RuntimeOnly");
    assert(session.Document().Scene().Read(session.Document().FindByPersistentID(id)).name == "PlaySource");
    assert(session.RuntimeWorld().Read(runtimeEntity).name == "RuntimeOnly");

    session.PausePlay();
    session.RequestStep();
    assert(session.ConsumeStep());
    session.StopPlay();
    assert(!session.Playing() && session.EditingEnabled());

    session.Save();
    assert(!session.Dirty());

    // A failed Open must preserve both the live document and its current path.
    std::ofstream(brokenPath) << "{not-json";
    bool openFailed = false;
    try { session.Open(brokenPath, {}, ReplacePolicy::DiscardChanges); }
    catch (const std::exception&) { openFailed = true; }
    assert(openFailed && session.Path() == firstPath);
    assert(session.Document().FindByPersistentID(id));

    auto otherWorld = std::make_unique<World>();
    otherWorld->Create("Second");
    auto other = std::make_unique<SceneDocument>(std::move(otherWorld));
    other->Save(secondPath);

    session.Open(secondPath, {}, ReplacePolicy::DiscardChanges);
    assert(session.Path() == secondPath && !session.Dirty());
    const auto snapshot = session.Document().Snapshot();
    assert(snapshot.size() == 1 && snapshot.front().record.name == "Second");

    session.NewEmpty(ReplacePolicy::DiscardChanges);
    assert(!session.HasPath() && session.Document().Scene().Size() == 0);

    std::filesystem::remove_all(root);
}
