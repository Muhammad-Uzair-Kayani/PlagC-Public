#pragma once
#include <PlagC/Events.h>
#include <PlagC/Renderer/Renderer.h>
#include <cstdint>
namespace PlagC
{
enum class LatencyStage : std::uint8_t
{
    GameRead,
    MovementIntent,
    PhysicsInput,
    PhysicsStep,
    PhysicsOutput,
    WorldTransform,
    Camera,
    Animation,
    SceneSync,
    RenderSubmit,
    Count
};
struct LatencyStageStats
{
    double averageMs = 0, p99Ms = 0, maxMs = 0;
    double averageFrames = 0, maxFrames = 0;
    std::uint64_t samples = 0;
};
struct LatencySnapshot
{
    std::uint64_t transitions = 0, presses = 0, releases = 0, movementReleases = 0, mouseMoves = 0;
    double pollMs = 0, pollP99Ms = 0, pollMaxMs = 0, oldestPendingMs = 0;
    LatencyStageStats stages[static_cast<unsigned>(LatencyStage::Count)]{};
    unsigned physicsSteps = 0;
    double physicsAlpha = 0, physicsDroppedMs = 0;
    bool gpuSupported = false, gpuValid = false;
    double gpuMs = 0;
    std::uint32_t gpuQueryLatencyFrames = 0;
    std::uint64_t frame = 0;
};
// Correlates input transitions with every CPU-visible stage that can add response latency.
// It is diagnostic instrumentation only: Debug/Release define PC_ENABLE_PROFILING; Dist calls are no-ops.
class LatencyProfiler
{
  public:
    static void BeginFrame();
    static std::uint64_t Frame();
    static void BeginPoll();
    static void EndPoll();
    static void OnCallback(const Event &);
    static void OnInputStateApplied(const Event &);
    static void OnGameRead();
    static void OnMovementIntent(Vec3 desiredVelocity);
    static void OnPhysicsInput(Vec3 desiredVelocity);
    static void OnPhysicsPlan(unsigned steps, double alpha, double droppedSeconds);
    static void OnPhysicsStep();
    static void OnPhysicsOutput();
    static void OnWorldTransform();
    static void OnCamera();
    static void OnAnimation();
    static void OnSceneSync();
    static void OnRenderSubmit();
    static void OnGpuTiming(bool supported, bool valid, double totalMs, std::uint32_t queryLatencyFrames);
    static LatencySnapshot ConsumeOneSecond();
};
} // namespace PlagC
