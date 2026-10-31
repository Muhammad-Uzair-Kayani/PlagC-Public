#!/usr/bin/env python3
"""Focused source regressions for Milestone 23 Editor Theme / Visual System."""

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class EditorMilestone23ThemeTests(unittest.TestCase):
    def read(self, relative: str) -> str:
        return (ROOT / relative).read_text(encoding="utf-8")

    def test_plagc_theme_defines_typography_spacing_padding_rounding_and_borders(self):
        theme = self.read("Editor/src/EditorTheme.cpp")
        for token in (
            "io.FontDefault = EditorVisualAssets::TextFont()",
            "io.FontGlobalScale = 1.0f",
            "style.WindowPadding = ImVec2(10.0f, 9.0f)",
            "style.FramePadding = ImVec2(7.0f, 4.0f)",
            "style.CellPadding = ImVec2(7.0f, 5.0f)",
            "style.ItemSpacing = ImVec2(7.0f, 6.0f)",
            "style.ItemInnerSpacing = ImVec2(5.0f, 4.0f)",
            "style.WindowBorderSize = 1.0f",
            "style.ChildBorderSize = 1.0f",
            "style.PopupBorderSize = 1.0f",
            "style.WindowRounding = 4.0f",
            "style.FrameRounding = 3.0f",
            "style.TabRounding = 4.0f",
        ):
            self.assertIn(token, theme)

    def test_theme_has_explicit_contrast_selection_hover_disabled_and_docking_colours(self):
        theme = self.read("Editor/src/EditorTheme.cpp")
        for colour in (
            "ImGuiCol_Text", "ImGuiCol_TextDisabled", "ImGuiCol_WindowBg",
            "ImGuiCol_ChildBg", "ImGuiCol_Border", "ImGuiCol_ButtonHovered",
            "ImGuiCol_ButtonActive", "ImGuiCol_Header", "ImGuiCol_HeaderHovered",
            "ImGuiCol_HeaderActive", "ImGuiCol_Tab", "ImGuiCol_TabHovered",
            "ImGuiCol_TabSelected", "ImGuiCol_DockingPreview",
            "ImGuiCol_DockingEmptyBg", "ImGuiCol_TextSelectedBg",
            "ImGuiCol_DragDropTarget", "ImGuiCol_ModalWindowDimBg",
        ):
            self.assertIn(colour, theme)
        self.assertIn("kAccent", theme)
        self.assertIn("kMuted", theme)
        self.assertIn("kBorder", theme)

    def test_existing_component_cards_and_toolbar_inherit_the_visual_system(self):
        theme = self.read("Editor/src/EditorTheme.cpp")
        inspector = self.read("Editor/src/EditorComponentInspector.cpp")
        toolbar = self.read("Editor/src/EditorToolbar.cpp")
        self.assertIn("Collapsing headers are the Inspector's component-card headers", theme)
        self.assertIn("BeginComponentCard", inspector)
        self.assertIn("ImGui::CollapsingHeader", inspector)
        self.assertIn('ImGui::BeginChild("##PlagC.Editor.Toolbar"', toolbar)
        self.assertIn("ImGuiCol_Header", theme)
        self.assertIn("ImGuiCol_ChildBg", theme)

    def test_theme_is_applied_once_before_first_frame_and_stays_renderer_neutral(self):
        preferences = self.read("Editor/src/EditorPreferences.cpp")
        theme_h = self.read("Editor/src/EditorTheme.h")
        theme = self.read("Editor/src/EditorTheme.cpp")
        self.assertIn('#include "EditorTheme.h"', preferences)
        self.assertIn("EditorTheme::Apply();", preferences)
        self.assertIn("class EditorTheme final", theme_h)
        self.assertNotIn("Ogre::", theme)
        self.assertNotIn("OpenGL", theme)
        self.assertNotIn("Vulkan", theme)

    def test_m24_remains_font_and_icon_owner(self):
        theme = self.read("Editor/src/EditorTheme.cpp")
        visuals = self.read("Editor/src/EditorVisualAssets.cpp")
        self.assertIn("EditorVisualAssets::TextFont()", theme)
        self.assertIn("ImFont* EditorVisualAssets::TextFont()", visuals)
        self.assertIn("Dear ImGui creates its ProggyClean-derived default font", visuals)
        self.assertNotIn("AddFontFromFileTTF", theme)

    def test_m23_is_wired_documented_and_does_not_claim_settings_scope(self):
        local = self.read("ValidateEditor.bat")
        ci = self.read(".github/workflows/editor-windows.yml")
        docs = self.read("Docs/Editor.md")
        self.assertIn("EditorMilestone23ThemeTests.py", local)
        self.assertIn("EditorMilestone23ThemeTests.py", ci)
        self.assertIn("## 23. Editor Theme / Visual System — `DONE`", docs)
        self.assertIn("Continue this explicitly assigned batch with **26 → 29**", docs)
        self.assertIn("M36 settings/theme-selector implementation is included", docs)
        self.assertIn("Scope boundary: no M26 selection, M29 preview-rendering, or M36", docs)


if __name__ == "__main__":
    unittest.main()
