#!/usr/bin/env python3
"""Focused regressions for Milestone 20 Statistics / Profiler."""

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

class EditorMilestone20StatisticsTests(unittest.TestCase):
    def read(self, path: str) -> str:
        return (ROOT / path).read_text(encoding="utf-8")

    def setUp(self):
        self.panel = self.read("Editor/src/EditorStatisticsPanel.cpp")
        self.header = self.read("Editor/src/EditorStatisticsPanel.h")
        self.layer = self.read("Editor/src/EditorLayer.cpp")
        self.jobs_h = self.read("PlagC/include/PlagC/Jobs.h")
        self.jobs_cpp = self.read("PlagC/src/Jobs.cpp")
        self.docs = self.read("Docs/Editor.md")

    def test_panel_is_composed_and_owns_bounded_history(self):
        self.assertIn("EditorStatisticsPanel statistics_", self.read("Editor/src/EditorLayer.h"))
        self.assertIn("statistics_.Draw(engine, session)", self.layer)
        self.assertIn("HistoryCapacity = 240", self.header)
        self.assertIn('ImGui::PlotLines("Frame ms"', self.panel)
        self.assertIn('ImGui::PlotLines("FPS"', self.panel)
        self.assertIn('ImGui::Checkbox("Freeze"', self.panel)
        self.assertIn('ImGui::Button("Clear History")', self.panel)
        self.assertIn("ConsumePublished()", self.panel)

    def test_required_cpu_and_gpu_timings_are_present(self):
        for token in (
            "pollEventsMs", "assetPumpMs", "applicationUpdateMs", "sceneSyncMs",
            "physicsMs", "animationMs", "audioMs", "applicationOtherMs",
            "renderSubmitMs", "rendererCpuMs", "gpuTotalMs", "gpuP99Ms", "gpuMaxMs",
            "frameP95Ms", "frameP99Ms", "frameMaxMs",
        ):
            self.assertIn(token, self.panel)
        self.assertIn("BackendName(backend_)", self.panel)
        self.assertIn("gpuLatencyFrames", self.panel)

    def test_render_counts_and_scene_counts_are_live(self):
        self.assertIn("engine.Graphics().Statistics()", self.panel)
        for name in ("drawCalls", "triangles", "instances", "objects", "meshes", "materials", "textures", "lights"):
            self.assertIn(f"render_.{name}", self.panel)
        self.assertIn("session.ViewDocument().Snapshot().size()", self.panel)
        self.assertIn("engine.Assets().Registry().size()", self.panel)

    def test_job_utilization_is_measured_not_guessed(self):
        self.assertIn("busyNanoseconds", self.jobs_h)
        self.assertIn("outstanding", self.jobs_h)
        self.assertIn("busyNanoseconds_", self.jobs_h)
        self.assertIn("steady_clock::now()", self.jobs_cpp)
        self.assertIn("busyNanoseconds_.fetch_add", self.jobs_cpp)
        self.assertIn("busyDelta", self.panel)
        self.assertIn("static_cast<double>(workers_)", self.panel)
        self.assertIn('ImGui::PlotLines("Worker utilization %"', self.panel)

    def test_engine_hot_path_stays_imgui_free_and_renderer_neutral(self):
        self.assertNotIn("imgui", self.jobs_h.lower())
        self.assertNotIn("imgui", self.jobs_cpp.lower())
        self.assertNotIn("Ogre::", self.panel)
        self.assertNotIn("JPH::", self.panel)

    def test_roadmap_and_validation_are_wired(self):
        self.assertIn("## 20. Statistics / Profiler — `DONE`", self.docs)
        local = self.read("ValidateEditor.bat")
        ci = self.read(".github/workflows/editor-windows.yml")
        self.assertIn("EditorMilestone20StatisticsTests.py", local)
        self.assertIn("EditorMilestone20StatisticsTests.py", ci)
        self.assertIn("## 23. Editor Theme / Visual System — `NOT STARTED`", self.docs)

if __name__ == "__main__":
    unittest.main()
