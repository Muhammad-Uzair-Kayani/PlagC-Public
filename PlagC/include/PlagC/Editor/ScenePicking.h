#pragma once

#include <PlagC/Assets/AssetScene.h>
#include <PlagC/Editor/EditorModels.h>
#include <PlagC/Editor/PickingMath.h>
#include <PlagC/Editor/SceneDocument.h>
#include <PlagC/World/SceneRenderer.h>
#include <optional>

namespace PlagC::Editor {

// Converts a renderer-level object hit into the persistent scene identity consumed
// by editor selection. Objects not owned by the scene bridges, stale bindings,
// and an empty object handle resolve to no selection.
std::optional<PickResult> ResolveSceneObjectPick(const SceneDocument&,
                                                 const SceneRenderer&,
                                                 const AssetScene&,
                                                 ObjectHandle,
                                                 float distance);

// Primary viewport picker. It uses the renderer's bounding-volume ray query so
// visible renderables remain selectable even when they have no physics collider.
class RendererPickingProvider final : public PickingProvider {
public:
    RendererPickingProvider(Renderer&, const SceneDocument&, const SceneRenderer&, const AssetScene&);
    std::optional<PickResult> Pick(const PickRequest&) override;

private:
    Renderer& renderer_;
    const SceneDocument& document_;
    const SceneRenderer& sceneRenderer_;
    const AssetScene& assetScene_;
};

} // namespace PlagC::Editor
