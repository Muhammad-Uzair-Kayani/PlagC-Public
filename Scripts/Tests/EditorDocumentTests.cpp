#include <PlagC/Editor/SceneDocument.h>
#include <PlagC/Thread.h>
#include <cassert>
#include <thread>
using namespace PlagC;
int main() {
    Thread::InitializeMain();
    auto world = std::make_unique<World>(); auto e = world->Create("Cube");
    Editor::SceneDocument d(std::move(world));
    d.Select(e); assert(d.Selection() == e && !d.Dirty());
    d.Rename(e, "Cube"); assert(!d.Undo());
    d.Rename(e, "Prop"); assert(d.Scene().Read(e).name == "Prop" && d.Dirty());
    d.MarkSaved(); auto t = d.Scene().ReadTransform(e); t.position.x = 5;
    d.SetTransform(e,t); assert(d.Undo() && !d.Dirty());
    assert(d.Scene().ReadTransform(e).position.x == 0);
    auto invalid = t; invalid.scale.x = -1;
    try { d.SetTransform(e,invalid); assert(false); } catch (const std::invalid_argument&) {}
    assert(!d.Dirty() && d.Redo() && d.Scene().ReadTransform(e).position.x == 5);
    World other; auto foreign = other.Create();
    try { d.Select(foreign); assert(false); } catch (const std::invalid_argument&) {}
    assert(d.Selection() == e);
    bool rejected = false;
    std::thread worker([&] { try { d.Select({}); } catch (const std::logic_error&) { rejected = true; } });
    worker.join(); assert(rejected);
    d.Select({}); assert(!d.Selection());
}
