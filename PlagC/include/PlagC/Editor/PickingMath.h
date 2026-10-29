#pragma once

#include <PlagC/Editor/EditorModels.h>

namespace PlagC::Editor {

struct PickRay {
    Vec3 origin{};
    Vec3 displacement{};
};

// Builds a world-space ray from viewport-local pixel coordinates and the
// editor camera. The ray begins at the near plane and spans to the far plane.
PickRay BuildPickRay(const PickRequest& request);

} // namespace PlagC::Editor
