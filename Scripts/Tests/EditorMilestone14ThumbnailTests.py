#!/usr/bin/env python3
"""Focused source regressions for Milestone 14 Thumbnail / Preview System."""

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class EditorMilestone14ThumbnailTests(unittest.TestCase):
    def read(self, relative: str) -> str:
        return (ROOT / relative).read_text(encoding="utf-8")

    def test_thumbnail_cache_is_renderer_neutral_and_job_backed(self):
        header = self.read("Editor/src/EditorThumbnailCache.h")
        source = self.read("Editor/src/EditorThumbnailCache.cpp")
        self.assertIn("class EditorThumbnailCache", header)
        self.assertIn("ThumbnailState::Queued", source)
        self.assertIn("ThumbnailState::Generating", source)
        self.assertIn("engine.Jobs().TrySubmit", source)
        self.assertIn("MaxConcurrentThumbnailJobs = 2", source)
        self.assertIn("MaxCachedThumbnails = 256", source)
        self.assertIn("group.pending.load(std::memory_order_acquire)", source)
        self.assertNotIn("#include <Ogre", source)
        self.assertNotIn("#include <Ogre", header)
        self.assertNotIn("Ogre::", source)
        self.assertNotIn("Ogre::", header)

    def test_fixed_studio_camera_and_lighting_drive_model_and_material_previews(self):
        source = self.read("Editor/src/EditorThumbnailCache.cpp")
        self.assertIn("PreviewCamera", source)
        self.assertIn("StudioCamera", source)
        self.assertIn("StudioLights", source)
        self.assertIn("RenderModelPreview", source)
        self.assertIn("RenderMaterialPreview", source)
        self.assertIn("RenderTexturePreview", source)
        self.assertIn("TransformPoint", source)
        self.assertIn("TransformNormal", source)
        self.assertIn("Shade(material.colour", source)

    def test_worker_results_upload_only_as_ui_textures_and_are_generation_safe(self):
        source = self.read("Editor/src/EditorThumbnailCache.cpp")
        self.assertIn("Fingerprint", source)
        self.assertIn("last_write_time", source)
        self.assertIn("task->serial!=entry.serial", source)
        self.assertIn("task->fingerprint!=entry.fingerprint", source)
        self.assertIn("CreateUiTexture", source)
        self.assertIn("DestroyUiTexture", source)
        self.assertIn("InvalidateAll", source)
        self.assertIn("engine.Jobs().Wait", source)

    def test_content_browser_exposes_selected_preview_strip_and_placeholders(self):
        header = self.read("Editor/src/EditorDataPanels.h")
        panel = self.read("Editor/src/EditorThumbnailPanel.cpp")
        layer = self.read("Editor/src/EditorLayer.h")
        self.assertIn("EditorPreviewDataPanels", header)
        self.assertIn("EditorPreviewDataPanels dataPanels_", layer)
        self.assertIn('SeparatorText("Thumbnail / Preview")', panel)
        self.assertIn('"Regenerate Preview"', panel)
        self.assertIn('"Regenerate All"', panel)
        self.assertIn('"Preview cache strip"', panel)
        self.assertIn("DrawPlaceholder", panel)
        self.assertIn("ImGuiRendererBridge::TextureID", panel)
        self.assertIn("PreviewStateLabel", panel)

    def test_previewable_types_are_model_material_texture_only(self):
        panel = self.read("Editor/src/EditorThumbnailPanel.cpp")
        self.assertIn("AssetType::Model", panel)
        self.assertIn("AssetType::Texture", panel)
        self.assertIn('extension==".pgmaterial"', panel)
        self.assertNotIn("ThumbnailKind::Audio", panel)
        self.assertNotIn("ThumbnailKind::Scene", panel)

    def test_m14_is_wired_and_preserves_later_scope_boundaries(self):
        local = self.read("ValidateEditor.bat")
        ci = self.read(".github/workflows/editor-windows.yml")
        docs = self.read("Docs/Editor.md")
        self.assertIn("EditorMilestone14ThumbnailTests.py", local)
        self.assertIn("EditorMilestone14ThumbnailTests.py", ci)
        self.assertIn("## 14. Thumbnail / Preview System — `DONE`", docs)
        self.assertIn("## 16. Scene File Workflow — `DONE`", docs)
        self.assertIn("## 29. Editor Preview Rendering — `NOT STARTED`", docs)


if __name__ == "__main__":
    unittest.main()
