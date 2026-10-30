#!/usr/bin/env python3
"""Milestone 5 regressions: professional Transform Inspector."""

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class EditorMilestone5Tests(unittest.TestCase):
    def read(self, relative: str) -> str:
        return (ROOT / relative).read_text(encoding="utf-8")

    def test_transform_inspector_owns_complete_milestone_scope(self):
        source = self.read("Editor/src/EditorTransformInspector.cpp")
        for required in (
            'ImGui::SeparatorText("Transform")',
            'ImGui::RadioButton("Local"',
            'ImGui::RadioButton("World"',
            'ImGui::Checkbox("Uniform Scale"',
            'ImGui::Button("Reset Transform")',
            'drawRow("Position"',
            'drawRow("Rotation"',
            'drawRow("Scale"',
            'ImGui::TextColored(AxisColour(axis)',
        ):
            self.assertIn(required, source)

    def test_local_and_world_spaces_use_existing_scene_document_contracts(self):
        source = self.read("Editor/src/EditorTransformInspector.cpp")
        self.assertIn("document.LocalTransform(selected.entity)", source)
        self.assertIn("selected.record.transform", source)
        self.assertIn("ComposeTransform", source)
        self.assertIn("document.SetLocalTransform(selected.entity, next)", source)
        self.assertIn("document.SetTransform(selected.entity, next)", source)

    def test_drag_preview_is_one_transaction_and_immediate(self):
        source = self.read("Editor/src/EditorTransformInspector.cpp")
        document_test = self.read("Scripts/Tests/EditorDocumentTests.cpp")
        self.assertIn("BeginTransformEdit", source)
        self.assertIn("edit_->Preview(world)", source)
        self.assertIn("Finish(engine, true)", source)
        self.assertIn("One continuous gizmo/inspector drag produces one history entry.", document_test)
        self.assertIn("edit.Preview(preview)", document_test)
        self.assertIn("edit.Commit()", document_test)
        self.assertIn("assert(document.Undo())", document_test)

    def test_uniform_scaling_preserves_axis_ratios(self):
        source = self.read("Editor/src/EditorTransformInspector.cpp")
        self.assertIn("const float ratio = component / beforeAxis", source)
        self.assertIn("Axis(vector, other) * ratio", source)
        self.assertIn("std::max(0.001f", source)

    def test_transform_reset_is_single_command_not_three_axis_commands(self):
        source = self.read("Editor/src/EditorTransformInspector.cpp")
        self.assertIn("ApplyImmediate(engine, session, selected, Transform{})", source)
        self.assertIn("next.*member = reset", source)
        self.assertIn("document.SetLocalTransform", source)
        self.assertIn("document.SetTransform", source)

    def test_lock_and_dynamic_physics_are_respected(self):
        source = self.read("Editor/src/EditorTransformInspector.cpp")
        self.assertIn("hierarchyLocked", source)
        self.assertIn("MotionType::Dynamic", source)
        self.assertIn("const bool readOnly = hierarchyLocked || physicsOwned", source)

    def test_editor_layer_replaces_legacy_dragfloat3_block(self):
        layer = self.read("Editor/src/EditorLayer.cpp")
        header = self.read("Editor/src/EditorLayer.h")
        self.assertIn("transformInspector_.Draw(engine, session, *selected", layer)
        self.assertIn("transformInspector_.Finish(engine, true)", layer)
        self.assertIn("EditorTransformInspector transformInspector_", header)
        self.assertNotIn('ImGui::DragFloat3("Position"', layer)
        self.assertNotIn('ImGui::DragFloat3("Rotation"', layer)
        self.assertNotIn('ImGui::DragFloat3("Scale"', layer)

    def test_validation_and_roadmap_are_updated(self):
        local = self.read("ValidateEditor.bat")
        workflow = self.read(".github/workflows/editor-windows.yml")
        docs = self.read("Docs/Editor.md")
        self.assertIn("EditorMilestone5Tests.py", local)
        self.assertIn("EditorMilestone5Tests.py", workflow)
        self.assertIn("## 5. Transform Inspector — `DONE`", docs)
        self.assertIn("## Milestone 5 — Transform Inspector — `DONE`", docs)


if __name__ == "__main__":
    unittest.main()
