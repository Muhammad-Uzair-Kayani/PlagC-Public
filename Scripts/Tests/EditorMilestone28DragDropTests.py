#!/usr/bin/env python3
"""Focused source regressions for Milestone 28 Drag & Drop."""

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class EditorMilestone28DragDropTests(unittest.TestCase):
    def read(self, relative: str) -> str:
        return (ROOT / relative).read_text(encoding="utf-8")

    def test_shared_frontend_payload_contract_validates_text_entity_ids_and_feedback(self):
        header = self.read("Editor/src/EditorDragDrop.h")
        source = self.read("Editor/src/EditorDragDrop.cpp")
        self.assertIn('CookedModel = "PLAGC_COOKED_MODEL"', header)
        self.assertIn('MaterialAsset = "PLAGC_MATERIAL_ASSET"', header)
        self.assertIn('Entity = "PLAGC_EDITOR_ENTITY"', header)
        self.assertIn("TextPayload", source)
        self.assertIn('throw std::invalid_argument("drag payload is empty")', source)
        self.assertIn("drag payload text is not null terminated", source)
        self.assertIn("drag payload contains embedded null bytes", source)
        self.assertIn("EntityIDPayload", source)
        self.assertIn("entity drag payload has an invalid size", source)
        self.assertIn("entity drag payload contains an invalid persistent id", source)
        self.assertIn("DrawInlineFeedback(bool accepted", source)
        self.assertIn("DrawRectFeedback(ImVec2 minimum, ImVec2 maximum,\n                      bool accepted", source)
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

    def test_model_asset_to_inspector_uses_shared_parser_validation_and_feedback(self):
        inspector = self.read("Editor/src/EditorAssetInteractions.cpp")
        self.assertIn('#include "EditorDragDrop.h"', inspector)
        self.assertIn('"Model Asset Drop Target"', inspector)
        self.assertIn('"Drop cooked ModelAsset here##ModelAssetDrop"', inspector)
        self.assertIn("ImGui::AcceptDragDropPayload(DragDrop::CookedModel)", inspector)
        self.assertIn("DragDrop::TextPayload(*payload)", inspector)
        self.assertIn("DragDrop::DrawRectFeedback", inspector)
        self.assertIn("LoadModel(engine", inspector)
        self.assertIn("CookedAssetKey", inspector)
        self.assertIn("ReplacementModel", inspector)
        self.assertIn("session.EditDocument().SetModel", inspector)
        self.assertIn("Only cooked assets under Assets://Cooked", inspector)

    def test_hierarchy_reparent_and_root_drop_use_shared_entity_validation_and_feedback(self):
        hierarchy = self.read("Editor/src/EditorSceneHierarchy.cpp")
        self.assertIn('#include "EditorDragDrop.h"', hierarchy)
        self.assertIn("ImGui::SetDragDropPayload(DragDrop::Entity, &id, sizeof(id))", hierarchy)
        self.assertIn('"Scene Root  (drop here)"', hierarchy)
        self.assertGreaterEqual(hierarchy.count("ImGui::AcceptDragDropPayload(DragDrop::Entity)"), 2)
        self.assertGreaterEqual(hierarchy.count("DragDrop::EntityIDPayload"), 4)
        self.assertIn("WouldCreateCycle", hierarchy)
        self.assertIn("dragged != item->persistentID", hierarchy)
        self.assertIn("!Locked(dragged)", hierarchy)
        self.assertIn("DragDrop::DrawRectFeedback", hierarchy)
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

    def test_model_preview_to_animator_uses_shared_parser_feedback_and_deep_validation(self):
        preview = self.read("Editor/src/EditorThumbnailPanel.cpp")
        animator = self.read("Editor/src/EditorAnimatorInspector.cpp")
        self.assertIn("payloadType = DragDrop::CookedModel", preview)
        self.assertIn('#include "EditorDragDrop.h"', animator)
        self.assertIn('"Drop cooked animation ModelAsset here##AnimatorSourceDrop"', animator)
        self.assertIn("ImGui::AcceptDragDropPayload(DragDrop::CookedModel)", animator)
        self.assertIn("DragDrop::TextPayload(*payload)", animator)
        self.assertIn("DragDrop::DrawRectFeedback", animator)
        self.assertIn("LoadAnimationModel", animator)
        self.assertIn("source->animations.empty()", animator)
        self.assertIn("SkeletonCompatible", animator)
        self.assertIn("graph.Validate(static_cast<unsigned>(source->animations.size()))", animator)
        self.assertIn("session.EditDocument().SetAnimator", animator)

    def test_m28_is_wired_without_freezing_downstream_milestone_status(self):
        local = self.read("ValidateEditor.bat")
        ci = self.read(".github/workflows/editor-windows.yml")
        docs = self.read("Docs/Editor.md")
        self.assertIn("EditorMilestone28DragDropTests.py", local)
        self.assertIn("EditorMilestone28DragDropTests.py", ci)
        self.assertIn("## 28. Drag & Drop — `DONE`", docs)


if __name__ == "__main__":
    unittest.main()
