#include "pgcpch.h"
#include <PlagC/Editor/ScenePicking.h>
#include <PlagC/Thread.h>
#include <cmath>
#include <stdexcept>

namespace PlagC::Editor {

std::optional<PickResult> ResolveSceneObjectPick(const SceneDocument& document,
                                                 const SceneRenderer& sceneRenderer,
                                                 ObjectHandle object,
                                                 float distance)
{
    Thread::RequireMain();
    if (!std::isfinite(distance) || distance < 0)
        throw std::invalid_argument("invalid scene pick distance");
    if (!object)
        return std::nullopt;

    const Entity entity = sceneRenderer.EntityForObject(object);
    if (!entity || !document.Scene().Alive(entity))
        return std::nullopt;

    return PickResult{document.PersistentID(entity), distance};
}

} // namespace PlagC::Editor
