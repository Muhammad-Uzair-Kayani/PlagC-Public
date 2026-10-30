#!/usr/bin/env python3
"""Milestone 8 regressions: complete Lighting Inspector authoring workflow."""

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class EditorMilestone8Tests(unittest.TestCase):
    def read(self, relative: str) -> str:
        return (ROOT / relative).read_text(encoding="utf-8")

    def test_inspector_preserves_component_card_architecture_and_all_light_types(self):
        source = self.read("Editor/src/EditorComponentInspector.cpp")
        for required in (
            '"Light", "LightCard", readOnly',
            "LightType::Directional",
            "LightType::Point",
            "LightType::Spot",
            'Property("Colour"',
            'Property("Intensity"',
            'Property("Range"',
            'Property("Inner Cone"',
            'Property("Outer Cone"',
            'ImGui::ColorEdit3("##Colour"',
            'ImGui::DragFloat("##Intensity"',
            'ImGui::DragFloat("##Range"',
        ):
            self.assertIn(required, source)

    def test_light_edits_commit_immediately_through_scene_document(self):
        source = self.read("Editor/src/EditorComponentInspector.cpp")
        self.assertIn("auto commitLight = [&]", source)
        self.assertIn("session.EditDocument().SetLight(selected->entity, *componentLight_)", source)
        self.assertIn('if (ImGui::ColorEdit3("##Colour"', source)
        self.assertIn('if (ImGui::DragFloat("##Intensity"', source)
        self.assertIn('if (ImGui::DragFloat("##Range"', source)
        self.assertIn("commitLight();", source)
        self.assertNotIn('Property("Power"', source)
        self.assertNotIn('Property("Radius"', source)

    def test_type_specific_controls_and_spot_cones_are_validated(self):
        source = self.read("Editor/src/EditorComponentInspector.cpp")
        self.assertIn("componentLight_->light.type != LightType::Point", source)
        self.assertIn("componentLight_->light.type != LightType::Directional", source)
        self.assertIn("componentLight_->light.type == LightType::Spot", source)
        self.assertIn("outerAngle - 0.1f", source)
        self.assertIn("innerAngle + 0.1f", source)
        self.assertIn("ValidateLight", source)

    def test_existing_renderer_shadow_pass_is_exposed_without_fake_per_light_contract(self):
        source = self.read("Editor/src/EditorComponentInspector.cpp")
        self.assertIn('Property("Shadows"', source)
        self.assertIn('ImGui::Checkbox("##SceneShadows"', source)
        self.assertIn("engine.Graphics().Settings()", source)
        self.assertIn("renderSettings.shadows = sceneShadows", source)
        self.assertIn("engine.Graphics().SetSettings(renderSettings)", source)

    def test_milestone4_read_only_policy_is_preserved_for_lighting(self):
        source = self.read("Editor/src/EditorComponentInspector.cpp")
        self.assertIn("const bool hierarchyLocked = hierarchy_.Locked(selected->persistentID)", source)
        self.assertIn("const bool readOnly = !editable || hierarchyLocked", source)
        self.assertIn('"Light", "LightCard", readOnly', source)
        self.assertIn("ImGui::BeginDisabled(readOnly)", source)

    def test_scene_view_draws_directional_point_and_spot_light_guides(self):
        source = self.read("Editor/src/EditorViewportExperience.cpp")
        for required in (
            "DrawLightVisualizations",
            "DrawWorldCircle",
            "light.type==LightType::Directional",
            "light.type==LightType::Point",
            "outerRadius",
            "innerRadius",
            "light.radius",
            "light.direction",
        ):
            self.assertIn(required, source)

    def test_light_visualization_is_editor_only_and_obeys_gizmo_visibility(self):
        source = self.read("Editor/src/EditorViewportExperience.cpp")
        self.assertIn("if(playing||!state.gizmosVisible) return;", source)
        self.assertIn(
            "DrawLightVisualizations(draw,rect,editorCamera,viewport_,entities,session.Playing())",
            source,
        )

    def test_scope_stays_on_existing_light_and_scene_view_contracts(self):
        component = self.read("Editor/src/EditorComponentInspector.cpp")
        viewport = self.read("Editor/src/EditorViewportExperience.cpp")
        self.assertIn("LightComponent", component)
        self.assertIn("record.light", viewport)
        self.assertNotIn("MaterialAsset", viewport)
        self.assertNotIn("AnimationControllerAsset", viewport)
        self.assertNotIn("SetPhysics(selected->entity, *componentLight_)", component)

    def test_validation_and_roadmap_are_updated(self):
        local = self.read("ValidateEditor.bat")
        workflow = self.read(".github/workflows/editor-windows.yml")
        docs = self.read("Docs/Editor.md")
        self.assertIn("EditorMilestone4InspectorTests.py", local)
        self.assertIn("EditorMilestone4InspectorTests.py", workflow)
        self.assertIn("EditorMilestone8Tests.py", local)
        self.assertIn("EditorMilestone8Tests.py", workflow)
        self.assertIn("## 8. Lighting Inspector — `DONE`", docs)
        self.assertIn("## 8. Lighting Inspector — `DONE`", docs[docs.index("# Milestone Execution Log"):])


if __name__ == "__main__":
    unittest.main()
