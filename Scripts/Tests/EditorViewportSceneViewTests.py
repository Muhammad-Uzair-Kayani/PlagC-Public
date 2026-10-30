#!/usr/bin/env python3
"""Source-level regressions for the usable editor Scene View milestone."""

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class EditorViewportSceneViewTests(unittest.TestCase):
    def read(self, relative: str) -> str:
        return (ROOT / relative).read_text(encoding="utf-8")

    def test_viewport_state_exposes_authoring_scene_view_controls(self):
        header = self.read("PlagC/include/PlagC/Editor/EditorModels.h")
        for required in (
            "enum class TransformSpace",
            "TransformSpace transformSpace",
            "bool gridVisible",
            "bool gizmosVisible",
            "bool previewLighting",
            "bool selectionFeedback",
            "float cameraSpeed",
            "float translationSnap",
            "float rotationSnapDegrees",
            "float scaleSnap",
        ):
            self.assertIn(required, header)

    def test_scene_view_overlay_is_submitted_above_target_before_gizmos(self):
        header = self.read("Editor/src/EditorLayer.h")
        app = self.read("Editor/src/EditorApp.cpp")
        overlay = self.read("Editor/src/EditorViewportExperience.cpp")
        self.assertIn("AppendViewportExperience", header)
        presenter = app.index("viewportPresenter_.Draw")
        experience = app.index("layer_.AppendViewportExperience")
        gizmo = app.index("layer_.AppendTransformGizmo")
        self.assertLess(presenter, experience)
        self.assertLess(experience, gizmo)
        for required in (
            '"Translate"', '"Rotate"', '"Scale"', '"World"', '"Local"',
            '"Grid"', '"Gizmos"', '"Wire"', '"Preview Light"', '"Focus"',
            '"Front"', '"Back"', '"Left"', '"Right"', '"Top"', '"Bottom"',
            "DrawGrid", "DrawOrientation", "DrawSelectionFeedback",
        ):
            self.assertIn(required, overlay)

    def test_editor_preview_lights_are_editor_only_and_lifetime_owned(self):
        app = self.read("Editor/src/EditorApp.cpp")
        self.assertIn("CreateEditorPreviewLights", app)
        self.assertIn("UpdateEditorPreviewLights", app)
        self.assertIn("DestroyEditorPreviewLights", app)
        self.assertIn("!session_->Playing() && layer_.Viewport().previewLighting", app)
        self.assertIn("engine.Graphics().CreateLight", app)
        self.assertIn("engine.Graphics().DestroyLight", app)

    def test_camera_navigation_uses_scene_view_speed_and_focus(self):
        app = self.read("Editor/src/EditorApp.cpp")
        overlay = self.read("Editor/src/EditorViewportExperience.cpp")
        self.assertIn("viewport.cameraSpeed", app)
        self.assertIn("ImGuiKey_F", overlay)
        self.assertIn("FocusSelection", overlay)
        self.assertIn("ApplyViewPreset", overlay)

    def test_local_world_gizmo_and_visibility_are_respected(self):
        source = self.read("Editor/src/EditorTransformGizmo.cpp")
        self.assertIn("viewport_.gizmosVisible", source)
        self.assertIn("TransformSpace::Local", source)
        self.assertIn("RotateEuler", source)
        self.assertIn("axes[axis]", source)

    def test_scene_view_updates_roadmap_status(self):
        doc = self.read("Docs/Editor.md")
        self.assertIn("## 1. Viewport / Scene View", doc)
        self.assertIn("# Milestone Execution Log", doc)


if __name__ == "__main__":
    unittest.main()
