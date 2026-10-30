#include "pgcpch.h"
#include "EditorPlayRuntime.h"
#include <PlagC/Editor/EditorSession.h>
#include <chrono>
#include <stdexcept>

namespace PlagC::Editor {

EditorPlayRuntime::EditorPlayRuntime(const EngineConfig& config)
{
    if (!(config.physicsHz > 0.0))
        throw std::invalid_argument("Editor play physics frequency must be positive");

    physicsConfig_.fixedHz = config.physicsHz;
    physicsConfig_.workers = config.physicsWorkers;
    if (config.physicsScheduler == "native")
        physicsConfig_.scheduler = PhysicsScheduler::Native;
    else if (config.physicsScheduler == "plagc")
    {
        physicsConfig_.scheduler = PhysicsScheduler::PlagC;
        useSharedJobs_ = true;
    }
    else
        throw std::invalid_argument("Editor play physics scheduler must be native or plagc");
}

std::span<const TransformUpdate>
EditorPlayRuntime::Update(Engine& engine, EditorSession& session, double frameSeconds)
{
    using Clock = std::chrono::steady_clock;
    const auto started = Clock::now();
    simulationDelta_ = 0.0;

    if (!session.Playing())
    {
        Reset();
        engine.timings.physicsMs = 0.0;
        return {};
    }

    const auto generation = session.PlayGeneration();
    if (!physics_ || generation_ != generation)
    {
        physics_.reset();
        generation_ = generation;
        physicsConfig_.sharedJobs = useSharedJobs_ ? &engine.Jobs() : nullptr;
        physics_ = std::make_unique<PhysicsSystem>(session.RuntimeWorld(), physicsConfig_);
    }

    if (session.Paused())
    {
        if (session.ConsumeStep())
        {
            simulationDelta_ = 1.0 / physicsConfig_.fixedHz;
            physics_->Advance(simulationDelta_);
        }
        else
        {
            physics_->Synchronize();
        }
    }
    else
    {
        simulationDelta_ = frameSeconds;
        physics_->Advance(frameSeconds);
    }

    engine.timings.physicsMs =
        std::chrono::duration<double, std::milli>(Clock::now() - started).count();
    return physics_->Presentation();
}

void EditorPlayRuntime::Reset() noexcept
{
    physics_.reset();
    generation_ = 0;
    simulationDelta_ = 0.0;
}

} // namespace PlagC::Editor
