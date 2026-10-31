#!/usr/bin/env python3
"""Focused source regressions for Milestone 25 Docking / Workspace."""

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class EditorMilestone25WorkspaceTests(unittest.TestCase):
    def read(self, relative: str) -> str:
        return (ROOT / relative).read_text(encoding="utf-8")

    def test_default_layout_remains_strong_and_panel_specific(self):
        layer = self.read("Editor/src/EditorLayer.cpp")
        self.assertIn("DockBuilderSplitNode(main, ImGuiDir_Left, 0.18f", layer)
        self.assertIn("DockBuilderSplitNode(main, ImGuiDir_Right, 0.24f", layer)
        self.assertIn("DockBuilderSplitNode(main, ImGuiDir_Down, 0.28f", layer)
        for panel in ("Hierarchy", "Inspector", "Statistics", "Content Browser", "Console", "Viewport"):
            self.assertIn(f'DockBuilderDockWindow("{panel}"', layer)

    def test_workspace_persists_imgui_geometry_and_plagc_panel_visibility(self):
        workspace = self.read("Editor/src/EditorWorkspace.cpp")
        header = self.read("Editor/src/EditorWorkspace.h")
        layer_header = self.read("Editor/src/EditorLayer.h")
        self.assertIn("SaveIniSettingsToDisk", workspace)
        self.assertIn("LoadIniSettingsFromDisk", workspace)
        self.assertIn('directory_{"User/Editor"}', header)
        self.assertIn('layoutPath_{directory_ / "Workspace.ini"}', header)
        self.assertIn('statePath_{directory_ / "Workspace.state"}', header)
        for field in (
            "showHierarchy_", "showInspector_", "showContentBrowser_",
            "showConsole_", "showStatistics_", "showAnimationStateMachine_", "showAssetImport_"
        ):
            self.assertIn(f"owner.{field}", workspace)
        self.assertIn("EditorWorkspace workspace_;", layer_header)

    def test_save_restore_reset_are_explicit_window_menu_actions(self):
        menu = self.read("Editor/src/EditorMenuController.cpp")
        self.assertIn('BeginMenu("Workspace")', menu)
        self.assertIn('"Save Workspace"', menu)
        self.assertIn("owner_.workspace_.Save(owner_)", menu)
        self.assertIn('"Restore Saved Workspace"', menu)
        self.assertIn("owner_.workspace_.RequestRestore()", menu)
        self.assertIn('"Reset to Default Layout"', menu)
        self.assertIn("owner_.workspace_.RequestReset()", menu)
        self.assertIn("owner_.workspace_.HasSavedWorkspace()", menu)

    def test_reset_rebuilds_default_layout_and_restore_uses_saved_docking(self):
        workspace = self.read("Editor/src/EditorWorkspace.cpp")
        self.assertIn("ImGui::DockBuilderRemoveNode(dockspaceID)", workspace)
        self.assertIn("ImGui::ClearIniSettings()", workspace)
        self.assertIn("owner.BuildDefaultLayout(", workspace)
        self.assertIn("SetDefaultPanels(owner)", workspace)
        self.assertIn("LoadPanelState(owner)", workspace)
        self.assertIn("startupRestore_ = true", workspace)

    def test_first_frame_legacy_imgui_ini_cannot_override_plagc_workspace(self):
        workspace = self.read("Editor/src/EditorWorkspace.cpp")
        self.assertIn("const bool hasSavedWorkspace = HasSavedWorkspace()", workspace)
        self.assertIn("ImGui::GetIO().IniFilename = nullptr", workspace)
        self.assertIn("if (hasSavedWorkspace)", workspace)
        self.assertIn("ImGui::ClearIniSettings()", workspace)
        self.assertIn("pending_ = Pending::Reset", workspace)
        self.assertIn("ApplyPending(owner)", workspace)
        # A stale panel-state sidecar must not be loaded unless its matching layout exists.
        saved_branch = workspace.index("if (hasSavedWorkspace)")
        load_panel = workspace.index("LoadPanelState(owner)", saved_branch)
        reset_branch = workspace.index("pending_ = Pending::Reset", saved_branch)
        self.assertLess(load_panel, reset_branch)

    def test_detached_windows_remain_supported_and_local_state_is_not_tracked(self):
        layer = self.read("Editor/src/EditorLayer.cpp")
        ignore = self.read(".gitignore")
        self.assertIn("ImGuiWindowFlags_NoDocking", layer)  # host only
        self.assertNotIn('ImGui::Begin("Hierarchy", nullptr, ImGuiWindowFlags_NoDocking', layer)
        self.assertIn("/User/", ignore)

    def test_m25_is_wired(self):
        local = self.read("ValidateEditor.bat")
        ci = self.read(".github/workflows/editor-windows.yml")
        docs = self.read("Docs/Editor.md")
        self.assertIn("EditorMilestone25WorkspaceTests.py", local)
        self.assertIn("EditorMilestone25WorkspaceTests.py", ci)
        self.assertIn("## 25. Docking / Workspace — `DONE`", docs)


if __name__ == "__main__":
    unittest.main()
