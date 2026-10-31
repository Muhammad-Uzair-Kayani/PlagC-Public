#!/usr/bin/env python3
"""Focused source regressions for Milestone 19 Console."""

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class EditorMilestone19ConsoleTests(unittest.TestCase):
    def read(self, relative: str) -> str:
        return (ROOT / relative).read_text(encoding="utf-8")

    def test_console_has_compact_severity_presentation_and_expandable_details(self):
        panel = self.read("Editor/src/EditorConsolePanel.cpp")
        header = self.read("Editor/src/EditorDataPanels.h")
        self.assertIn("SeverityBadge", panel)
        self.assertIn("SeverityColour", panel)
        self.assertIn("CompactMessage", panel)
        self.assertIn("expandedConsole_", header)
        self.assertIn("Collapse Details", panel)
        self.assertIn("Expand Details", panel)
        self.assertIn("ImGui::TextWrapped", panel)

    def test_category_and_search_filters_remain_logger_backed(self):
        panel = self.read("Editor/src/EditorConsolePanel.cpp")
        models = self.read("PlagC/src/Editor/EditorModels.cpp")
        self.assertIn('InputTextWithHint("##console-search"', panel)
        self.assertIn('ImGui::BeginCombo("Category"', panel)
        self.assertIn("BuildCategories(all)", panel)
        self.assertIn("FilterLogRecords(all, consoleFilter_)", panel)
        self.assertIn("record.category +", models)
        self.assertIn("record.thread +", models)
        self.assertIn("record.message", models)

    def test_repeated_messages_are_collapsed_without_mutating_engine_log(self):
        panel = self.read("Editor/src/EditorConsolePanel.cpp")
        self.assertIn("BuildConsoleGroups", panel)
        self.assertIn("SameRepeat", panel)
        self.assertIn("++result.back().repeats", panel)
        self.assertIn("collapseConsoleRepeats_", panel)
        self.assertIn("Collapsed %u identical consecutive records.", panel)
        self.assertNotIn("engine.Logger().Write", panel)

    def test_context_copy_actions_and_clear_are_present(self):
        panel = self.read("Editor/src/EditorConsolePanel.cpp")
        self.assertIn('BeginPopupContextItem("ConsoleEntryContext")', panel)
        self.assertIn('"Copy Message"', panel)
        self.assertIn('"Copy Details"', panel)
        self.assertIn('"Copy Category"', panel)
        self.assertGreaterEqual(panel.count("ImGui::SetClipboardText"), 3)
        self.assertIn("engine.Logger().Clear()", panel)

    def test_profiler_dumps_are_separated_from_normal_logs(self):
        panel = self.read("Editor/src/EditorConsolePanel.cpp")
        self.assertIn("IsProfilerRecord", panel)
        self.assertIn('return LowerConsole(record.category) == "profiler"', panel)
        self.assertIn('ImGui::BeginTabItem("Logs")', panel)
        self.assertIn("###ProfilerTab", panel)
        self.assertIn("Large PROFILER reports are isolated here", panel)
        self.assertIn("BuildConsoleGroups(filtered, consoleCategory_, false", panel)
        self.assertIn('BuildConsoleGroups(filtered, "All", true', panel)

    def test_m19_is_wired_and_roadmap_preserves_unowned_milestones(self):
        local = self.read("ValidateEditor.bat")
        ci = self.read(".github/workflows/editor-windows.yml")
        docs = self.read("Docs/Editor.md")
        self.assertIn("EditorMilestone19ConsoleTests.py", local)
        self.assertIn("EditorMilestone19ConsoleTests.py", ci)
        self.assertIn("## 19. Console — `DONE`", docs)
        self.assertIn("## 20. Statistics / Profiler — `FOUNDATION`", docs)


if __name__ == "__main__":
    unittest.main()
