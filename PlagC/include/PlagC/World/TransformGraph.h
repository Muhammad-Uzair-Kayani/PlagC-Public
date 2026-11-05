#pragma once

#include <PlagC/Renderer/Renderer.h>

namespace PlagC {

// Canonical PlagC hierarchical TRS math. Rotation uses the same Yaw(Y) *
// Pitch(X) * Roll(Z) convention as the renderer and Jolt adapter.
//
// ComposeTransform(parent, local) resolves a child-local transform into world
// space. RelativeTransform(parent, world) performs the inverse operation.
// Positive scale is required; arbitrary shear is intentionally unsupported.
// Rotate a direction by canonical Y * X * Z Euler angles (ignores scale/translation).
Vec3 RotateDirection(Vec3 rotationDegrees, Vec3 direction);
// Zero roll pose looking along a direction; local camera/light forward is -Z.
Vec3 DirectionRotation(Vec3 direction);
Vec3 DirectionRotation(Vec3 direction, Vec3 up);
Transform ComposeTransform(const Transform& parentWorld, const Transform& local);
Transform RelativeTransform(const Transform& parentWorld, const Transform& world);

} // namespace PlagC
