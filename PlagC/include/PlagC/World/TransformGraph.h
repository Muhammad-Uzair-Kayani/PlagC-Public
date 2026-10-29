#pragma once

#include <PlagC/Renderer/Renderer.h>

namespace PlagC {

// Canonical PlagC hierarchical TRS math. Rotation uses the same Yaw(Y) *
// Pitch(X) * Roll(Z) convention as the renderer and Jolt adapter.
//
// ComposeTransform(parent, local) resolves a child-local transform into world
// space. RelativeTransform(parent, world) performs the inverse operation.
// Positive scale is required; arbitrary shear is intentionally unsupported.
Transform ComposeTransform(const Transform& parentWorld, const Transform& local);
Transform RelativeTransform(const Transform& parentWorld, const Transform& world);

} // namespace PlagC
