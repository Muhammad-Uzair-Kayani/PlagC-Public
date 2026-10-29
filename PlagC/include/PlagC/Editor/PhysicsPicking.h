#pragma once

#include <PlagC/Editor/PickingMath.h>
#include <PlagC/Editor/SceneDocument.h>
#include <PlagC/Physics/PhysicsSystem.h>

namespace PlagC::Editor {

// Collider-backed fallback picker. Renderer object/ID picking should remain
// the primary path so visible entities without physics are still selectable.
class PhysicsPickingProvider final : public PickingProvider {
public:
    PhysicsPickingProvider(PhysicsSystem& physics,
                           const SceneDocument& document,
                           CollisionLayer queryLayer = CollisionLayer::Projectile);

    std::optional<PickResult> Pick(const PickRequest& request) override;

private:
    PhysicsSystem& physics_;
    const SceneDocument& document_;
    CollisionLayer queryLayer_ = CollisionLayer::Projectile;
};

} // namespace PlagC::Editor
