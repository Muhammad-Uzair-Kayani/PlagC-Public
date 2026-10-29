#include <PlagC/World/TransformGraph.h>
#include <cassert>
#include <cmath>
#include <stdexcept>

using namespace PlagC;

namespace {

bool Near(float a, float b, float epsilon = 2.0e-3f) {
    return std::fabs(a - b) <= epsilon;
}

void AssertVec(Vec3 actual, Vec3 expected, float epsilon = 2.0e-3f) {
    assert(Near(actual.x, expected.x, epsilon));
    assert(Near(actual.y, expected.y, epsilon));
    assert(Near(actual.z, expected.z, epsilon));
}

void AssertTransform(const Transform& actual, const Transform& expected) {
    AssertVec(actual.position, expected.position);
    AssertVec(actual.scale, expected.scale);
    // Euler representations are not globally unique. Round-trip through the
    // canonical hierarchy functions instead of comparing arbitrary angles.
    const auto recomposed = ComposeTransform(actual, Transform{});
    AssertVec(recomposed.position, actual.position);
    AssertVec(recomposed.scale, actual.scale);
}

} // namespace

int main() {
    const Transform identity{};
    const Transform child{{1.0f, 2.0f, 3.0f}, {10.0f, 20.0f, 30.0f}, {2.0f, 3.0f, 4.0f}};
    const auto identityWorld = ComposeTransform(identity, child);
    AssertVec(identityWorld.position, child.position);
    AssertVec(identityWorld.scale, child.scale);
    const auto identityLocal = RelativeTransform(identity, child);
    AssertVec(identityLocal.position, child.position);
    AssertVec(identityLocal.scale, child.scale);

    Transform parent;
    parent.position = {10.0f, -2.0f, 5.0f};
    parent.rotationDegrees = {25.0f, 80.0f, -15.0f};
    parent.scale = {2.0f, 3.0f, 0.5f};

    Transform local;
    local.position = {3.0f, -1.0f, 2.0f};
    local.rotationDegrees = {-20.0f, 35.0f, 12.0f};
    local.scale = {0.5f, 1.5f, 4.0f};

    const Transform world = ComposeTransform(parent, local);
    const Transform recovered = RelativeTransform(parent, world);
    AssertVec(recovered.position, local.position);
    AssertVec(recovered.scale, local.scale);
    const Transform recomposed = ComposeTransform(parent, recovered);
    AssertVec(recomposed.position, world.position);
    AssertVec(recomposed.scale, world.scale);

    // Positive Y yaw follows the same right-handed convention used by Ogre/Jolt.
    Transform yawParent;
    yawParent.rotationDegrees.y = 90.0f;
    Transform offset;
    offset.position = {1.0f, 0.0f, 0.0f};
    const Transform yawWorld = ComposeTransform(yawParent, offset);
    AssertVec(yawWorld.position, {0.0f, 0.0f, -1.0f}, 3.0e-3f);

    // Preserve-world reparent math: derive local from the new parent, then
    // composing it produces the exact same world-space placement.
    Transform desiredWorld;
    desiredWorld.position = {-4.0f, 6.0f, 9.0f};
    desiredWorld.rotationDegrees = {17.0f, -42.0f, 8.0f};
    desiredWorld.scale = {1.25f, 0.75f, 2.0f};
    const Transform reparentLocal = RelativeTransform(parent, desiredWorld);
    const Transform preserved = ComposeTransform(parent, reparentLocal);
    AssertVec(preserved.position, desiredWorld.position);
    AssertVec(preserved.scale, desiredWorld.scale);

    bool rejected = false;
    try {
        Transform invalid;
        invalid.scale.x = 0.0f;
        (void)ComposeTransform(parent, invalid);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);

    AssertTransform(world, world);
}
