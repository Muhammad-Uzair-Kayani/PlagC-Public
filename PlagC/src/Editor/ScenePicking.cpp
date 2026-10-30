#include "pgcpch.h"
#include <PlagC/Editor/ScenePicking.h>
#include <PlagC/Thread.h>
#include <cmath>
#include <stdexcept>

namespace PlagC::Editor {

std::optional<PickResult> ResolveSceneObjectPick(const SceneDocument& document,
                                                 const SceneRenderer& sceneRenderer,
                                                 const AssetScene& assetScene,
                                                 ObjectHandle object,
                                                 float distance)
{
    Thread::RequireMain();
    if (!std::isfinite(distance) || distance < 0)
        throw std::invalid_argument("invalid scene pick distance");
    if (!object)
        return std::nullopt;

    Entity entity = sceneRenderer.EntityForObject(object);
    if (!entity)
        entity = assetScene.EntityForObject(object);
    if (!entity || !document.Scene().Alive(entity))
        return std::nullopt;

    return PickResult{document.PersistentID(entity), distance};
}

RendererPickingProvider::RendererPickingProvider(Renderer& renderer,
                                                 const SceneDocument& document,
                                                 const SceneRenderer& sceneRenderer,
                                                 const AssetScene& assetScene)
    : renderer_(renderer), document_(document), sceneRenderer_(sceneRenderer), assetScene_(assetScene)
{
}

std::optional<PickResult> RendererPickingProvider::Pick(const PickRequest& request)
{
    Thread::RequireMain();
    const PickRay ray = BuildPickRay(request);
    for (const ObjectRayHit& hit : renderer_.RaycastObjects(ray.origin, ray.displacement))
        if (auto resolved = ResolveSceneObjectPick(document_, sceneRenderer_, assetScene_,
                                                   hit.object, hit.distance))
            return resolved;
    return std::nullopt;
}

} // namespace PlagC::Editor
