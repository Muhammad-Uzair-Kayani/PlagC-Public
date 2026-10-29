#include "pgcpch.h"
#include <PlagC/LatencyProfiler.h>

#ifdef PC_ENABLE_PROFILING
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <deque>
#include <vector>

namespace PlagC
{
namespace
{
using Clock = std::chrono::steady_clock;
constexpr unsigned StageCount = static_cast<unsigned>(LatencyStage::Count);

struct Transition
{
    std::uint64_t callbackFrame = 0;
    Clock::time_point callback{};
    std::array<bool, StageCount> marked{};
};

struct Samples
{
    std::vector<double> ms, frames;
};

struct State
{
    Clock::time_point pollStart{}, windowStart = Clock::now();
    std::uint64_t frame = 0;
    std::uint64_t transitions = 0, presses = 0, releases = 0, mouseMoves = 0;
    std::deque<Transition> pending;
    std::vector<double> pollSamples;
    std::array<Samples, StageCount> stage;
    unsigned physicsSteps = 0;
    double physicsAlpha = 0.0, physicsDroppedMs = 0.0;
    bool gpuSupported = false, gpuValid = false;
    double gpuMs = 0.0;
    std::uint32_t gpuQueryLatencyFrames = 0, gpuOutstandingFrames = 0;
} state;

double Ms(Clock::duration duration)
{
    return std::chrono::duration<double, std::milli>(duration).count();
}

double Average(const std::vector<double> &values)
{
    double sum = 0.0;
    for (double value : values)
        sum += value;
    return values.empty() ? 0.0 : sum / double(values.size());
}

double Maximum(const std::vector<double> &values)
{
    return values.empty() ? 0.0 : *std::max_element(values.begin(), values.end());
}

double Percentile(std::vector<double> values, double q)
{
    if (values.empty())
        return 0.0;
    std::sort(values.begin(), values.end());
    const auto index = static_cast<std::size_t>(std::ceil(q * double(values.size())) - 1.0);
    return values[std::min(index, values.size() - 1)];
}

void Mark(LatencyStage stage)
{
    const auto now = Clock::now();
    const auto index = static_cast<unsigned>(stage);
    for (auto &transition : state.pending)
    {
        if (transition.marked[index])
            continue;
        transition.marked[index] = true;
        state.stage[index].ms.push_back(Ms(now - transition.callback));
        state.stage[index].frames.push_back(double(state.frame - transition.callbackFrame));
    }

    if (stage == LatencyStage::RenderSubmit)
        while (!state.pending.empty() &&
               state.frame - state.pending.front().callbackFrame > 8)
            state.pending.pop_front();
}
} // namespace

void LatencyProfiler::BeginFrame() { ++state.frame; }
std::uint64_t LatencyProfiler::Frame() { return state.frame; }
void LatencyProfiler::BeginPoll() { state.pollStart = Clock::now(); }

void LatencyProfiler::EndPoll()
{
    if (state.pollStart != Clock::time_point{})
        state.pollSamples.push_back(Ms(Clock::now() - state.pollStart));
}

void LatencyProfiler::OnCallback(const Event &event)
{
    if (event.type == EventType::MouseMoved)
    {
        ++state.mouseMoves;
        return;
    }

    const bool press =
        event.type == EventType::KeyPressed || event.type == EventType::MousePressed;
    const bool release =
        event.type == EventType::KeyReleased || event.type == EventType::MouseReleased;
    if (!press && !release)
        return;

    ++state.transitions;
    if (release)
        ++state.releases;
    else
        ++state.presses;

    state.pending.push_back({state.frame, Clock::now(), {}});
    while (state.pending.size() > 128)
        state.pending.pop_front();
}

void LatencyProfiler::OnInputStateApplied(const Event &) {}
void LatencyProfiler::OnInputRead() { Mark(LatencyStage::InputRead); }
void LatencyProfiler::OnMovementIntent(Vec3) { Mark(LatencyStage::SimulationInput); }
void LatencyProfiler::OnPhysicsInput(Vec3) { Mark(LatencyStage::PhysicsInput); }

void LatencyProfiler::OnPhysicsPlan(unsigned steps, double alpha, double droppedSeconds)
{
    state.physicsSteps = steps;
    state.physicsAlpha = alpha;
    state.physicsDroppedMs = droppedSeconds * 1000.0;
}

void LatencyProfiler::OnPhysicsStep() { Mark(LatencyStage::PhysicsStep); }
void LatencyProfiler::OnPhysicsOutput() { Mark(LatencyStage::PhysicsOutput); }
void LatencyProfiler::OnWorldTransform() { Mark(LatencyStage::WorldTransform); }
void LatencyProfiler::OnCamera() { Mark(LatencyStage::Camera); }
void LatencyProfiler::OnAnimation() { Mark(LatencyStage::Animation); }
void LatencyProfiler::OnSceneSync() { Mark(LatencyStage::SceneSync); }
void LatencyProfiler::OnRenderSubmit() { Mark(LatencyStage::RenderSubmit); }

void LatencyProfiler::OnGpuTiming(bool supported, bool valid, double totalMs,
                                  std::uint32_t latency)
{
    state.gpuSupported = supported;
    state.gpuValid = valid;
    if (valid)
    {
        state.gpuMs = totalMs;
        state.gpuQueryLatencyFrames = latency;
    }
}

void LatencyProfiler::OnGpuTiming(bool supported, bool valid, double totalMs,
                                  std::uint32_t latency, std::uint32_t outstanding)
{
    OnGpuTiming(supported, valid, totalMs, latency);
    state.gpuOutstandingFrames = outstanding;
}

LatencySnapshot LatencyProfiler::ConsumeOneSecond()
{
    LatencySnapshot out{};
    const auto now = Clock::now();
    if (Ms(now - state.windowStart) < 1000.0)
        return out;

    out.transitions = state.transitions;
    out.presses = state.presses;
    out.releases = state.releases;
    out.mouseMoves = state.mouseMoves;
    out.pollMs = Average(state.pollSamples);
    out.pollP99Ms = Percentile(state.pollSamples, .99);
    out.pollMaxMs = Maximum(state.pollSamples);

    for (const auto &transition : state.pending)
        out.oldestPendingMs =
            std::max(out.oldestPendingMs, Ms(now - transition.callback));

    for (unsigned i = 0; i < StageCount; ++i)
    {
        auto &stats = out.stages[i];
        stats.averageMs = Average(state.stage[i].ms);
        stats.p99Ms = Percentile(state.stage[i].ms, .99);
        stats.maxMs = Maximum(state.stage[i].ms);
        stats.averageFrames = Average(state.stage[i].frames);
        stats.maxFrames = Maximum(state.stage[i].frames);
        stats.samples = state.stage[i].ms.size();
        state.stage[i].ms.clear();
        state.stage[i].frames.clear();
    }

    out.physicsSteps = state.physicsSteps;
    out.physicsAlpha = state.physicsAlpha;
    out.physicsDroppedMs = state.physicsDroppedMs;
    out.gpuSupported = state.gpuSupported;
    out.gpuValid = state.gpuValid;
    out.gpuMs = state.gpuMs;
    out.gpuQueryLatencyFrames = state.gpuQueryLatencyFrames;
    out.gpuOutstandingFrames = state.gpuOutstandingFrames;
    out.frame = state.frame;

    state.pollSamples.clear();
    state.transitions = state.presses = state.releases = state.mouseMoves = 0;
    state.windowStart = now;
    return out;
}
} // namespace PlagC

