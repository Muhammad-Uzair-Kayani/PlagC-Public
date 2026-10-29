#include <PlagC/Editor/PickingMath.h>
#include <cassert>
#include <cmath>
#include <stdexcept>

using namespace PlagC;
using namespace PlagC::Editor;

namespace {

bool Near(float a, float b, float epsilon = 1.0e-4f) {
    return std::fabs(a - b) <= epsilon;
}

} // namespace

int main() {
    PickRequest center{};
    center.x = 50.0f;
    center.y = 50.0f;
    center.width = 100;
    center.height = 100;
    center.camera.position = {0.0f, 0.0f, 0.0f};
    center.camera.target = {0.0f, 0.0f, -1.0f};
    center.camera.fov = 90.0f;
    center.camera.nearClip = 1.0f;
    center.camera.farClip = 11.0f;
    center.camera.aspect = 0.0f;

    const PickRay centerRay = BuildPickRay(center);
    assert(Near(centerRay.origin.x, 0.0f));
    assert(Near(centerRay.origin.y, 0.0f));
    assert(Near(centerRay.origin.z, -1.0f));
    assert(Near(centerRay.displacement.x, 0.0f));
    assert(Near(centerRay.displacement.y, 0.0f));
    assert(Near(centerRay.displacement.z, -10.0f));

    PickRequest right = center;
    right.x = 75.0f;
    const PickRay rightRay = BuildPickRay(right);
    assert(rightRay.displacement.x > 0.0f);
    assert(rightRay.displacement.z < 0.0f);

    PickRequest top = center;
    top.y = 25.0f;
    const PickRay topRay = BuildPickRay(top);
    assert(topRay.displacement.y > 0.0f);

    PickRequest vertical = center;
    vertical.camera.target = {0.0f, 1.0f, 0.0f};
    const PickRay verticalRay = BuildPickRay(vertical);
    assert(Near(verticalRay.origin.y, 1.0f));
    assert(Near(verticalRay.displacement.y, 10.0f));

    bool rejected = false;
    try {
        PickRequest invalid = center;
        invalid.width = 0;
        (void)BuildPickRay(invalid);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);

    rejected = false;
    try {
        PickRequest outside = center;
        outside.x = 101.0f;
        (void)BuildPickRay(outside);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
}
