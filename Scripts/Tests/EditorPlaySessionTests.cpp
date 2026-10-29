#include <PlagC/Editor/PlaySession.h>
#include <PlagC/Thread.h>
#include <cassert>
using namespace PlagC;

int main() {
    Thread::InitializeMain();

    auto world = std::make_unique<World>();
    auto entity = world->Create("Authoring");
    Editor::SceneDocument document(std::move(world));
    const auto id = document.PersistentID(entity);

    Editor::PlaySession play;
    assert(!play.Running());
    const auto beforeGeneration = play.Generation();

    play.Start(document);
    assert(play.Running() && !play.Paused());
    assert(play.Generation() == beforeGeneration + 1);

    auto runtimeEntity = play.RuntimeDocument().FindByPersistentID(id);
    assert(runtimeEntity);
    play.RuntimeWorld().Rename(runtimeEntity, "RuntimeOnly");
    assert(document.Scene().Read(document.FindByPersistentID(id)).name == "Authoring");

    play.Pause();
    assert(play.Paused());
    play.RequestStep();
    assert(play.ConsumeStep());
    assert(!play.ConsumeStep());

    play.Resume();
    assert(!play.Paused());
    play.TogglePause();
    assert(play.Paused());

    const auto runningGeneration = play.Generation();
    play.Stop();
    assert(!play.Running() && play.Generation() == runningGeneration + 1);

    bool rejected = false;
    try { (void)play.RuntimeWorld(); }
    catch (const std::logic_error&) { rejected = true; }
    assert(rejected);
}
