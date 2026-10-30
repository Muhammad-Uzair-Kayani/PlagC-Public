#pragma once

#include <PlagC/Application.h>
#include <PlagC/Gameplay/GameplayRuntime.h>
#include <PlagC/Physics/PhysicsSystem.h>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>

namespace PlagC::Editor {

class EditorSession;

class EditorPlayRuntime final {
public:
    explicit EditorPlayRuntime(const EngineConfig&);

    void Initialize(Engine&);
    void Shutdown(Engine&) noexcept;
    void Reset(Engine&) noexcept;
    void AddGameplaySystem(std::shared_ptr<GameplaySystem> system) { gameplay_.AddSystem(std::move(system)); }
    void SetGameInputRouting(bool keyboardEnabled, bool mouseEnabled) {
        gameplay_.SetInputRouting(keyboardEnabled, mouseEnabled);
    }

    std::span<const TransformUpdate> Update(Engine&, EditorSession&, double frameSeconds);
    // Called after AssetScene animation/skinning so late gameplay and audio maintenance have explicit ordering.
    void CompleteFrame(Engine&, EditorSession&);
    double SimulationDelta() const noexcept { return simulationDelta_; }
    GameplayRuntime& Gameplay() noexcept { return gameplay_; }

private:
    void ClearPlayState() noexcept;

    PhysicsConfig physicsConfig_{};
    bool useSharedJobs_ = false;
    std::unique_ptr<PhysicsSystem> physics_;
    GameplayRuntime gameplay_;
    std::uint64_t generation_ = 0;
    double simulationDelta_ = 0.0;
    bool gameplayPaused_ = false;
    bool completeFramePending_ = false;
};

} // namespace PlagC::Editor
