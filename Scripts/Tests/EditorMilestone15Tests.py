#!/usr/bin/env python3
"""Milestone 15 regressions: source-to-cooked model/texture import and reimport."""

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class EditorMilestone15Tests(unittest.TestCase):
    def read(self, relative: str) -> str:
        return (ROOT / relative).read_text(encoding="utf-8")

    def setUp(self):
        self.asset_h = self.read("PlagC/include/PlagC/Assets/Asset.h")
        self.importer = self.read("PlagC/src/Assets/AssetImport.cpp")
        self.receipt_h = self.read("PlagC/include/PlagC/Assets/AssetImport.h")
        self.receipt = self.read("PlagC/src/Assets/AssetImportReceipt.cpp")
        self.io = self.read("PlagC/src/Assets/AssetIO.cpp")
        self.editor = self.read("Editor/src/EditorAssetImport.cpp")
        self.integration = self.read("Editor/src/EditorCameraInspector.cpp")

    def test_model_import_options_drive_real_asset_processing(self):
        self.assertIn("struct ModelImportOptions", self.asset_h)
        self.assertIn("ModelCoordinateImport", self.asset_h)
        self.assertIn("ModelNormalImport", self.asset_h)
        self.assertIn("ModelAsset ImportModel(const std::filesystem::path &, const ModelImportOptions &)", self.asset_h)
        self.assertIn("ScaleModel(asset, options.scale)", self.importer)
        self.assertIn("MirrorZ(asset)", self.importer)
        self.assertIn("RecalculateSmoothNormals(asset)", self.importer)
        self.assertIn("if (!options.tangents)", self.importer)
        self.assertIn("if (!options.materials)", self.importer)
        self.assertIn("if (!options.animations)", self.importer)
        self.assertIn("vertex.v = 1.0f - vertex.v", self.importer)

    def test_textures_are_first_class_cooked_assets(self):
        self.assertIn("Texture = 3", self.asset_h)
        self.assertIn("struct TextureAsset final : Asset", self.asset_h)
        self.assertIn("TextureAsset ImportTexture", self.asset_h)
        self.assertIn('extension != ".png"', self.importer)
        self.assertIn("stbi_load", self.importer)
        self.assertIn("SaveAsset(const TextureAsset", self.io)
        self.assertIn("type == AssetType::Texture", self.io)
        self.assertIn("std::make_shared<TextureAsset>()", self.io)

    def test_import_receipt_persists_source_cooked_relationship_and_settings(self):
        self.assertIn("struct AssetImportReceipt", self.receipt_h)
        self.assertIn("std::filesystem::path source", self.receipt_h)
        self.assertIn("std::filesystem::path cooked", self.receipt_h)
        self.assertIn('"PGIMPORT "', self.receipt)
        self.assertIn('"SOURCE "', self.receipt)
        self.assertIn('"COOKED "', self.receipt)
        self.assertIn('"MODEL "', self.receipt)
        self.assertIn('"TEXTURE "', self.receipt)
        self.assertIn("SaveAssetImportReceipt(receipt)", self.receipt)

    def test_cook_and_reimport_use_real_import_and_asset_io(self):
        self.assertIn("void CookImportedAsset", self.receipt)
        self.assertIn("ImportModel(receipt.source, receipt.model)", self.receipt)
        self.assertIn("ImportTexture(receipt.source, receipt.texture)", self.receipt)
        self.assertIn("SaveAsset(asset, receipt.cooked)", self.receipt)
        self.assertIn("LoadAssetImportReceipt(assetImportCooked_.data())", self.editor)
        self.assertIn('ImGui::Button("Reimport Saved")', self.editor)

    def test_editor_exposes_requested_model_and_texture_import_settings(self):
        for label in (
            '"Scale"', '"Coordinates"', '"Flip UV V"', '"Normals"',
            '"Tangents"', '"Materials + Textures"', '"Animations"',
            '"Flip Vertical"', '"Import / Cook"', '"Load Saved Settings"'
        ):
            self.assertIn(label, self.editor)
        self.assertIn("Supported model sources: glTF 2.0", self.editor)
        self.assertIn("Supported texture sources: PNG, JPEG", self.editor)

    def test_import_errors_are_visible_and_logged(self):
        self.assertIn("assetImportError_ = error.what()", self.editor)
        self.assertIn('"EDITOR-IMPORT"', self.editor)
        self.assertIn('"Import failed: %s"', self.editor)
        self.assertIn("ImGuiCol_Text", self.editor)

    def test_import_workspace_is_reached_from_normal_editor_frame(self):
        self.assertIn("AppendAssetImport(engine, session);", self.integration)
        self.assertNotIn("Ogre::", self.editor)


if __name__ == "__main__":
    unittest.main()
