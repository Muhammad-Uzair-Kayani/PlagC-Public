#!/usr/bin/env python3
"""Source regressions for Content Browser model authoring workflows."""

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class EditorAssetInteractionTests(unittest.TestCase):
    def read(self, relative: str) -> str:
        return (ROOT / relative).read_text(encoding="utf-8")

    def test_cooked_assets_are_loaded_relative_to_asset_manager_root(self):
        source = self.read("Editor/src/EditorAssetInteractions.cpp")
        self.assertIn('mountedPrefix = "Assets://Cooked/"', source)
        self.assertIn('filesystemPrefix = "Assets/Cooked/"', source)
        self.assertIn('cookedPrefix = "Cooked/"', source)
        self.assertIn("path.starts_with(mountedPrefix)", source)
        self.assertIn("engine.Assets().Load<ModelAsset>(CookedAssetKey(payloadPath))", source)
        self.assertIn("Assets://Cooked", source)

    def test_content_browser_can_create_model_entities(self):
        source = self.read("Editor/src/EditorAssetInteractions.cpp")
        self.assertIn("entry.kind != EditorAssetKind::Cooked", source)
        self.assertIn("ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)", source)
        self.assertIn("entity.model = ModelComponent{model, true}", source)
        self.assertIn("document.Create(std::move(entity))", source)
        self.assertIn("document.Select(created)", source)

    def test_inspector_model_drop_is_undoable(self):
        source = self.read("Editor/src/EditorAssetInteractions.cpp")
        app = self.read("Editor/src/EditorApp.cpp")
        header = self.read("Editor/src/EditorLayer.h")
        self.assertIn('constexpr const char* CookedModelPayload = "PLAGC_COOKED_MODEL"', source)
        self.assertIn("ImGui::SetDragDropPayload(CookedModelPayload", source)
        self.assertIn("ImGui::AcceptDragDropPayload(CookedModelPayload)", source)
        self.assertIn("session.EditDocument().SetModel(selected->entity, ModelComponent{model, true})", source)
        self.assertNotIn("SystemWorld().SetModel", source)
        self.assertIn("void AppendAssetInteractions(Engine&, EditorSession&);", header)
        self.assertIn("layer_.AppendAssetInteractions(engine, *session_)", app)

    def test_no_fake_full_viewport_drop_target_is_advertised(self):
        source = self.read("Editor/src/EditorAssetInteractions.cpp")
        self.assertNotIn('ImGui::Begin("Viewport")', source)
        self.assertNotIn("Drop a cooked ModelAsset into the viewport", source)


if __name__ == "__main__":
    unittest.main()
