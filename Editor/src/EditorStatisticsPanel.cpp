#include "pgcpch.h"
#include "EditorStatisticsPanel.h"
#include <PlagC/Application.h>
#include <PlagC/Editor/EditorSession.h>
#include <imgui.h>
#include <algorithm>
#include <cfloat>
#include <cmath>

namespace PlagC::Editor {
namespace {

void TimingRow(const char* label, double current, double tail = -1.0, double maximum = -1.0)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted(label);
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("%.3f", current);
    ImGui::TableSetColumnIndex(2);
    if (tail >= 0.0)
        ImGui::Text("%.3f", tail);
    else
        ImGui::TextDisabled("—");
    ImGui::TableSetColumnIndex(3);
    if (maximum >= 0.0)
        ImGui::Text("%.3f", maximum);
    else
        ImGui::TextDisabled("—");
}

void CountRow(const char* label, std::uint64_t value)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextUnformatted(label);
    ImGui::TableSetColumnIndex(1);
    ImGui::Text("%llu", static_cast<unsigned long long>(value));
}

} // namespace

void EditorStatisticsPanel::Series::Push(float value) noexcept
{
    values[cursor] = std::isfinite(value) ? value : 0.0f;
    cursor = (cursor + 1) % HistoryCapacity;
    count = std::min(count + 1, HistoryCapacity);
}

void EditorStatisticsPanel::Series::Clear() noexcept
{
    values.fill(0.0f);
    cursor = 0;
    count = 0;
}

int EditorStatisticsPanel::Series::PlotOffset() const noexcept
{
    return count == HistoryCapacity ? static_cast<int>(cursor) : 0;
}

void EditorStatisticsPanel::ClearHistory() noexcept
{
    frameMsHistory_.Clear();
    fpsHistory_.Clear();
    gpuMsHistory_.Clear();
    jobUtilizationHistory_.Clear();
}

void EditorStatisticsPanel::Reset() noexcept
{
    frozen_ = false;
    haveSnapshot_ = false;
    frame_ = {};
    jobs_ = {};
    render_ = {};
    backend_ = RendererBackend::Auto;
    entities_ = assets_ = 0;
    workers_ = 0;
    jobUtilization_ = 0.0;
    previousBusyNanoseconds_ = 0;
    previousJobSample_ = {};
    ClearHistory();
}

void EditorStatisticsPanel::Capture(Engine& engine, EditorSession& session)
{
    frame_ = engine.Profile().Latest();
    jobs_ = engine.Jobs().Stats();
    render_ = engine.Graphics().Statistics();
    backend_ = engine.Graphics().Backend();
    workers_ = engine.Jobs().WorkerCount();
    entities_ = session.ViewDocument().Snapshot().size();
    assets_ = engine.Assets().Registry().size();

    const auto now = std::chrono::steady_clock::now();
    if (previousJobSample_.time_since_epoch().count() != 0 && workers_ != 0)
    {
        const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
            now - previousJobSample_).count();
        const std::uint64_t busyDelta = jobs_.busyNanoseconds >= previousBusyNanoseconds_
            ? jobs_.busyNanoseconds - previousBusyNanoseconds_
            : 0;
        if (elapsed > 0)
        {
            const double capacity = static_cast<double>(elapsed) * static_cast<double>(workers_);
            jobUtilization_ = std::clamp(static_cast<double>(busyDelta) / capacity, 0.0, 1.0);
        }
    }
    previousBusyNanoseconds_ = jobs_.busyNanoseconds;
    previousJobSample_ = now;

    frameMsHistory_.Push(static_cast<float>(frame_.frameMs));
    fpsHistory_.Push(static_cast<float>(frame_.fps));
    gpuMsHistory_.Push(static_cast<float>(frame_.gpuValid ? frame_.gpuTotalMs : 0.0));
    jobUtilizationHistory_.Push(static_cast<float>(jobUtilization_ * 100.0));
    haveSnapshot_ = true;
}

