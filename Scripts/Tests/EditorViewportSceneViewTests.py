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
            '"Local"', '"World"',
            '"Move Snap"', '"Rotate Snap"', '"Scale Snap"',
            '"Perspective"', '"Orthographic"',
            '"Front"', '"Back"', '"Left"', '"Right"', '"Top"', '"Bottom"',
            '"Focus"', '"Frame All"', '"Camera Speed"',
            '"Grid"', '"Gizmos"', '"Wireframe"', '"Preview Light"', '"Exposure"',
        ):
            self.assertIn(token, source)

    def test_scene_view_draws_adaptive_grid_axes_and_orientation_indicator(self):
        source = self.read("Editor/src/EditorViewportExperience.cpp")
        self.assertIn("DrawAdaptiveGrid", source)
        self.assertIn("DrawWorldAxes", source)
        self.assertIn("DrawOrientationIndicator", source)
        self.assertIn("NiceGridStep", source)
        self.assertIn("ProjectPoint", source)
        self.assertIn("IM_COL32", source)

    def test_scene_view_selection_feedback_uses_persistent_selection(self):
        source = self.read("Editor/src/EditorViewportExperience.cpp")
        self.assertIn("entity.selected", source)
        self.assertIn("DrawSelectionBounds", source)
        self.assertIn("document.PersistentID", source)

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
        ogre = self.read("PlagC/src/Renderer/OgreRenderer.cpp")
        app = self.read("Editor/src/EditorApp.cpp")
        self.assertIn("bool orthographic = false", viewport)
        self.assertIn("float orthographicHeight", viewport)
        self.assertIn("CameraProjection", renderer)
        self.assertIn("SetCameraProjection", renderer)
        self.assertIn("Ogre::PT_ORTHOGRAPHIC", ogre)
        self.assertIn("setOrthoWindowHeight", ogre)
        self.assertIn("viewport.orthographic ? PlagC::CameraProjection::Orthographic", app)

    def test_focus_and_frame_all_use_scene_bounds(self):
        source = self.read("Editor/src/EditorViewportExperience.cpp")
        self.assertIn("FocusSelected", source)
        self.assertIn("FrameAll", source)
        self.assertIn("BoundsForEntity", source)
        self.assertIn("CombinedBounds", source)

    def test_editor_preview_lighting_and_environment_are_play_isolated(self):
        app = self.read("Editor/src/EditorApp.cpp")
        self.assertIn("PlagCEnvironment.dds", app)
        self.assertIn("!session_->Playing() && layer_.Viewport().previewLighting", app)
        self.assertIn("RestoreEditorPreviewEnvironment", app)
        self.assertIn("editorPreviewLights_", app)

    def test_wireframe_and_exposure_are_real_renderer_settings(self):
        source = self.read("Editor/src/EditorViewportExperience.cpp")
        ogre = self.read("PlagC/src/Renderer/OgreRenderer.cpp")
        self.assertIn("settings.wireframe=viewport_.wireframe", source)
        self.assertIn("settings.exposure=viewport_.exposure", source)
        self.assertIn("SetSettings(settings)", source)
        self.assertIn("Ogre::SCENE_WIREFRAME", ogre)
        self.assertIn("mToneMapParams.x=p.settings.exposure", ogre)

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
