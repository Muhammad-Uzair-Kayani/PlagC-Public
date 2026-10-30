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

void EditorPlayRuntime::Initialize(Engine& engine)
{
    gameplay_.Initialize(engine.Events());
}

void EditorPlayRuntime::Shutdown(Engine& engine) noexcept
{
    Reset(engine);
    gameplay_.Shutdown(engine);
}

void EditorPlayRuntime::ClearPlayState() noexcept
{
    physics_.reset();
    generation_ = 0;
    simulationDelta_ = 0.0;
    gameplayPaused_ = false;
    completeFramePending_ = false;
}

void EditorPlayRuntime::Reset(Engine& engine) noexcept
{
    if (gameplay_.Running())
    {
        try { gameplay_.Stop(engine); }
        catch (...) {}
    }
    ClearPlayState();
    gameplay_.FinishInputFrame();
}

std::span<const TransformUpdate>
EditorPlayRuntime::Update(Engine& engine, EditorSession& session, double frameSeconds)
{
    using Clock = std::chrono::steady_clock;
    const auto started = Clock::now();
    simulationDelta_ = 0.0;
    completeFramePending_ = false;

    if (!std::isfinite(frameSeconds) || frameSeconds < 0.0)
        throw std::invalid_argument("Editor play frame delta must be finite and non-negative");

    if (!session.Playing())
    {
        if (gameplay_.Running())
            gameplay_.Stop(engine);
        ClearPlayState();
        gameplay_.FinishInputFrame();
        engine.timings.physicsMs = 0.0;
        return {};
    }

    const auto generation = session.PlayGeneration();
    if (!physics_ || generation_ != generation)
    {
        if (gameplay_.Running())
            gameplay_.Stop(engine);
        physics_.reset();
        generation_ = 0;
        physicsConfig_.sharedJobs = useSharedJobs_ ? &engine.Jobs() : nullptr;
        physics_ = std::make_unique<PhysicsSystem>(session.RuntimeWorld(), physicsConfig_);

        GameplayContext context{engine, session.RuntimeWorld(), *physics_, gameplay_.Input()};
        gameplay_.Start(context);
        gameplayPaused_ = false;
        if (session.Paused())
        {
            gameplay_.Pause(context);
            gameplayPaused_ = true;
        }
        generation_ = generation;
    }

    GameplayContext context{engine, session.RuntimeWorld(), *physics_, gameplay_.Input()};
    if (session.Paused() != gameplayPaused_)
    {
        if (session.Paused())
            gameplay_.Pause(context);
        else
            gameplay_.Resume(context);
        gameplayPaused_ = session.Paused();
    }

    if (session.Paused() && !session.ConsumeStep())
    {
        physics_->Synchronize();
        gameplay_.FinishInputFrame();
    }
    else
    {
        simulationDelta_ = session.Paused() ? (1.0 / physicsConfig_.fixedHz) : frameSeconds;
        gameplay_.DispatchInput(context);
        gameplay_.Update(context, simulationDelta_);
        physics_->Advance(simulationDelta_, [&](double fixedSeconds) {
            gameplay_.FixedUpdate(context, fixedSeconds);
        });
        gameplay_.DispatchPhysicsEvents(context);
        completeFramePending_ = true;
    }

    engine.timings.physicsMs =
        std::chrono::duration<double, std::milli>(Clock::now() - started).count();
    return physics_->Presentation();
}

void EditorPlayRuntime::CompleteFrame(Engine& engine, EditorSession& session)
{
    using Clock = std::chrono::steady_clock;
    const auto audioStarted = Clock::now();

    if (completeFramePending_ && gameplay_.Running() && session.Playing() && physics_)
    {
        GameplayContext context{engine, session.RuntimeWorld(), *physics_, gameplay_.Input()};
        gameplay_.LateUpdate(context, simulationDelta_);
    }

    if (session.Playing() && engine.Audio().Available())
    {
        // Audio runs after gameplay/physics and animation. Systems own source pause/resume policy;
        // the engine performs source retirement here without crossing into authoring concerns.
        engine.Audio().CollectUnused();
        engine.timings.audioMs =
            std::chrono::duration<double, std::milli>(Clock::now() - audioStarted).count();
    }
    else
        engine.timings.audioMs = 0.0;

    gameplay_.FinishInputFrame();
    completeFramePending_ = false;
}

} // namespace PlagC::Editor
