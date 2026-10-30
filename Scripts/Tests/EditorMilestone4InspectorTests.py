#!/usr/bin/env python3
"""Source-level regressions for Milestone 4: usable Inspector framework."""

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class EditorMilestone4InspectorTests(unittest.TestCase):
    def read(self, relative: str) -> str:
        return (ROOT / relative).read_text(encoding="utf-8")

    def test_base_inspector_replaces_component_yes_no_debug_summary(self):
        layer = self.read("Editor/src/EditorLayer.cpp")
        self.assertIn('InputTextWithHint("##EntityName"', layer)
        self.assertIn("PLAY / READ ONLY", layer)
        self.assertIn("Hierarchy lock prevents Inspector mutations.", layer)
        self.assertIn("Runtime world is isolated from the authoring document.", layer)
        self.assertNotIn('ImGui::Text("Camera: %s"', layer)
        self.assertNotIn('ImGui::Text("Light: %s"', layer)
        self.assertNotIn('ImGui::Text("Model: %s"', layer)
        self.assertNotIn('ImGui::Text("Physics: %s"', layer)
        self.assertNotIn('ImGui::Text("Animator: %s"', layer)

    def test_component_authoring_uses_cards_and_aligned_property_rows(self):
        source = self.read("Editor/src/EditorComponentInspector.cpp")
        self.assertIn("BeginComponentCard", source)
        self.assertIn("BeginProperties", source)
        self.assertIn("ImGuiTableFlags_BordersInnerV", source)
        for name in ("CameraCard", "LightCard", "ModelCard", "PhysicsCard", "AnimatorCard"):
            self.assertIn(name, source)
        self.assertIn("+ Add Component", source)
        self.assertIn("ComponentActions", source)
        self.assertIn('ImGui::MenuItem("Reset")', source)
        self.assertIn('ImGui::MenuItem("Remove Component")', source)

    def test_validation_and_help_are_part_of_inspector_contract(self):
        source = self.read("Editor/src/EditorComponentInspector.cpp")
        for validator in ("ValidateCamera", "ValidateLight", "ValidatePhysics", "ValidateAnimator"):
            self.assertIn(validator, source)
        self.assertIn("HelpMarker", source)
        self.assertIn("ImGui::SetTooltip", source)
        self.assertIn("ValidationMessage", source)
        self.assertIn('"EDITOR-INSPECTOR"', source)

    def test_runtime_and_locked_entities_are_read_only(self):
        source = self.read("Editor/src/EditorComponentInspector.cpp")
        self.assertIn("const bool editable = session.EditingEnabled()", source)
        self.assertIn("const bool hierarchyLocked = hierarchy_.Locked(selected->persistentID)", source)
        self.assertIn("const bool readOnly = !editable || hierarchyLocked", source)
        self.assertIn("Play mode: component authoring is read-only.", source)
        self.assertIn("This entity is locked in the Hierarchy.", source)
        self.assertIn("ImGui::BeginDisabled(readOnly)", source)

    def test_mutations_remain_scene_document_command_backed(self):
        source = self.read("Editor/src/EditorComponentInspector.cpp")
        for call in (
            "session.EditDocument().SetCamera",
            "session.EditDocument().RemoveCamera",
            "session.EditDocument().SetLight",
            "session.EditDocument().RemoveLight",
            "session.EditDocument().SetModel",
            "session.EditDocument().RemoveModel",
            "session.EditDocument().SetPhysics",
            "session.EditDocument().RemovePhysics",
            "session.EditDocument().SetAnimator",
            "session.EditDocument().RemoveAnimator",
            "session.EditDocument().SetModelVisible",
            "session.EditDocument().SetModelCastShadows",
        ):
            self.assertIn(call, source)
        self.assertNotIn("engine.Graphics().UpdateLight", source)
        self.assertNotIn("engine.Graphics().SetTransform", source)

    def test_milestone_boundary_is_documented(self):
        doc = self.read("Docs/Editor.md")
        self.assertIn("## 4. Inspector — `DONE`", doc)
        self.assertIn("## 4. Inspector — `DONE`", doc[doc.index("# Milestone Execution Log"):])
        self.assertIn("Scope boundary", doc)
        self.assertIn("Milestone 5", doc)
        self.assertIn("Milestone 7", doc)
        self.assertIn("Milestone 10", doc)

    def test_validation_entrypoints_run_milestone4_gate(self):
        validate = self.read("ValidateEditor.bat")
        workflow = self.read(".github/workflows/editor-windows.yml")
        self.assertIn("EditorMilestone4InspectorTests.py", validate)
        self.assertIn("EditorMilestone4InspectorTests.py", workflow)


if __name__ == "__main__":
    unittest.main()
