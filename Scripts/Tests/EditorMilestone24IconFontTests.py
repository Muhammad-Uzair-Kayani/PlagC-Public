#!/usr/bin/env python3
"""Source-level regressions for editor Milestone 24 Icons / Fonts."""

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class EditorMilestone24IconFontTests(unittest.TestCase):
    def read(self, relative: str) -> str:
        return (ROOT / relative).read_text(encoding="utf-8")

    def test_vector_icon_catalog_covers_required_editor_domains(self):
        header = self.read("Editor/src/EditorVisualAssets.h")
        for icon in (
            "Entity", "Folder", "Asset", "Model", "Texture", "Material",
            "Camera", "Light", "Physics", "Save", "Undo", "Redo",
            "Translate", "Rotate", "Scale", "Play", "Pause", "Step",
            "Stop", "Settings",
        ):
            self.assertIn(icon, header)
        self.assertIn("static void DrawIcon", header)
        self.assertIn("static bool IconButton", header)

    def test_icons_are_renderer_neutral_draw_list_primitives(self):
        source = self.read("Editor/src/EditorVisualAssets.cpp")
        self.assertIn("ImDrawList", source)
        self.assertIn("AddLine", source)
        self.assertIn("AddRect", source)
        self.assertIn("AddCircle", source)
        self.assertIn("AddTriangleFilled", source)
        self.assertNotIn("Ogre::", source)
        self.assertNotIn("glBindTexture", source)
        self.assertNotIn("vkCmd", source)

    def test_text_font_is_embedded_imgui_default_without_raw_font_files(self):
        header = self.read("Editor/src/EditorVisualAssets.h")
        source = self.read("Editor/src/EditorVisualAssets.cpp")
        self.assertIn("ImFont* TextFont() noexcept", header)
        self.assertIn("ImGui::GetFont()", source)
        self.assertIn("built-in embedded default font", header)
        lowered = (header + source).lower()
        self.assertNotIn(".ttf", lowered)
        self.assertNotIn(".otf", lowered)
        self.assertNotIn("addfontfromfile", lowered)

    def test_global_toolbar_uses_icons_for_authoring_and_transport(self):
        toolbar = self.read("Editor/src/EditorToolbar.cpp")
        for icon in (
            "EditorIcon::Save", "EditorIcon::Undo", "EditorIcon::Redo",
            "EditorIcon::Translate", "EditorIcon::Rotate", "EditorIcon::Scale",
            "EditorIcon::Snap", "EditorIcon::Play", "EditorIcon::Pause",
            "EditorIcon::Step", "EditorIcon::Stop", "EditorIcon::Settings",
        ):
            self.assertIn(icon, toolbar)
        self.assertIn("EditorVisualAssets::IconButton", toolbar)

    def test_accessibility_names_exist_for_non_toolbar_icon_categories(self):
        source = self.read("Editor/src/EditorVisualAssets.cpp")
        for name in ("Entity", "Folder", "Asset", "Camera", "Light", "Physics"):
            self.assertIn(f'return "{name}";', source)

    def test_validation_wiring(self):
        validate = self.read("ValidateEditor.bat")
        workflow = self.read(".github/workflows/editor-windows.yml")
        self.assertIn("EditorMilestone24IconFontTests.py", validate)
        self.assertIn("EditorMilestone24IconFontTests.py", workflow)


if __name__ == "__main__":
    unittest.main()
