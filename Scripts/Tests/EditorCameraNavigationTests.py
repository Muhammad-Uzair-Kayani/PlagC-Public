#!/usr/bin/env python3
"""Regression coverage for editor viewport camera input routing."""

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class EditorCameraNavigationTests(unittest.TestCase):
    def test_keyboard_navigation_is_independent_from_rmb_look(self):
        app = (ROOT / "Editor/src/EditorApp.cpp").read_text(encoding="utf-8")

        self.assertIn("const bool cameraMouse=viewport.input.AcceptCameraMouse()&&!blocked;", app)
        self.assertIn(
            "const bool cameraKeyboard=viewport.input.AcceptKeyboard()&&!blocked&&!io.KeyCtrl&&!io.KeyAlt&&!io.KeySuper;",
            app,
        )
        self.assertIn(
            "if(cameraMouse&&ImGui::IsMouseDown(ImGuiMouseButton_Right))",
            app,
        )
        self.assertIn("if(cameraKeyboard)", app)
        for key in ("ImGuiKey_W", "ImGuiKey_A", "ImGuiKey_S", "ImGuiKey_D", "ImGuiKey_Q", "ImGuiKey_E"):
            self.assertIn(key, app)
        self.assertNotIn(
            "AcceptCameraMouse() && viewport.input.AcceptKeyboard()",
            app,
        )

    def test_left_mouse_remains_selection_input(self):
        app = (ROOT / "Editor/src/EditorApp.cpp").read_text(encoding="utf-8")
        self.assertIn("ImGui::IsMouseClicked(ImGuiMouseButton_Left, false)", app)
        self.assertIn("document.SelectPersistent(hit->persistentID)", app)
        self.assertIn("document.Select({})", app)


if __name__ == "__main__":
    unittest.main()
