#!/usr/bin/env python3
"""Focused source regressions for Milestone 1 Viewport / Scene View."""

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class EditorViewportSceneViewTests(unittest.TestCase):
    def read(self, relative: str) -> str:
        return (ROOT / relative).read_text(encoding="utf-8")

    def test_viewport_toolbar_exposes_scene_authoring_controls(self):
        source = self.read("Editor/src/EditorViewportExperience.cpp")
        for token in (
            '"Translate"', '"Rotate"', '"Scale"',
            '"Local"', '"World"', '"Snap"',
            '"Perspective"', '"Orthographic"',
            '"Front"', '"Back"', '"Left"', '"Right"', '"Top"', '"Bottom"',
            '"Focus"', '"Frame All"', '"Speed"',
            '"Grid"', '"Gizmos"', '"Wire"', '"Preview Light"', '"Exposure"',
            '"Move"', '"Angle"', '"Scale Snap"',
        ):
            self.assertIn(token, source)

    def test_scene_view_draws_adaptive_grid_axes_and_orientation_indicator(self):
        source = self.read("Editor/src/EditorViewportExperience.cpp")
        self.assertIn("AdaptiveGridSpacing", source)
        self.assertIn("DrawGrid", source)
        self.assertIn("DrawOrientation", source)
        self.assertIn("camera.target.x/spacing", source)
        self.assertIn("IM_COL32(210,70,70,160)", source)
        self.assertIn("IM_COL32(70,120,225,160)", source)

    def test_scene_view_selection_feedback_uses_document_selection_snapshot(self):
        source = self.read("Editor/src/EditorViewportExperience.cpp")
        self.assertIn("session.ViewDocument().Snapshot()", source)
        self.assertIn("e.selected", source)
        self.assertIn("DrawSelectionFeedback", source)
        self.assertIn("state.selectionFeedback", source)
        self.assertIn("selected->record.name.c_str()", source)

    def test_viewport_picking_and_empty_space_deselection_are_renderer_backed(self):
        app = self.read("Editor/src/EditorApp.cpp")
        picking = self.read("PlagC/src/Editor/ScenePicking.cpp")
        self.assertIn("RendererPickingProvider", app)
        self.assertIn("picker.Pick(request)", app)
        self.assertIn("document.SelectPersistent(hit->persistentID)", app)
        self.assertIn("document.Select({})", app)
        self.assertIn("sceneRenderer.EntityForObject", picking)
        self.assertIn("assetScene.EntityForObject", picking)

    def test_scene_view_exposes_perspective_and_real_orthographic_projection(self):
        viewport = self.read("PlagC/include/PlagC/Editor/EditorModels.h")
        renderer = self.read("PlagC/include/PlagC/Renderer/Renderer.h")
        projection = self.read("PlagC/src/Renderer/OgreCameraProjection.cpp")
        app = self.read("Editor/src/EditorApp.cpp")
        self.assertIn("bool orthographic = false", viewport)
        self.assertIn("float orthographicHeight", viewport)
        self.assertIn("CameraProjection", renderer)
        self.assertIn("SetCameraProjection", renderer)
        self.assertIn("Ogre::PT_ORTHOGRAPHIC", projection)
        self.assertIn("setOrthoWindowHeight", projection)
        self.assertIn("viewport.orthographic ? PlagC::CameraProjection::Orthographic", app)

    def test_focus_and_frame_all_use_current_scene_entities(self):
        source = self.read("Editor/src/EditorViewportExperience.cpp")
        self.assertIn("FocusSelection", source)
        self.assertIn("FrameAll", source)
        self.assertIn("selected->record.transform.position", source)
        self.assertIn("entities.front().record.transform.position", source)
        self.assertIn("camera.target=P(center)", source)

    def test_editor_preview_lighting_and_environment_are_play_isolated(self):
        app = self.read("Editor/src/EditorApp.cpp")
        self.assertIn("PlagCEnvironment.dds", app)
        self.assertIn("!session_->Playing() && layer_.Viewport().previewLighting", app)
        self.assertIn("RestoreEditorPreviewEnvironment", app)
        self.assertIn("editorPreviewLights_", app)

    def test_wireframe_and_exposure_are_real_renderer_settings(self):
        source = self.read("Editor/src/EditorViewportExperience.cpp")
        ogre = self.read("PlagC/src/Renderer/OgreRenderer.cpp")
        self.assertIn("settings.wireframe=!settings.wireframe", source)
        self.assertIn('ImGui::DragFloat("Exposure",&settings.exposure', source)
        self.assertIn("engine.Graphics().SetSettings(settings)", source)
        self.assertIn("Ogre::PM_WIREFRAME", ogre)
        self.assertIn('"exposure", settings.exposure', ogre)

    def test_orthographic_asset_drop_uses_parallel_cursor_ray(self):
        source = self.read("Editor/src/EditorViewportAssetDrop.cpp")
        self.assertIn("viewport.orthographic", source)
        self.assertIn("viewport.orthographicHeight", source)
        self.assertIn("DropPosition(editorCamera,viewport_", source)

    def test_scene_view_roadmap_is_done_and_has_execution_evidence(self):
        doc = self.read("Docs/Editor.md")
        self.assertIn("## 1. Viewport / Scene View — `DONE`", doc)
        self.assertIn("## 1. Viewport / Scene View — `DONE`", doc[doc.index("# Milestone Execution Log"):])
        self.assertIn("EditorViewportSceneViewTests.py", doc)
        self.assertIn("editor-only preview lighting/environment", doc)


if __name__ == "__main__":
    unittest.main()