void EditorStatisticsPanel::Draw(Engine& engine, EditorSession& session)
{
    const bool published = frozen_ ? false : engine.Profile().ConsumePublished();
    if (!frozen_ && (!haveSnapshot_ || published))
        Capture(engine, session);

    if (!ImGui::Begin("Statistics"))
    {
        ImGui::End();
        return;
    }

    if (ImGui::Checkbox("Freeze", &frozen_))
    {
        // Re-base utilization after a freeze so the next sample measures a clean interval.
        if (!frozen_)
        {
            previousBusyNanoseconds_ = engine.Jobs().Stats().busyNanoseconds;
            previousJobSample_ = std::chrono::steady_clock::now();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear History"))
        ClearHistory();
    ImGui::SameLine();
    ImGui::TextDisabled("%zu / %zu samples", frameMsHistory_.count, HistoryCapacity);

    if (frozen_)
        ImGui::TextDisabled("Profiler view frozen — engine profiling continues in the background.");

    ImGui::SeparatorText("Frame History");
    if (frameMsHistory_.count)
    {
        ImGui::PlotLines("Frame ms", frameMsHistory_.values.data(),
            static_cast<int>(frameMsHistory_.count), frameMsHistory_.PlotOffset(), nullptr,
            0.0f, FLT_MAX, ImVec2(-1.0f, 70.0f));
        ImGui::PlotLines("FPS", fpsHistory_.values.data(),
            static_cast<int>(fpsHistory_.count), fpsHistory_.PlotOffset(), nullptr,
            0.0f, FLT_MAX, ImVec2(-1.0f, 70.0f));
    }
    else
        ImGui::TextDisabled("Waiting for profiler history...");

    ImGui::Text("Frame %.3f ms  |  %.1f FPS", frame_.frameMs, frame_.fps);
    ImGui::Text("p95 %.3f ms  |  p99 %.3f ms  |  max %.3f ms",
        frame_.frameP95Ms, frame_.frameP99Ms, frame_.frameMaxMs);
    ImGui::Text("Hitches >16.7 ms: %llu  |  >33.3 ms: %llu",
        static_cast<unsigned long long>(frame_.hitch16Count),
        static_cast<unsigned long long>(frame_.hitch33Count));

    ImGui::SeparatorText("CPU / Subsystem Timings");
    if (ImGui::BeginTable("ProfilerTimings", 4,
        ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
    {
        ImGui::TableSetupColumn("Stage");
        ImGui::TableSetupColumn("ms", ImGuiTableColumnFlags_WidthFixed, 72.0f);
        ImGui::TableSetupColumn("p99", ImGuiTableColumnFlags_WidthFixed, 72.0f);
        ImGui::TableSetupColumn("max", ImGuiTableColumnFlags_WidthFixed, 72.0f);
        ImGui::TableHeadersRow();
        TimingRow("Events", frame_.pollEventsMs, frame_.eventsP99Ms, frame_.eventsMaxMs);
        TimingRow("Asset pump", frame_.assetPumpMs);
        TimingRow("Application update", frame_.applicationUpdateMs, frame_.applicationP99Ms, frame_.applicationMaxMs);
        TimingRow("Scene sync", frame_.sceneSyncMs);
        TimingRow("Physics", frame_.physicsMs);
        TimingRow("Animation", frame_.animationMs);
        TimingRow("Audio", frame_.audioMs);
        TimingRow("Editor / UI + other", frame_.applicationOtherMs);
        TimingRow("Render submit", frame_.renderSubmitMs, frame_.renderP99Ms, frame_.renderMaxMs);
        TimingRow("Renderer CPU", frame_.rendererCpuMs);
        TimingRow("Frame pacing", frame_.pacingMs);
        TimingRow("Unaccounted", frame_.unaccountedMs);
        ImGui::EndTable();
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("Editor / UI + other is application time not attributed to physics, animation, scene sync or audio.");

    ImGui::SeparatorText("Animation Breakdown");
    ImGui::Text("Pose %.3f ms  Skinning %.3f ms  Upload %.3f ms  Other %.3f ms",
        frame_.animation.poseMs, frame_.animation.skinningMs,
        frame_.animation.uploadMs, frame_.animation.otherMs);

    ImGui::SeparatorText("Renderer / GPU");
    ImGui::Text("Backend: %s", BackendName(backend_));
    ImGui::Text("GPU timing: %s", frame_.gpuSupported ? (frame_.gpuValid ? "valid" : "supported / pending") : "unsupported");
    if (frame_.gpuValid)
    {
        ImGui::Text("GPU %.3f ms  |  p99 %.3f ms  |  max %.3f ms  |  latency %u frame(s)",
            frame_.gpuTotalMs, frame_.gpuP99Ms, frame_.gpuMaxMs, frame_.gpuLatencyFrames);
        ImGui::PlotLines("GPU ms", gpuMsHistory_.values.data(),
            static_cast<int>(gpuMsHistory_.count), gpuMsHistory_.PlotOffset(), nullptr,
            0.0f, FLT_MAX, ImVec2(-1.0f, 55.0f));
    }
    else
        ImGui::TextDisabled("GPU adapter naming is not exposed by the renderer-neutral contract; backend and timing capability are shown here.");

    ImGui::Text("Renderer animation %.3f ms  Frame %.3f ms  Metrics %.3f ms  Other %.3f ms",
        frame_.renderer.animationTickMs, frame_.renderer.renderFrameMs,
        frame_.renderer.metricsMs, frame_.renderer.otherMs);

    ImGui::SeparatorText("Render Counts");
    if (ImGui::BeginTable("RenderCounts", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
    {
        CountRow("Draw calls", render_.drawCalls);
        CountRow("Triangles", render_.triangles);
        CountRow("Instances", render_.instances);
        CountRow("Objects", render_.objects);
        CountRow("Meshes", render_.meshes);
        CountRow("Materials", render_.materials);
        CountRow("Textures", render_.textures);
        CountRow("Lights", render_.lights);
        CountRow("Scene entities", entities_);
        CountRow("Registered assets", assets_);
        ImGui::EndTable();
    }

    ImGui::SeparatorText("Job System");
    ImGui::Text("Workers: %u  |  Utilization: %.1f%%  |  Outstanding: %llu",
        workers_, jobUtilization_ * 100.0,
        static_cast<unsigned long long>(jobs_.outstanding));
    ImGui::PlotLines("Worker utilization %", jobUtilizationHistory_.values.data(),
        static_cast<int>(jobUtilizationHistory_.count), jobUtilizationHistory_.PlotOffset(), nullptr,
        0.0f, 100.0f, ImVec2(-1.0f, 55.0f));
    ImGui::Text("Submitted %llu  Executed %llu  Stolen %llu  Failed %llu",
        static_cast<unsigned long long>(jobs_.submitted),
        static_cast<unsigned long long>(jobs_.executed),
        static_cast<unsigned long long>(jobs_.stolen),
        static_cast<unsigned long long>(jobs_.failed));

    ImGui::End();
}

} // namespace PlagC::Editor
