#!/usr/bin/env python3
"""Source-level regressions for editor Milestone 21 Editor Toolbar."""

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class EditorMilestone21ToolbarTests(unittest.TestCase):
    def read(self, relative: str) -> str:
        return (ROOT / relative).read_text(encoding="utf-8")

    def test_global_toolbar_is_between_menu_and_dockspace(self):
        layer = self.read("Editor/src/EditorLayer.cpp")
        header = self.read("Editor/src/EditorLayer.h")
        self.assertIn("void DrawEditorToolbar(Engine&, EditorSession&);", header)
        self.assertIn("DrawEditorToolbar(engine, session);", layer)
        menu_end = layer.index("ImGui::EndMenuBar();")
        toolbar = layer.index("DrawEditorToolbar(engine, session);")
        dock = layer.index("ImGui::DockSpace(dockspaceID")
        self.assertLess(menu_end, toolbar)
        self.assertLess(toolbar, dock)

    def test_toolbar_composes_existing_editor_actions(self):
        toolbar = self.read("Editor/src/EditorToolbar.cpp")
        file_header = self.read("Editor/src/EditorFileController.h")
        self.assertIn('"ToolbarSave"', toolbar)
        self.assertIn("files_.Save(engine, session)", toolbar)
        self.assertIn("bool Save(Engine&, EditorSession&);", file_header)
        self.assertIn("session.CanUndo()", toolbar)
        self.assertIn("session.Undo()", toolbar)
        self.assertIn("session.CanRedo()", toolbar)
        self.assertIn("session.Redo()", toolbar)
        self.assertIn("TransportAction::PlayStop", toolbar)
        self.assertIn("TransportAction::PauseResume", toolbar)
        self.assertIn("TransportAction::Step", toolbar)
        self.assertIn("EditorPreferences::ToggleActive()", toolbar)

    def test_transform_tools_space_pivot_and_snap_share_viewport_state(self):
        toolbar = self.read("Editor/src/EditorToolbar.cpp")
        models = self.read("PlagC/include/PlagC/Editor/EditorModels.h")
        self.assertIn("viewport_.gizmo = GizmoMode::Translate", toolbar)
        self.assertIn("viewport_.gizmo = GizmoMode::Rotate", toolbar)
        self.assertIn("viewport_.gizmo = GizmoMode::Scale", toolbar)
        self.assertIn("viewport_.transformSpace", toolbar)
        self.assertIn("enum class TransformPivot { Pivot, Center };", models)
        self.assertIn("TransformPivot pivot = TransformPivot::Pivot;", models)
        self.assertIn("viewport_.pivot", toolbar)
        self.assertIn("viewport_.snapEnabled = !viewport_.snapEnabled", toolbar)

    def test_play_mode_locks_authoring_toolbar_commands(self):
        toolbar = self.read("Editor/src/EditorToolbar.cpp")
        session = self.read("PlagC/src/Editor/EditorSession.cpp")
        self.assertIn("const bool authoring = session.EditingEnabled();", toolbar)
        self.assertIn("return !play_.Running() && !document_->HasTransientEdit();", session)
        self.assertIn('"Editor authoring operation is disabled during Play"', session)

    def test_icon_presentation_remains_layered_over_toolbar_actions(self):
        toolbar = self.read("Editor/src/EditorToolbar.cpp")
        self.assertIn("EditorVisualAssets::IconButton", toolbar)
        self.assertIn("EditorIcon::Save", toolbar)
        self.assertIn("EditorIcon::Translate", toolbar)
        self.assertIn("EditorIcon::Settings", toolbar)

    def test_validation_wiring(self):
        validate = self.read("ValidateEditor.bat")
        workflow = self.read(".github/workflows/editor-windows.yml")
        self.assertIn("EditorMilestone21ToolbarTests.py", validate)
        self.assertIn("EditorMilestone21ToolbarTests.py", workflow)


if __name__ == "__main__":
    unittest.main()
