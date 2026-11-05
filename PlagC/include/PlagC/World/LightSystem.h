#pragma once
#include <PlagC/World/World.h>
#include <PlagC/World/TransformGraph.h>

namespace PlagC {
// Shared extraction for rendering and editor visualization. Component descriptors
// supply photometric properties; resolved Transform owns position and direction.
inline LightDesc ExtractLight(const LightComponent& component, const Transform& transform) {
    auto result = component.light;
    result.position = transform.position;
    result.direction = RotateDirection(transform.rotationDegrees, {0,0,-1});
    return result;
}
} // namespace PlagC
