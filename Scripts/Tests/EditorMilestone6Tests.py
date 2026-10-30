#!/usr/bin/env python3
"""Milestone 6 regressions: usable Model / Mesh Inspector."""

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class EditorMilestone6Tests(unittest.TestCase):
    def read(self, relative: str) -> str:
        return (ROOT / relative).read_text(encoding="utf-8")

    def setUp(self):
        self.ui = self.read("Editor/src/EditorAssetInteractions.cpp")
        self.world = self.read("PlagC/include/PlagC/World/World.h")
        self.document = self.read("PlagC/src/Editor/SceneDocumentModel.cpp")
        self.asset_scene = self.read("PlagC/src/Assets/AssetScene.cpp")
        self.browser = self.read("Editor/src/EditorDataPanels.cpp")

    def test_model_component_has_renderer_neutral_shadow_state(self):
        self.assertIn("struct ModelComponent", self.world)
        self.assertIn("bool castShadows = true;", self.world)
        self.assertIn("SetModelCastShadows", self.document)
        self.assertIn('"Edit model shadows"', self.document)

    def test_renderer_applies_model_shadow_state_to_all_submesh_objects(self):
        self.assertIn("shadowStateApplied", self.asset_scene)
        self.assertIn("p.renderer.SetCastShadows(h,model.castShadows)", self.asset_scene)

    def test_inspector_exposes_asset_replace_visibility_shadow_reload_and_locate(self):
        for text in (
            '"Model / Mesh Inspector"',
            '"Model Asset"',
            '"Browse / Replace..."',
            '"Visible##ModelMeshInspector"',
            '"Cast Shadows"',
            '"Reload"',
            '"Locate in Browser"',
        ):
            self.assertIn(text, self.ui)
        self.assertIn("document.SetModelVisible", self.ui)
        self.assertIn("document.SetModelCastShadows", self.ui)
        self.assertIn("panels.RevealAsset", self.ui)

    def test_mesh_material_and_bounds_data_come_from_cooked_asset(self):
        self.assertIn('"Mesh / Submesh Information"', self.ui)
        self.assertIn('"Material Slots"', self.ui)
        self.assertIn('"Bounds"', self.ui)
        self.assertIn("mesh.geometry.vertices", self.ui)
        self.assertIn("mesh.geometry.indices", self.ui)
        self.assertIn("mesh.material < asset.materials.size()", self.ui)
        self.assertIn("vertex.position", self.ui)
        self.assertIn("Imported slots are read-only here", self.ui)

    def test_reload_rebinds_new_immutable_asset_generation_through_document(self):
        self.assertIn("engine.Assets().Reload(path);", self.ui)
        self.assertIn("engine.Assets().Wait();", self.ui)
        self.assertIn("engine.Assets().Get<ModelAsset>(path)", self.ui)
        self.assertIn("document.ReloadModelAsset", self.ui)
        self.assertIn('"Reload model asset"', self.document)
        self.assertIn("before.asset->metadata.path != asset->metadata.path", self.document)

    def test_replacement_preserves_instance_flags_and_uses_document_command(self):
        self.assertIn("result.visible = selected.record.model->visible", self.ui)
        self.assertIn("result.castShadows = selected.record.model->castShadows", self.ui)
        self.assertIn("session.EditDocument().SetModel", self.ui)
        self.assertNotIn("SystemWorld().SetModel", self.ui)

    def test_locate_in_browser_is_a_real_reveal_action(self):
        self.assertIn("void EditorDataPanels::RevealAsset", self.browser)
        self.assertIn("ImGui::TableSetBgColor", self.browser)
        self.assertIn("ImGui::SetScrollHereY(0.5f)", self.browser)


if __name__ == "__main__":
    unittest.main()
