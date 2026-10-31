#!/usr/bin/env python3
"""Focused source regressions for Milestone 28 Drag & Drop."""

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class EditorMilestone28DragDropTests(unittest.TestCase):
    def read(self, relative: str) -> str:
        return (ROOT / relative).read_text(encoding="utf-8")

    def test_shared_frontend_payload_contract_validates_text_and_feedback(self):
        header = self.read("Editor/src/EditorDragDrop.h")
        source = self.read("Editor/src/EditorDragDrop.cpp")
        self.assertIn('CookedModel = "PLAGC_COOKED_MODEL"', header)
        self.assertIn('MaterialAsset = "PLAGC_MATERIAL_ASSET"', header)
        self.assertIn('Entity = "PLAGC_EDITOR_ENTITY"', header)
        self.assertIn("TextPayload", source)
        self.assertIn('throw std::invalid_argument("drag payload is empty")', source)
        self.assertIn("drag payload text is not null terminated", source)
        self.assertIn("drag payload contains embedded null bytes", source)
        self.assertIn("DrawInlineFeedback", source)
        self.assertIn("DrawRectFeedback", source)
        self.assertIn('accepted ? "[DROP]" : "[NO]"', source)

    def test_model_asset_to_viewport_is_validated_and_visibly_targeted(self):
        viewport = self.read("Editor/src/EditorViewportAssetDrop.cpp")
        self.assertIn('#include "EditorDragDrop.h"', viewport)
        self.assertIn("DragDrop::DrawRectFeedback", viewport)
        self.assertIn("session.EditingEnabled()?DragDrop::CookedModel:nullptr", viewport)
        self.assertIn("ImGui::AcceptDragDropPayload(DragDrop::CookedModel)", viewport)
        self.assertIn("DragDrop::TextPayload(*payload)", viewport)
        self.assertIn("CreateCookedModelEntity", viewport)
        self.assertIn("DropPosition", viewport)

    def test_model_asset_to_inspector_reuses_existing_model_validation(self):
        inspector = self.read("Editor/src/EditorAssetInteractions.cpp")
        self.assertIn('"Model Asset Drop Target"', inspector)
        self.assertIn("ImGui::AcceptDragDropPayload(CookedModelPayload)", inspector)
        self.assertIn("LoadModel(engine", inspector)
        self.assertIn("CookedAssetKey", inspector)
        self.assertIn("ReplacementModel", inspector)
        self.assertIn("session.EditDocument().SetModel", inspector)
        self.assertIn("Only cooked assets under Assets://Cooked", inspector)

    def test_hierarchy_reparent_and_root_drop_validate_entity_payload(self):
        hierarchy = self.read("Editor/src/EditorSceneHierarchy.cpp")
        self.assertIn('EntityPayload = "PLAGC_EDITOR_ENTITY"', hierarchy)
        self.assertIn("ImGui::SetDragDropPayload(EntityPayload, &id, sizeof(id))", hierarchy)
        self.assertIn('"Scene Root  (drop here)"', hierarchy)
        self.assertGreaterEqual(hierarchy.count("ImGui::AcceptDragDropPayload(EntityPayload)"), 2)
        self.assertIn("payload->DataSize == sizeof(std::uint64_t)", hierarchy)
        self.assertIn("dragged != item->persistentID", hierarchy)
        self.assertIn("!Locked(dragged)", hierarchy)
        self.assertIn("PendingKind::Reparent", hierarchy)
        self.assertIn("document.SetParent", hierarchy)
        self.assertIn("document.ClearParent", hierarchy)

    def test_material_preview_is_a_real_source_and_material_slot_is_a_validated_target(self):
        preview = self.read("Editor/src/EditorThumbnailPanel.cpp")
        material = self.read("Editor/src/EditorMaterialInspector.cpp")
        self.assertIn("DrawPreviewDragSource", preview)
        self.assertIn("payloadType = DragDrop::MaterialAsset", preview)
        self.assertIn("ImGui::SetDragDropPayload(payloadType", preview)
        self.assertIn('"Drop .pgmaterial onto selected slot##MaterialSlotDrop"', material)
        self.assertIn("ImGui::AcceptDragDropPayload(DragDrop::MaterialAsset)", material)
        self.assertIn("DragDrop::TextPayload(*payload)", material)
        self.assertIn("LogicalMaterialPath(path)", material)
        self.assertIn("LoadMaterialAsset(path)", material)
        self.assertIn("ValidateTextureFiles(*material)", material)
        self.assertIn("session.EditDocument().SetModelMaterial", material)
        self.assertIn('"[NO] Material slot is read-only."', material)
        self.assertIn("DragDrop::DrawInlineFeedback", material)

    def test_model_preview_to_animator_preserves_clip_skeleton_and_controller_validation(self):
        preview = self.read("Editor/src/EditorThumbnailPanel.cpp")
        animator = self.read("Editor/src/EditorAnimatorInspector.cpp")
        self.assertIn("payloadType = DragDrop::CookedModel", preview)
        self.assertIn("ImGui::AcceptDragDropPayload(CookedModelPayload)", animator)
        self.assertIn("LoadAnimationModel", animator)
        self.assertIn("source->animations.empty()", animator)
        self.assertIn("SkeletonCompatible", animator)
        self.assertIn("graph.Validate(static_cast<unsigned>(source->animations.size()))", animator)
        self.assertIn("session.EditDocument().SetAnimator", animator)

    def test_m28_is_wired_and_scope_stops_before_preview_rendering(self):
        local = self.read("ValidateEditor.bat")
        ci = self.read(".github/workflows/editor-windows.yml")
        docs = self.read("Docs/Editor.md")
        self.assertIn("EditorMilestone28DragDropTests.py", local)
        self.assertIn("EditorMilestone28DragDropTests.py", ci)
        self.assertIn("## 28. Drag & Drop — `DONE`", docs)
        self.assertIn("## 29. Editor Preview Rendering — `NOT STARTED`", docs)


if __name__ == "__main__":
    unittest.main()
