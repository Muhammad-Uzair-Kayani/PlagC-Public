#!/usr/bin/env python3
"""Focused source regressions for Milestone 20 Statistics / Profiler."""

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class EditorMilestone20StatisticsTests(unittest.TestCase):
    def read(self, relative: str) -> str:
        return (ROOT / relative).read_text(encoding="utf-8")

    def test_profiler_panel_keeps_bounded_history_graphs_and_freeze_controls(self):
        panel = self.read("Editor/src/EditorProfilerPanel.cpp")
        self.assertIn("HistoryCapacity = 120", panel)
        self.assertIn("ImGui::PlotLines", panel)
        for label in (
            '"Frame ms"', '"FPS"', '"Application CPU ms"', '"GPU ms"',
            '"UI submit ms"', '"Job utilization %"', '"Freeze"', '"Resume"',
            '"Clear History"'
        ):
            self.assertIn(label, panel)
        self.assertIn("state.frozen", panel)
        self.assertIn("history.erase(history.begin())", panel)

    def test_cpu_gpu_and_engine_subsystem_timings_are_exposed(self):
        panel = self.read("Editor/src/EditorProfilerPanel.cpp")
        for field in (
            "pollEventsMs", "assetPumpMs", "applicationUpdateMs", "physicsMs",
            "animationMs", "sceneSyncMs", "audioMs", "renderSubmitMs",
            "rendererCpuMs", "pacingMs", "unaccountedMs", "frameP95Ms",
            "frameP99Ms", "frameMaxMs", "hitch16Count", "hitch33Count"
        ):
            self.assertIn(field, panel)
        for field in (
            "gpuSupported", "gpuValid", "gpuTotalMs", "gpuP99Ms", "gpuMaxMs",
            "gpuLatencyFrames", "gpuSamples"
        ):
            self.assertIn(field, panel)

    def test_renderer_scene_and_backend_counts_are_visible(self):
        panel = self.read("Editor/src/EditorProfilerPanel.cpp")
        self.assertIn("BackendName(engine.Graphics().Backend())", panel)
        for field in (
            "render.drawCalls", "render.triangles", "render.instances", "render.objects",
            "render.meshes", "render.materials", "render.lights", "render.textures"
        ):
            self.assertIn(field, panel)
        self.assertIn("session.ViewWorld().Size()", panel)
        self.assertIn("engine.Assets().Registry().size()", panel)

    def test_job_utilization_uses_real_worker_busy_time_not_job_count_guessing(self):
        header = self.read("PlagC/include/PlagC/Jobs.h")
        source = self.read("PlagC/src/Jobs.cpp")
        panel = self.read("Editor/src/EditorProfilerPanel.cpp")
        self.assertIn("busyNanoseconds", header)
        self.assertIn("busyNanoseconds_", header)
        self.assertIn("std::chrono::steady_clock::now()", source)
        self.assertIn("busyNanoseconds_.fetch_add", source)
        self.assertIn("engine.Jobs().WorkerCount()", panel)
        self.assertIn("jobs.busyNanoseconds - state.lastBusyNanoseconds", panel)
        self.assertIn("jobUtilization", panel)

    def test_ui_submission_cost_is_measured_in_renderer_neutral_bridge(self):
        header = self.read("Editor/src/ImGuiRendererBridge.h")
        source = self.read("Editor/src/ImGuiRendererBridge.cpp")
        panel = self.read("Editor/src/EditorProfilerPanel.cpp")
        self.assertIn("LastSubmitMilliseconds()", header)
        self.assertIn("lastUiSubmitNanoseconds", source)
        self.assertIn("PublishUiSubmit(submitStart)", source)
        self.assertIn("ImGuiRendererBridge::LastSubmitMilliseconds()", panel)
        self.assertNotIn("Ogre::", panel)

    def test_profiler_extends_existing_statistics_window_without_second_window_owner(self):
        layer = self.read("Editor/src/EditorLayer.cpp")
        preferences = self.read("Editor/src/EditorPreferences.cpp")
        menu = self.read("Editor/src/EditorMenuController.cpp")
        panel = self.read("Editor/src/EditorProfilerPanel.cpp")
        self.assertIn("DrawStatistics(engine, session)", layer)
        self.assertIn('ImGui::MenuItem("Statistics", nullptr, &owner_.showStatistics_)', menu)
        self.assertIn("DrawEditorProfilerPanel(engine, session);", preferences)
        self.assertIn('ImGui::FindWindowByName("Statistics")', panel)
        self.assertIn("statistics->Active", panel)

    def test_m20_is_wired_and_documented(self):
        local = self.read("ValidateEditor.bat")
        ci = self.read(".github/workflows/editor-windows.yml")
        docs = self.read("Docs/Editor.md")
        self.assertIn("EditorMilestone20StatisticsTests.py", local)
        self.assertIn("EditorMilestone20StatisticsTests.py", ci)
        self.assertIn("## 20. Statistics / Profiler — `DONE`", docs)
        self.assertIn("Continue this explicitly assigned batch with **23 → 26 → 29**", docs)


if __name__ == "__main__":
    unittest.main()
