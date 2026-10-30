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

    def test_content_browser_exposes_read_only_snapshot_for_authoring(self):
        source = self.read("Editor/src/EditorAssetInteractions.cpp")
        panels = self.read("Editor/src/EditorDataPanels.h")
        self.assertIn("ContentSnapshot() const", panels)
        self.assertIn("return assets_.Snapshot();", panels)
        self.assertIn("const auto content = dataPanels_.ContentSnapshot();", source)
        self.assertIn("for (const auto& entry : *content)", source)
        self.assertNotIn("entry.directory", source)

    def test_content_browser_can_create_model_entities(self):
        source = self.read("Editor/src/EditorAssetInteractions.cpp")
        self.assertIn("entry.kind != EditorAssetKind::Cooked", source)
        self.assertIn("ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)", source)
        self.assertIn("CreateCookedModelEntity(engine, session, entry.path.c_str(), Vec3{})", source)
        self.assertIn("entity.model = ModelComponent{model, true}", source)
        self.assertIn("session.EditDocument().Create(std::move(entity))", source)

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

    def test_viewport_drop_targets_real_rendered_surface(self):
        source = self.read("Editor/src/EditorViewportAssetDrop.cpp")
        app = self.read("Editor/src/EditorApp.cpp")
        header = self.read("Editor/src/EditorLayer.h")
        self.assertIn('ImGui::Begin("Viewport")', source)
        self.assertIn("ImGui::GetCurrentWindow()", source)
        self.assertIn('window->GetID("ViewportSurface")', source)
        self.assertIn("ImGui::BeginDragDropTargetCustom(rect,surface)", source)
        self.assertIn('ImGui::AcceptDragDropPayload(CookedModelPayload)', source)
        self.assertIn("DropPosition(editorCamera,viewport_,rect,ImGui::GetIO().MousePos)", source)
        self.assertIn("CreateCookedModelEntity(engine,session", source)
        self.assertIn("void AppendViewportAssetDrop(Engine&, EditorSession&, const CameraDesc& editorCamera);", header)
        self.assertIn("viewportPresenter_.Draw(viewportTarget_->Handle());", app)
        self.assertIn("layer_.AppendViewportAssetDrop(engine, *session_, editorCamera_);", app)
        self.assertLess(app.index("viewportPresenter_.Draw(viewportTarget_->Handle());"),
                        app.index("layer_.AppendViewportAssetDrop(engine, *session_, editorCamera_);"))

    def test_viewport_drop_keeps_viewport_current_for_custom_target(self):
        source = self.read("Editor/src/EditorViewportAssetDrop.cpp")
        self.assertLess(source.index('ImGui::Begin("Viewport")'),
                        source.index("ImGui::BeginDragDropTargetCustom(rect,surface)"))
        self.assertIn("ImGui::End();", source)

    def test_viewport_drop_uses_ground_plane_with_forward_ray_fallback(self):
        source = self.read("Editor/src/EditorViewportAssetDrop.cpp")
        self.assertIn("const float t=-origin.y/direction.y", source)
        self.assertIn("return Add(origin,Mul(direction,t))", source)
        self.assertIn("fallbackDistance=Length(Sub(camera.target,camera.position))", source)
        self.assertIn("return Add(origin,Mul(direction,fallbackDistance))", source)

    def test_orthographic_drop_offsets_origin_instead_of_ray_direction(self):
        source = self.read("Editor/src/EditorViewportAssetDrop.cpp")
        self.assertIn("if(viewport.orthographic)", source)
        self.assertIn("viewport.orthographicHeight", source)
        self.assertIn("origin=Add(origin,Mul(right", source)
        self.assertIn("origin=Add(origin,Mul(up", source)


if __name__ == "__main__":
    unittest.main()
