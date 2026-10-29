#pragma once

#include <PlagC/Editor/EditorModels.h>
#include <PlagC/Editor/SceneDocument.h>
#include <PlagC/World/SceneRenderer.h>
#include <optional>

namespace PlagC::Editor {

// Converts a renderer-level object hit into the persistent scene identity consumed
// by editor selection. Objects not owned by SceneRenderer (HUD/debug/UI objects),
// stale scene bindings, and an empty object handle resolve to no selection.
std::optional<PickResult> ResolveSceneObjectPick(const SceneDocument&,
                                                 const SceneRenderer&,
                                                 ObjectHandle,
                                                 float distance);

} // namespace PlagC::Editor
