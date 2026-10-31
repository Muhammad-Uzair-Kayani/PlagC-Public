#include "pgcpch.h"
#include "EditorProfilerPanel.h"
#include "ImGuiRendererBridge.h"
#include <PlagC/Application.h>
#include <PlagC/Editor/EditorSession.h>
#include <PlagC/Renderer/Backend.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <chrono>
#include <cfloat>
#include <cstdint>
#include <vector>

namespace PlagC::Editor {
namespace {

constexpr std::size_t HistoryCapacity = 120;
using Clock = std::chrono::steady_clock;

struct ProfilerPanelState
{
    bool frozen = false;
    bool haveJobBaseline = false;
    Clock::time_point lastSample{};
    std::uint64_t lastBusyNanoseconds = 0;

    FrameProfile frame{};
    JobStats jobs{};
    std::size_t entities = 0;
    std::size_t assets = 0;
    double uiSubmitMs = 0.0;
    float jobUtilization = 0.0f;

    std::vector<float> fpsHistory;
    std::vector<float> frameHistory;
    std::vector<float> cpuHistory;
    std::vector<float> gpuHistory;
    std::vector<float> uiHistory;
    std::vector<float> jobsHistory;
};

ProfilerPanelState state;

void PushHistory(std::vector<float>& history, float value)
{
    if (history.size() == HistoryCapacity)
        history.erase(history.begin());
    history.push_back(value);
}

void ResetSamplingBaseline()
{
    state.haveJobBaseline = false;
    state.lastBusyNanoseconds = 0;
    state.lastSample = {};
}

void ClearHistory()
{
    state.fpsHistory.clear();
    state.frameHistory.clear();
    state.cpuHistory.clear();
    state.gpuHistory.clear();
    state.uiHistory.clear();
    state.jobsHistory.clear();
    ResetSamplingBaseline();
}

void CaptureProfilerSample(Engine& engine, const EditorSession& session)
{
    if (state.frozen)
        return;

    const auto now = Clock::now();
    if (state.lastSample != Clock::time_point{} &&
        now - state.lastSample < std::chrono::seconds(1))
        return;

    const JobStats jobs = engine.Jobs().Stats();
    float utilization = 0.0f;
    if (state.haveJobBaseline && jobs.busyNanoseconds >= state.lastBusyNanoseconds)
    {
        const auto elapsedNanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(
            now - state.lastSample).count();
        const unsigned workers = engine.Jobs().WorkerCount();
        if (elapsedNanoseconds > 0 && workers > 0)
        {
            const std::uint64_t busyDelta = jobs.busyNanoseconds - state.lastBusyNanoseconds;
            const double available = static_cast<double>(elapsedNanoseconds) * workers;
            utilization = static_cast<float>(std::clamp(
                available > 0.0 ? 100.0 * static_cast<double>(busyDelta) / available : 0.0,
                0.0, 100.0));
        }
    }

    state.frame = engine.Profile().Latest();
    state.jobs = jobs;
    state.entities = session.ViewWorld().Size();
    state.assets = engine.Assets().Registry().size();
    state.uiSubmitMs = ImGuiRendererBridge::LastSubmitMilliseconds();
    state.jobUtilization = utilization;

    state.lastBusyNanoseconds = jobs.busyNanoseconds;
    state.lastSample = now;
    state.haveJobBaseline = true;

    PushHistory(state.fpsHistory, static_cast<float>(state.frame.fps));
    PushHistory(state.frameHistory, static_cast<float>(state.frame.frameMs));
    PushHistory(state.cpuHistory, static_cast<float>(state.frame.applicationUpdateMs));
    PushHistory(state.gpuHistory,
                static_cast<float>(state.frame.gpuValid ? state.frame.gpuTotalMs : 0.0));
    PushHistory(state.uiHistory, static_cast<float>(state.uiSubmitMs));
    PushHistory(state.jobsHistory, state.jobUtilization);
}

void PlotHistory(const char* label, const std::vector<float>& history,
                 float minimum = 0.0f, float maximum = FLT_MAX)
{
    if (history.empty())
    {
        ImGui::TextDisabled("%s: collecting history...", label);
        return;
    }
    ImGui::PlotLines(label, history.data(), static_cast<int>(history.size()), 0, nullptr,
                     minimum, maximum, ImVec2(-1.0f, 58.0f));
}

void Metric(const char* label, double milliseconds)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextDisabled("%s", label);
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("%.3f ms", milliseconds);
}

void CountMetric(const char* label, std::uint64_t value)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextDisabled("%s", label);
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("%llu", static_cast<unsigned long long>(value));
}

} // namespace

