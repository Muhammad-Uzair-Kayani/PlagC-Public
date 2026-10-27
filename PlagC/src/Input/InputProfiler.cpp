#include "pgcpch.h"
#include <PlagC/Input/InputProfiler.h>
#ifdef PC_ENABLE_PROFILING
#include <algorithm>
#include <chrono>
#include <cmath>
#include <vector>
namespace PlagC
{
namespace
{
using Clock = std::chrono::steady_clock;
struct PendingTransition { Clock::time_point callback; bool release = false; };
struct State
{
    Clock::time_point pollStart{};
    Clock::time_point windowStart = Clock::now();
    std::vector<PendingTransition> pending;
    std::vector<double> pollSamples;
    std::vector<double> consumeSamples;
    std::vector<double> releaseSamples;
    std::uint64_t transitions = 0, presses = 0, releases = 0, mouseMoves = 0;
};
State state;
double Ms(Clock::duration d) { return std::chrono::duration<double, std::milli>(d).count(); }
double Percentile(std::vector<double> values, double q)
{
    if (values.empty()) return 0.0;
    std::sort(values.begin(), values.end());
    const auto i = static_cast<std::size_t>(std::ceil(q * double(values.size())) - 1.0);
    return values[std::min(i, values.size() - 1)];
}
double Max(const std::vector<double>& v) { return v.empty() ? 0.0 : *std::max_element(v.begin(), v.end()); }
double Avg(const std::vector<double>& v)
{
    if (v.empty()) return 0.0;
    double total = 0.0; for (double x : v) total += x; return total / double(v.size());
}
}
void InputProfiler::BeginPoll() { state.pollStart = Clock::now(); }
void InputProfiler::EndPoll()
{
    if (state.pollStart != Clock::time_point{}) state.pollSamples.push_back(Ms(Clock::now() - state.pollStart));
}
void InputProfiler::OnCallback(const Event& event)
{
    const auto now = Clock::now();
    if (event.type == EventType::MouseMoved) { ++state.mouseMoves; return; }
    const bool press = event.type == EventType::KeyPressed || event.type == EventType::MousePressed;
    const bool release = event.type == EventType::KeyReleased || event.type == EventType::MouseReleased;
    if (!press && !release) return; // Ignore key-repeat: it is not a physical transition.
    ++state.transitions; if (release) ++state.releases; else ++state.presses;
    state.pending.push_back({now, release});
}
void InputProfiler::OnGameConsumePoint()
{
    const auto now = Clock::now();
    for (const auto& transition : state.pending)
    {
        const double age = Ms(now - transition.callback);
        state.consumeSamples.push_back(age);
        if (transition.release) state.releaseSamples.push_back(age);
    }
    state.pending.clear();
}
InputProfileSnapshot InputProfiler::ConsumeOneSecond()
{
    InputProfileSnapshot out{};
    const auto now = Clock::now();
    if (Ms(now - state.windowStart) < 1000.0) return out;
    out.transitions = state.transitions; out.presses = state.presses; out.releases = state.releases; out.mouseMoves = state.mouseMoves;
    out.pollMs = Avg(state.pollSamples); out.pollP99Ms = Percentile(state.pollSamples, .99); out.pollMaxMs = Max(state.pollSamples);
    out.callbackToGameAvgMs = Avg(state.consumeSamples); out.callbackToGameP99Ms = Percentile(state.consumeSamples, .99); out.callbackToGameMaxMs = Max(state.consumeSamples);
    out.releaseToGameAvgMs = Avg(state.releaseSamples); out.releaseToGameP99Ms = Percentile(state.releaseSamples, .99); out.releaseToGameMaxMs = Max(state.releaseSamples);
    for (const auto& transition : state.pending) out.oldestTransitionAgeMs = std::max(out.oldestTransitionAgeMs, Ms(now - transition.callback));
    state.pollSamples.clear(); state.consumeSamples.clear(); state.releaseSamples.clear();
    state.transitions = state.presses = state.releases = state.mouseMoves = 0; state.windowStart = now;
    return out;
}
} // namespace PlagC
#else
namespace PlagC
{
void InputProfiler::BeginPoll() {}
void InputProfiler::EndPoll() {}
void InputProfiler::OnCallback(const Event&) {}
void InputProfiler::OnGameConsumePoint() {}
InputProfileSnapshot InputProfiler::ConsumeOneSecond() { return {}; }
}
#endif
