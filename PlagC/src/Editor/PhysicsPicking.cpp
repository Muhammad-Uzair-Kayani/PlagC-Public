#include "pgcpch.h"
#include <PlagC/Editor/PhysicsPicking.h>
#include <PlagC/Thread.h>

#include <algorithm>

namespace PlagC::Editor {

PhysicsPickingProvider::PhysicsPickingProvider(PhysicsSystem& physics,
                                               const SceneDocument& document,
                                               CollisionLayer queryLayer)
    : physics_(physics), document_(document), queryLayer_(queryLayer) {
    Thread::RequireMain();
}

std::optional<PickResult> PhysicsPickingProvider::Pick(const PickRequest& request) {
    Thread::RequireMain();
    const PickRay ray = BuildPickRay(request);
    const auto hit = physics_.Raycast(ray.origin, ray.displacement, queryLayer_);
    if (!hit)
        return std::nullopt;

    const Entity entity = physics_.EntityFor(hit->body);
    if (!entity || !document_.Scene().Alive(entity))
        return std::nullopt;

    const float fraction = std::clamp(hit->fraction, 0.0f, 1.0f);
    const float distance = request.camera.nearClip +
        (request.camera.farClip - request.camera.nearClip) * fraction;
    return PickResult{document_.PersistentID(entity), distance};
}

} // namespace PlagC::Editor
