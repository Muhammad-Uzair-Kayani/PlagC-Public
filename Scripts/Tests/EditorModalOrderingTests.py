#!/usr/bin/env python3
"""Regressions for editor popup/modal draw ordering."""

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class EditorModalOrderingTests(unittest.TestCase):
    def read(self, relative: str) -> str:
        return (ROOT / relative).read_text(encoding="utf-8")

    def test_modals_are_submitted_after_late_viewport_passes(self):
        app = self.read("Editor/src/EditorApp.cpp")
        viewport = app.index("viewportPresenter_.Draw(viewportTarget_->Handle());")
        gizmo = app.index("layer_.AppendTransformGizmo(engine, *session_, editorCamera_);")
        preferences = app.index("preferences_.Draw(engine, *session_);")
        file_modals = app.index("layer_.DrawModals(engine, *session_);")
        close_modal = app.index("DrawClosePrompt(engine);")
        render = app.index("ImGui::Render();")
        self.assertLess(viewport, gizmo)
        self.assertLess(gizmo, preferences)
        self.assertLess(preferences, file_modals)
        self.assertLess(file_modals, close_modal)
        self.assertLess(close_modal, render)

    def test_unsaved_modal_is_not_drawn_inside_dock_host(self):
        source = self.read("Editor/src/EditorLayer.cpp")
        header = self.read("Editor/src/EditorLayer.h")
        self.assertIn("void DrawModals(Engine&, EditorSession&);", header)
        self.assertIn("void EditorLayer::DrawModals", source)
        self.assertEqual(source.count("files_.DrawUnsavedModal(engine, session);"), 1)
        modal = source.index("files_.DrawUnsavedModal(engine, session);")
        dock = source.index("void EditorLayer::DrawDockHost")
        self.assertLess(modal, dock)

    def test_window_close_modal_is_centered_when_appearing(self):
        app = self.read("Editor/src/EditorApp.cpp")
        self.assertIn("ImGui::GetMainViewport()->GetCenter()", app)
        self.assertIn("ImGuiCond_Appearing", app)


if __name__ == "__main__":
    unittest.main()
