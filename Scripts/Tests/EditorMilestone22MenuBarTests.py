#!/usr/bin/env python3
"""Focused source regressions for Milestone 22 Menu Bar."""

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class EditorMilestone22MenuBarTests(unittest.TestCase):
    def read(self, relative: str) -> str:
        return (ROOT / relative).read_text(encoding="utf-8")

    def test_complete_menu_surface_is_composed_from_existing_owners(self):
        menu = self.read("Editor/src/EditorMenuController.cpp")
        layer = self.read("Editor/src/EditorLayer.cpp")
        files = self.read("Editor/src/EditorFileController.cpp")
        header = self.read("Editor/src/EditorLayer.h")
        self.assertIn('const char* menuLabel = session.Dirty() ? "File *" : "File"', files)
        self.assertIn("ImGui::BeginMenu(menuLabel)", files)
        self.assertIn('BeginMenu("Edit")', layer)
        self.assertIn('BeginMenu("View")', layer)
        for name in ("Entity", "Assets", "Window", "Debug", "Help"):
            self.assertIn(f'BeginMenu("{name}")', menu)
        self.assertIn("EditorFileController::DrawMenu(engine, session)", menu)
        self.assertIn("EditorMenuController files_;", header)

    def test_entity_menu_reuses_document_commands_and_lock_policy(self):
        menu = self.read("Editor/src/EditorMenuController.cpp")
        self.assertIn("session.EditingEnabled()", menu)
        self.assertIn("document.HasTransientEdit()", menu)
        self.assertIn("owner_.hierarchy_.Locked", menu)
        self.assertIn('"Create Empty"', menu)
        self.assertIn('"Duplicate Selected", "Ctrl+D"', menu)
        self.assertIn('"Delete Selected", "Delete"', menu)
        self.assertIn('"Clear Selection", "Esc"', menu)
        self.assertIn("session.EditDocument().Create()", menu)
        self.assertIn("session.EditDocument().Duplicate(selected)", menu)
        self.assertIn("session.EditDocument().Destroy(selected)", menu)

    def test_assets_window_and_debug_menus_route_existing_state(self):
        menu = self.read("Editor/src/EditorMenuController.cpp")
        self.assertIn('"Refresh Asset Browser"', menu)
        self.assertIn("owner_.dataPanels_.RefreshAssets(engine)", menu)
        self.assertIn('"Import / Reimport..."', menu)
        self.assertIn("owner_.showAssetImport_", menu)
        for field in (
            "showHierarchy_", "showInspector_", "showContentBrowser_",
            "showConsole_", "showStatistics_", "showAnimationStateMachine_"
        ):
            self.assertIn(f"owner_.{field}", menu)
        self.assertIn("owner_.viewport_.gridVisible", menu)
        self.assertIn("owner_.viewport_.gizmosVisible", menu)
        self.assertIn("owner_.viewport_.previewLighting", menu)
        self.assertIn("owner_.physicsDebugVisible_", menu)
        self.assertIn("owner_.physicsDebugAll_", menu)

    def test_shortcuts_are_context_safe_and_match_advertised_menu_labels(self):
        files = self.read("Editor/src/EditorFileController.cpp")
        layer = self.read("Editor/src/EditorLayer.cpp")
        menu = self.read("Editor/src/EditorMenuController.cpp")
        self.assertIn("ImGuiKey_N", files)
        self.assertIn("ImGuiKey_O", files)
        self.assertIn("ImGuiKey_S", files)
        self.assertIn("io.WantTextInput", files)
        self.assertIn("ImGuiKey_Z", layer)
        self.assertIn("ImGuiKey_Y", layer)
        self.assertIn("ImGuiKey_D", layer)
        self.assertIn("ImGuiKey_Delete", layer)
        self.assertIn("session.CanUndo()", layer)
        self.assertIn("session.CanRedo()", layer)
        self.assertIn("ImGuiKey_Escape", menu)
        self.assertIn("clearSelectionBlocked", menu)
        self.assertIn("io.WantTextInput", menu)
        self.assertIn("ImGui::IsAnyItemActive()", menu)
        self.assertIn("ImGui::IsPopupOpen(nullptr", menu)
        self.assertIn("session.EditDocument().Select({})", menu)

    def test_help_menu_has_editor_specific_discovery_without_external_dependency(self):
        menu = self.read("Editor/src/EditorMenuController.cpp")
        self.assertIn('"PlagC Editor"', menu)
        self.assertIn('"Copy Editor Documentation Path"', menu)
        self.assertIn('ImGui::SetClipboardText("Docs/Editor.md")', menu)
        self.assertIn('"Edit: Ctrl+Z / Ctrl+Y / Ctrl+D / Delete / Esc"', menu)
        self.assertIn('"Play: F5 / F6 / F7"', menu)

    def test_m22_is_wired(self):
        local = self.read("ValidateEditor.bat")
        ci = self.read(".github/workflows/editor-windows.yml")
        docs = self.read("Docs/Editor.md")
        self.assertIn("EditorMilestone22MenuBarTests.py", local)
        self.assertIn("EditorMilestone22MenuBarTests.py", ci)
        self.assertIn("## 22. Menu Bar — `DONE`", docs)


if __name__ == "__main__":
    unittest.main()
