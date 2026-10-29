#include <PlagC/World/World.h>
#include <PlagC/Thread.h>
#include <cassert>
#include <atomic>
#include <exception>
#include <thread>
using namespace PlagC;

template <class Fn> static bool Rejected(Fn&& fn) {
    try { fn(); } catch (const std::exception&) { return true; }
    return false;
}

int main() {
    Thread::InitializeMain();
    World world, other;
    const auto root = world.Create("Root");
    const auto child = world.Create("Child");
    const auto grandchild = world.Create("Grandchild");
    const auto foreign = other.Create("Foreign");

    const auto childTransform = world.ReadTransform(child);
    world.SetParent(child, root);
    world.SetParent(grandchild, child);
    assert(world.Parent(child) == root);
    assert(world.Parent(grandchild) == child);
    assert(world.ReadTransform(child).position.x == childTransform.position.x);

    const auto children = world.Children(root);
    assert(children.size() == 1 && children[0] == child);

    assert(Rejected([&] { world.SetParent(root, grandchild); }));
    assert(Rejected([&] { world.SetParent(root, root); }));
    assert(Rejected([&] { world.SetParent(child, foreign); }));
    assert(!world.Parent(root));
    assert(world.Parent(child) == root);
    assert(world.Parent(grandchild) == child);

    world.ClearParent(grandchild);
    assert(!world.Parent(grandchild));
    world.ClearParent(grandchild); // idempotent

    world.SetParent(grandchild, root);
    auto rootChildren = world.Children(root);
    assert(rootChildren.size() == 2);
    assert(rootChildren[0].value < rootChildren[1].value);

    world.Destroy(root);
    assert(!world.Parent(child));
    assert(!world.Parent(grandchild));

    std::atomic<unsigned> denied{0};
    std::thread worker([&] {
        denied += Rejected([&] { (void)world.Parent(child); });
        denied += Rejected([&] { world.SetParent(child, grandchild); });
    });
    worker.join();
    assert(denied == 2);

    world.Clear();
    assert(world.Size() == 0);
    return 0;
}
