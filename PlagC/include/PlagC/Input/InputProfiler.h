#pragma once
#include <PlagC/Events.h>
#include <cstdint>
namespace PlagC
{
struct InputProfileSnapshot
{
    std::uint64_t transitions = 0;
    std::uint64_t presses = 0;
    std::uint64_t releases = 0;
    std::uint64_t mouseMoves = 0;
    double pollMs = 0;
    double pollP99Ms = 0;
    double pollMaxMs = 0;
    double callbackToGameAvgMs = 0;
    double callbackToGameP99Ms = 0;
    double callbackToGameMaxMs = 0;
    double releaseToGameAvgMs = 0;
    double releaseToGameP99Ms = 0;
    double releaseToGameMaxMs = 0;
    double oldestTransitionAgeMs = 0;
};

// Diagnostic profiler for the input sampling path. Debug/Release only;
// all methods compile to no-ops in Dist because PC_ENABLE_PROFILING is absent.
class InputProfiler
{
public:
    static void BeginPoll();
    static void EndPoll();
    static void OnCallback(const Event& event);
    // Call immediately before the game's OnFrame. This measures how long a
    // GLFW transition waited before gameplay had a chance to observe it.
    static void OnGameConsumePoint();
    static InputProfileSnapshot ConsumeOneSecond();
};
} // namespace PlagC
