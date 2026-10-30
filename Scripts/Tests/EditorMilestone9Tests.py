#!/usr/bin/env python3
"""Milestone 9 regressions: usable Camera Inspector and Scene View integration."""

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class EditorMilestone9Tests(unittest.TestCase):
    def read(self, relative: str) -> str:
        return (ROOT / relative).read_text(encoding="utf-8")

    def setUp(self):
        self.camera = self.read("Editor/src/EditorCameraInspector.cpp")
        self.layer = self.read("Editor/src/EditorLayer.h")
        self.viewport = self.read("Editor/src/EditorViewportExperience.cpp")

    def test_camera_inspector_is_integrated_with_mutable_scene_view_camera(self):
        self.assertIn("void AppendCameraInspector(Engine&, EditorSession&, CameraDesc& editorCamera);", self.layer)
        self.assertIn("AppendCameraInspector(engine,session,editorCamera);", self.viewport)
        self.assertNotIn("Ogre::", self.camera)

    def test_projection_properties_are_real_document_edits(self):
        for label in (
            '"FOV##CameraInspector"',
            '"Near Clip##CameraInspector"',
            '"Far Clip##CameraInspector"',
            '"Aspect Override##CameraInspector"',
        ):
            self.assertIn(label, self.camera)
        self.assertIn("document.SetCamera(selected->entity,camera)", self.camera)
        self.assertGreaterEqual(self.camera.count("ImGui::IsItemDeactivatedAfterEdit()"), 4)

    def test_active_camera_controls_are_command_backed(self):
        self.assertIn('"Make Active Camera##CameraInspector"', self.camera)
        self.assertIn('"Clear Active Camera##CameraInspector"', self.camera)
        self.assertIn("document.SetActiveCamera(selected->entity)", self.camera)
        self.assertIn("document.ClearActiveCamera()", self.camera)

    def test_selected_camera_draws_a_real_frustum_overlay(self):
        self.assertIn("void DrawFrustum", self.camera)
        self.assertIn("const auto nearP=plane(nearD),farP=plane(farD);", self.camera)
        self.assertIn("camera.fov", self.camera)
        self.assertIn("camera.farClip", self.camera)
        self.assertIn("draw->AddLine", self.camera)
        self.assertIn("DrawFrustum(viewportWindow->DrawList", self.camera)

    def test_camera_preview_uses_existing_backend_neutral_scene_view(self):
        self.assertIn('"Preview Through Camera"', self.camera)
        self.assertIn('"Exit Camera Preview"', self.camera)
        self.assertIn("cameraPreviewRestore_=editorCamera", self.camera)
        self.assertIn("editorCamera=AuthoredCamera(*selected)", self.camera)
        self.assertIn("engine.Graphics().SetCameraProjection(CameraProjection::Perspective)", self.camera)
        self.assertIn("engine.Graphics().SetCamera(editorCamera)", self.camera)

    def test_bidirectional_alignment_is_exposed(self):
        self.assertIn('"Align Camera to View"', self.camera)
        self.assertIn("transform.position=editorCamera.position", self.camera)
        self.assertIn("aligned.target=editorCamera.target", self.camera)
        self.assertIn("document.SetTransform(selected->entity,transform)", self.camera)
        self.assertIn("document.SetCamera(selected->entity,aligned)", self.camera)
        self.assertIn('"Align View to Camera"', self.camera)
        self.assertIn("editorCamera=AuthoredCamera(*selected)", self.camera)


if __name__ == "__main__":
    unittest.main()
