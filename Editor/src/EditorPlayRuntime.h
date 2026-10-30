#pragma once

#include <PlagC/Application.h>
#include <PlagC/Physics/PhysicsSystem.h>
#include <cstdint>
#include <memory>
#include <span>

namespace PlagC::Editor {

class EditorSession;

// Owns runtime-only simulation systems for the isolated PlaySession clone.
// Authoring state is never mutated; renderer interpolation is returned separately.
class EditorPlayRuntime final {
public:
    explicit EditorPlayRuntime(const EngineConfig&);

    std::span<const TransformUpdate> Update(Engine&, EditorSession&, double frameSeconds);
    void Reset() noexcept;

private:
    PhysicsConfig physicsConfig_{};
    bool useSharedJobs_ = false;
    std::unique_ptr<PhysicsSystem> physics_;
    std::uint64_t generation_ = 0;
};

} // namespace PlagC::Editor
