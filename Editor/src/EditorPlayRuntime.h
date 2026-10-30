#pragma once

#include <PlagC/Application.h>
#include <PlagC/Physics/PhysicsSystem.h>
#include <cstdint>
#include <memory>
#include <span>

namespace PlagC::Editor {

class EditorSession;

class EditorPlayRuntime final {
public:
    explicit EditorPlayRuntime(const EngineConfig&);

    std::span<const TransformUpdate> Update(Engine&, EditorSession&, double frameSeconds);
    double SimulationDelta() const noexcept { return simulationDelta_; }
    void Reset() noexcept;

private:
    PhysicsConfig physicsConfig_{};
    bool useSharedJobs_ = false;
    std::unique_ptr<PhysicsSystem> physics_;
    std::uint64_t generation_ = 0;
    double simulationDelta_ = 0.0;
};

} // namespace PlagC::Editor
