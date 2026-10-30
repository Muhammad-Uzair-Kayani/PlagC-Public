#!/usr/bin/env python3
"""Source regressions for viewport transform gizmo behavior."""

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class EditorGizmoTests(unittest.TestCase):
    def read(self, relative: str) -> str:
        return (ROOT / relative).read_text(encoding="utf-8")

    def test_gizmo_is_drawn_after_viewport_image(self):
        app = self.read("Editor/src/EditorApp.cpp")
        presenter = app.index("viewportPresenter_.Draw(viewportTarget_->Handle());")
        gizmo = app.index("layer_.AppendTransformGizmo(engine, *session_, editorCamera_);")
        self.assertLess(presenter, gizmo)

    def test_gizmo_uses_world_projection_and_axis_hit_testing(self):
        source = self.read("Editor/src/EditorTransformGizmo.cpp")
        self.assertIn("ProjectedPoint Project", source)
        self.assertIn("DistanceToSegment", source)
        self.assertIn("Cross(forward,{0,1,0})", source)
        self.assertIn("axisWorldLength", source)
        self.assertIn("window->WorkRect", source)

    def test_translate_rotate_scale_and_snap_use_viewport_contract(self):
        source = self.read("Editor/src/EditorTransformGizmo.cpp")
        for required in (
            "GizmoMode::Translate",
            "GizmoMode::Rotate",
            "GizmoMode::Scale",
            "ImGuiKey_W",
            "ImGuiKey_E",
            "ImGuiKey_R",
            "viewport_.translationSnap",
            "viewport_.rotationSnapDegrees",
            "viewport_.scaleSnap",
        ):
            self.assertIn(required, source)

    def test_drag_is_one_scene_document_transform_transaction(self):
        header = self.read("Editor/src/EditorLayer.h")
        source = self.read("Editor/src/EditorTransformGizmo.cpp")
        self.assertIn("std::unique_ptr<SceneDocument::TransformEdit> gizmoEdit_", header)
        self.assertIn("document.BeginTransformEdit(selected->entity)", source)
        self.assertIn("gizmoEdit_->Preview(preview)", source)
        self.assertIn("gizmoEdit_->Commit()", source)
        self.assertIn("gizmoEdit_->Cancel()", source)
        self.assertIn("ImGuiKey_Escape", source)
        self.assertIn("ImGui::IsMouseReleased(ImGuiMouseButton_Left)", source)

    def test_gizmo_blocks_viewport_camera_and_uses_local_capture_policy(self):
        source = self.read("Editor/src/EditorTransformGizmo.cpp")
        models = self.read("PlagC/include/PlagC/Editor/EditorModels.h")
        self.assertIn("viewport_.input.gizmoActive", source)
        self.assertIn("!viewport_.input.uiBlocksMouse", source)
        self.assertNotIn("!io.WantCaptureMouse", source)
        self.assertIn("!gizmoActive", models)

    def test_active_gizmo_survives_its_own_transient_edit(self):
        source = self.read("Editor/src/EditorTransformGizmo.cpp")
        self.assertIn("session.Playing() || (!gizmoEdit_ && !session.EditingEnabled())", source)


if __name__ == "__main__":
    unittest.main()