void DrawEditorProfilerPanel(Engine& engine, EditorSession& session)
{
    if (!ImGui::GetCurrentContext())
        return;

    // EditorLayer remains the sole owner of Statistics visibility. This hook runs
    // after EditorLayer::Frame and only appends when that window was active this frame.
    ImGuiWindow* statistics = ImGui::FindWindowByName("Statistics");
    if (!statistics || !statistics->Active)
        return;

    CaptureProfilerSample(engine, session);

    if (!ImGui::Begin("Statistics"))
    {
        ImGui::End();
        return;
    }

    ImGui::SeparatorText("Profiler History");
    if (ImGui::Button(state.frozen ? "Resume" : "Freeze"))
    {
        state.frozen = !state.frozen;
        if (!state.frozen)
            ResetSamplingBaseline();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear History"))
        ClearHistory();
    ImGui::SameLine();
    ImGui::TextDisabled("%s | %zu/%zu samples",
                        state.frozen ? "FROZEN" : "LIVE",
                        state.frameHistory.size(), HistoryCapacity);

    PlotHistory("Frame ms", state.frameHistory);
    PlotHistory("FPS", state.fpsHistory);
    PlotHistory("Application CPU ms", state.cpuHistory);
    PlotHistory("GPU ms", state.gpuHistory);
    PlotHistory("UI submit ms", state.uiHistory);
    PlotHistory("Job utilization %", state.jobsHistory, 0.0f, 100.0f);

    ImGui::SeparatorText("Frame Summary");
    ImGui::Text("FPS: %.1f", state.frame.fps);
    ImGui::Text("Frame: %.3f ms | P95 %.3f | P99 %.3f | Max %.3f",
                state.frame.frameMs, state.frame.frameP95Ms,
                state.frame.frameP99Ms, state.frame.frameMaxMs);
    ImGui::Text("Hitches: >16.7 ms %llu | >33.3 ms %llu",
                static_cast<unsigned long long>(state.frame.hitch16Count),
                static_cast<unsigned long long>(state.frame.hitch33Count));

    if (ImGui::BeginTable("##ProfilerCpuTimings", 2,
                          ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg |
                          ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("CPU stage", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableHeadersRow();
        Metric("Platform / events", state.frame.pollEventsMs);
        Metric("Asset pump", state.frame.assetPumpMs);
        Metric("Application update", state.frame.applicationUpdateMs);
        Metric("Physics", state.frame.physicsMs);
        Metric("Animation", state.frame.animationMs);
        Metric("Scene sync", state.frame.sceneSyncMs);
        Metric("Audio", state.frame.audioMs);
        Metric("Other application", state.frame.applicationOtherMs);
        Metric("UI submit", state.uiSubmitMs);
        Metric("Render submit", state.frame.renderSubmitMs);
        Metric("Renderer CPU", state.frame.rendererCpuMs);
        Metric("Frame pacing", state.frame.pacingMs);
        Metric("Unaccounted", state.frame.unaccountedMs);
        ImGui::EndTable();
    }

    ImGui::SeparatorText("GPU / Backend");
    ImGui::Text("Backend: %s", BackendName(engine.Graphics().Backend()));
    if (state.frame.gpuSupported && state.frame.gpuValid)
    {
        ImGui::Text("GPU frame: %.3f ms | P99 %.3f | Max %.3f",
                    state.frame.gpuTotalMs, state.frame.gpuP99Ms, state.frame.gpuMaxMs);
        ImGui::Text("GPU timestamp samples: %llu | query latency: %u frame(s)",
                    static_cast<unsigned long long>(state.frame.gpuSamples),
                    state.frame.gpuLatencyFrames);
    }
    else
    {
        ImGui::TextDisabled("GPU timing: %s",
            state.frame.gpuSupported ? "waiting for asynchronous timestamps"
                                     : "unsupported by active backend");
    }

    ImGui::SeparatorText("Renderer / Scene Counts");
    if (ImGui::BeginTable("##ProfilerRenderCounts", 2,
                          ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg |
                          ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Counter", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableHeadersRow();
        CountMetric("Draw calls", state.frame.render.drawCalls);
        CountMetric("Triangles", state.frame.render.triangles);
        CountMetric("Instances", state.frame.render.instances);
        CountMetric("Objects", state.frame.render.objects);
        CountMetric("Meshes", state.frame.render.meshes);
        CountMetric("Materials", state.frame.render.materials);
        CountMetric("Lights", state.frame.render.lights);
        CountMetric("Textures", state.frame.render.textures);
        CountMetric("Entities", state.entities);
        CountMetric("Assets", state.assets);
        ImGui::EndTable();
    }

    ImGui::SeparatorText("Job System");
    ImGui::Text("Workers: %u | interval utilization: %.1f%%",
                engine.Jobs().WorkerCount(), state.jobUtilization);
    ImGui::ProgressBar(state.jobUtilization / 100.0f, ImVec2(-1.0f, 0.0f));
    ImGui::Text("Submitted %llu | Executed %llu | Stolen %llu | Failed %llu",
                static_cast<unsigned long long>(state.jobs.submitted),
                static_cast<unsigned long long>(state.jobs.executed),
                static_cast<unsigned long long>(state.jobs.stolen),
                static_cast<unsigned long long>(state.jobs.failed));
    ImGui::TextDisabled("Cumulative worker busy time: %.3f s",
        static_cast<double>(state.jobs.busyNanoseconds) / 1'000'000'000.0);

    ImGui::End();
}

} // namespace PlagC::Editor