#else

namespace PlagC
{
void LatencyProfiler::BeginFrame() {}
std::uint64_t LatencyProfiler::Frame() { return 0; }
void LatencyProfiler::BeginPoll() {}
void LatencyProfiler::EndPoll() {}
void LatencyProfiler::OnCallback(const Event &) {}
void LatencyProfiler::OnInputStateApplied(const Event &) {}
void LatencyProfiler::OnInputRead() {}
void LatencyProfiler::OnMovementIntent(Vec3) {}
void LatencyProfiler::OnPhysicsInput(Vec3) {}
void LatencyProfiler::OnPhysicsPlan(unsigned, double, double) {}
void LatencyProfiler::OnPhysicsStep() {}
void LatencyProfiler::OnPhysicsOutput() {}
void LatencyProfiler::OnWorldTransform() {}
void LatencyProfiler::OnCamera() {}
void LatencyProfiler::OnAnimation() {}
void LatencyProfiler::OnSceneSync() {}
void LatencyProfiler::OnRenderSubmit() {}
void LatencyProfiler::OnGpuTiming(bool, bool, double, std::uint32_t) {}
void LatencyProfiler::OnGpuTiming(bool, bool, double, std::uint32_t, std::uint32_t) {}
LatencySnapshot LatencyProfiler::ConsumeOneSecond() { return {}; }
} // namespace PlagC

#endif
