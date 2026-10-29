#include <PlagC/World/World.h>
#include <PlagC/Thread.h>
#include <cassert>
#include <atomic>
#include <cmath>
#include <exception>
#include <thread>
using namespace PlagC;

template <class Fn> static bool Rejected(Fn&& fn) {
    try { fn(); } catch (const std::exception&) { return true; }
    return false;
}

static bool Near(float a, float b, float epsilon = 1.0e-3f) {
    return std::fabs(a - b) <= epsilon;
}

static void PositionIs(const Transform& t, float x, float y, float z) {
    assert(Near(t.position.x, x));
    assert(Near(t.position.y, y));
    assert(Near(t.position.z, z));
}

int main() {
    Thread::InitializeMain();
    World world, other;
    const auto root = world.Create("Root");
    const auto child = world.Create("Child");
    const auto grandchild = world.Create("Grandchild");
    const auto foreign = other.Create("Foreign");

    Transform rootWorld;
    rootWorld.position = {10, 0, 0};
    world.SetTransform(root, rootWorld);
    Transform childWorld;
    childWorld.position = {12, 0, 0};
    world.SetTransform(child, childWorld);
    Transform grandWorld;
    grandWorld.position = {13, 0, 0};
    world.SetTransform(grandchild, grandWorld);

    // Reparent preserves world placement and derives local placement.
    world.SetParent(child, root);
    world.SetParent(grandchild, child);
    assert(world.Parent(child) == root);
    assert(world.Parent(grandchild) == child);
    PositionIs(world.ReadTransform(child), 12, 0, 0);
    PositionIs(world.ReadLocalTransform(child), 2, 0, 0);
    PositionIs(world.ReadLocalTransform(grandchild), 1, 0, 0);

    const auto children = world.Children(root);
    assert(children.size() == 1 && children[0] == child);

    // Parent world edits propagate through stored locals.
    rootWorld.position = {20, 0, 0};
    world.SetTransform(root, rootWorld);
    PositionIs(world.ReadTransform(root), 20, 0, 0);
    PositionIs(world.ReadTransform(child), 22, 0, 0);
    PositionIs(world.ReadTransform(grandchild), 23, 0, 0);

    // Local edits propagate descendants without changing the parent.
    Transform childLocal = world.ReadLocalTransform(child);
    childLocal.position = {5, 0, 0};
    world.SetLocalTransform(child, childLocal);
    PositionIs(world.ReadTransform(child), 25, 0, 0);
    PositionIs(world.ReadTransform(grandchild), 26, 0, 0);

    // A world-space batch may explicitly move both parent and child. The explicit
    // child target wins and its new local is derived against the parent's final world.
    Transform nextRoot = world.ReadTransform(root);
    nextRoot.position = {30, 0, 0};
    Transform nextChild = world.ReadTransform(child);
    nextChild.position = {40, 0, 0};
    const TransformUpdate batch[]{{root, nextRoot}, {child, nextChild}};
    world.ApplyTransforms(batch);
    PositionIs(world.ReadTransform(root), 30, 0, 0);
    PositionIs(world.ReadTransform(child), 40, 0, 0);
    PositionIs(world.ReadLocalTransform(child), 10, 0, 0);
    PositionIs(world.ReadTransform(grandchild), 41, 0, 0);

    assert(Rejected([&] { world.SetParent(root, grandchild); }));
    assert(Rejected([&] { world.SetParent(root, root); }));
    assert(Rejected([&] { world.SetParent(child, foreign); }));
    assert(!world.Parent(root));
    assert(world.Parent(child) == root);
    assert(world.Parent(grandchild) == child);

    // Clearing a parent preserves resolved world placement and makes local == world.
    const Transform grandBeforeRoot = world.ReadTransform(grandchild);
    world.ClearParent(grandchild);
    assert(!world.Parent(grandchild));
    PositionIs(world.ReadTransform(grandchild), grandBeforeRoot.position.x,
               grandBeforeRoot.position.y, grandBeforeRoot.position.z);
    PositionIs(world.ReadLocalTransform(grandchild), grandBeforeRoot.position.x,
               grandBeforeRoot.position.y, grandBeforeRoot.position.z);
    world.ClearParent(grandchild); // idempotent

    world.SetParent(grandchild, root);
    auto rootChildren = world.Children(root);
    assert(rootChildren.size() == 2);
    assert(rootChildren[0].value < rootChildren[1].value);

    // Physics attachment and transform hierarchy cannot silently compete for ownership.
    PhysicsComponent physics;
    assert(Rejected([&] { world.SetPhysics(child, physics); }));
    const auto physicsRoot = world.Create("PhysicsRoot");
    world.SetPhysics(physicsRoot, physics);
    assert(Rejected([&] { world.SetParent(physicsRoot, child); }));
    assert(Rejected([&] { world.SetParent(child, physicsRoot); }));

    // Destroying a parent promotes direct children to roots without moving them.
    const Transform childBeforeDelete = world.ReadTransform(child);
    const Transform grandBeforeDelete = world.ReadTransform(grandchild);
    world.Destroy(root);
    assert(!world.Parent(child));
    assert(!world.Parent(grandchild));
    PositionIs(world.ReadTransform(child), childBeforeDelete.position.x,
               childBeforeDelete.position.y, childBeforeDelete.position.z);
    PositionIs(world.ReadLocalTransform(child), childBeforeDelete.position.x,
               childBeforeDelete.position.y, childBeforeDelete.position.z);
    PositionIs(world.ReadTransform(grandchild), grandBeforeDelete.position.x,
               grandBeforeDelete.position.y, grandBeforeDelete.position.z);

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
