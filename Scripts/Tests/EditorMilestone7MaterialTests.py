#!/usr/bin/env python3
"""Milestone 7 regressions: standalone PBS materials and slot authoring."""

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class EditorMilestone7MaterialTests(unittest.TestCase):
    def read(self, relative: str) -> str:
        return (ROOT / relative).read_text(encoding="utf-8")

    def setUp(self):
        self.asset = self.read("PlagC/include/PlagC/Assets/Asset.h")
        self.material_io = self.read("PlagC/src/Assets/MaterialAsset.cpp")
        self.world = self.read("PlagC/include/PlagC/World/World.h")
        self.document_h = self.read("PlagC/include/PlagC/Editor/SceneDocument.h")
        self.document_model = self.read("PlagC/src/Editor/SceneDocumentModel.cpp")
        self.scene = self.read("PlagC/src/Editor/SceneSerialization.cpp")
        self.asset_scene = self.read("PlagC/src/Assets/AssetScene.cpp")
        self.renderer_h = self.read("PlagC/include/PlagC/Renderer/Renderer.h")
        self.ogre = self.read("PlagC/src/Renderer/OgreRenderer.cpp")
        self.inspector = self.read("Editor/src/EditorMaterialInspector.cpp")
        self.layer = self.read("Editor/src/EditorLayer.cpp")
        self.docs = self.read("Docs/Editor.md")
        self.validate = self.read("ValidateEditor.bat")
        self.workflow = self.read(".github/workflows/editor-windows.yml")

    def test_standalone_material_asset_covers_full_pbs_surface(self):
        for token in (
            "std::string assetPath;",
            "baseColourTexturePath",
            "normalTexturePath",
            "metallicTexturePath",
            "roughnessTexturePath",
            "occlusionTexturePath",
            "emissiveTexturePath",
            "float alphaCutoff",
            "bool doubleSided",
            "SaveMaterialAsset",
            "LoadMaterialAsset",
        ):
            self.assertIn(token, self.asset)
        self.assertIn('MaterialMagic[] = "PGCMAT01"', self.material_io)
        self.assertIn("MaterialVersion = 1", self.material_io)
        self.assertIn("MoveFileExW", self.material_io)
        self.assertIn("Validate(const MaterialAsset&", self.material_io)

    def test_model_slots_and_document_commands_are_renderer_neutral_and_undoable(self):
        self.assertIn("std::vector<AssetHandle<MaterialAsset>> materialOverrides", self.world)
        self.assertIn("SetModelMaterial", self.document_h)
        self.assertIn("ClearModelMaterial", self.document_h)
        self.assertIn('"Assign material"', self.document_model)
        self.assertIn('"Clear material"', self.document_model)
        self.assertIn("history_.Execute", self.document_model)
        self.assertNotIn("Ogre::", self.document_model)

    def test_asset_scene_builds_real_pbs_overrides_with_correct_colour_space(self):
        for token in (
            "overrideTextures",
            "overrideMaterials",
            "overrideAssets",
            "OverridesMatch(model)",
            "d.colour = handle->colour",
            "d.metallic = handle->metallic",
            "d.roughness = handle->roughness",
            "d.normalScale = handle->normalScale",
            "d.alphaCutoff = handle->alphaCutoff",
            "d.doubleSided = handle->doubleSided",
            "overrideMaterials[slot]",
        ):
            self.assertIn(token, self.asset_scene)
        self.assertIn("loadTexture(handle->baseColourTexturePath, true)", self.asset_scene)
        self.assertIn("loadTexture(handle->emissiveTexturePath, true)", self.asset_scene)
        self.assertIn("loadTexture(handle->normalTexturePath, false)", self.asset_scene)
        self.assertIn("loadTexture(handle->metallicTexturePath, false)", self.asset_scene)
        self.assertIn("loadTexture(handle->roughnessTexturePath, false)", self.asset_scene)
        self.assertIn("loadTexture(handle->occlusionTexturePath, false)", self.asset_scene)

    def test_renderer_contract_and_ogre_pbs_support_alpha_and_double_sided(self):
        for token in (
            "baseColourTexture",
            "normalTexture",
            "metallicTexture",
            "roughnessTexture",
            "occlusionTexture",
            "emissiveTexture",
            "alphaCutoff",
            "doubleSided",
        ):
            self.assertIn(token, self.renderer_h)
        self.assertIn("setAlphaTest", self.ogre)
        self.assertIn("setTwoSidedLighting", self.ogre)

    def test_material_assignments_remain_persistent_in_current_scene_format(self):
        self.assertIn("CurrentSceneVersion = 4", self.scene)
        self.assertIn("if(version<1 || version>CurrentSceneVersion)", self.scene)
        self.assertIn("if(version<3)", self.scene)
        self.assertIn('w.Key("materials")', self.scene)
        self.assertIn("materialOverrides", self.scene)
        self.assertIn("LoadMaterialAsset", self.scene)
        self.assertIn("r.castShadows", self.scene)

    def test_material_inspector_supports_slots_full_pbs_save_reload_clear_and_live_preview(self):
        for token in (
            '"Material System"',
            '"Material Slot"',
            '"Base Colour"',
            '"Base Texture"',
            '"Normal Map"',
            '"Metallic"',
            '"Roughness"',
            '"AO Map"',
            '"Emissive"',
            '"Alpha Cutoff"',
            '"Double Sided"',
            '"Save Material"',
            '"Reload"',
            '"Clear Override"',
            "SetModelMaterial",
            "ClearModelMaterial",
            "SaveMaterialAsset",
            "LoadMaterialAsset",
            "Live viewport preview active",
            "Colour maps: sRGB. Normal / metallic / roughness / AO maps: linear.",
        ):
            self.assertIn(token, self.inspector)
        self.assertIn("DrawMaterialInspector(engine, session)", self.layer)
        self.assertNotIn("engine.Graphics().CreateMaterial", self.inspector)
        self.assertNotIn("Ogre::", self.inspector)

    def test_scope_boundary_and_validation_wiring(self):
        self.assertIn("## 7. Material System + Inspector — `DONE`", self.docs)
        self.assertIn("## 10. Physics Inspector — `DONE`", self.docs)
        self.assertIn("EditorMilestone7MaterialTests.py", self.validate)
        self.assertIn("EditorMilestone7MaterialTests.py", self.workflow)
        self.assertIn("EditorMilestone8Tests.py", self.validate)
        self.assertIn("EditorMilestone8Tests.py", self.workflow)
        self.assertFalse((ROOT / "_temp_should_not_exist").exists())
        self.assertFalse((ROOT / "_another_temp").exists())


if __name__ == "__main__":
    unittest.main()
