#!/usr/bin/env python3
"""Milestone 3 regressions: real editor entity creation workflows."""

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class EditorMilestone3Tests(unittest.TestCase):
    def read(self, relative: str) -> str:
        return (ROOT / relative).read_text(encoding="utf-8")

    def setUp(self):
        self.source = self.read("Editor/src/EditorAssetInteractions.cpp")

    def test_hierarchy_exposes_real_create_workflow(self):
        self.assertIn('ImGui::Button("+ Create..."', self.source)
        self.assertIn('ImGui::OpenPopup("PlagC.CreateEntity")', self.source)
        self.assertIn('ImGui::BeginPopup("PlagC.CreateEntity")', self.source)
        self.assertIn('ImGui::MenuItem("Empty")', self.source)
        self.assertIn('ImGui::BeginMenu("Model")', self.source)
        self.assertIn('ImGui::MenuItem("Camera")', self.source)
        self.assertIn('ImGui::BeginMenu("Light")', self.source)
        self.assertIn('ImGui::BeginMenu("Primitive")', self.source)
        self.assertIn('ImGui::BeginMenu("Physics")', self.source)

    def test_creation_uses_document_commands_not_raw_world_mutation(self):
        self.assertIn("EntityTemplate entity;", self.source)
        self.assertIn("session.EditDocument().Create(std::move(entity));", self.source)
        self.assertNotIn("SystemWorld().Create", self.source)
        self.assertNotIn("SystemWorld().SetCamera", self.source)
        self.assertNotIn("SystemWorld().SetLight", self.source)
        self.assertNotIn("SystemWorld().SetPhysics", self.source)
        self.assertNotIn("SystemWorld().SetRenderable", self.source)

    def test_camera_and_all_light_types_have_defaults(self):
        self.assertIn("entity.camera = camera;", self.source)
        self.assertIn("camera.target = {anchor.x, anchor.y + 1.0f, anchor.z};", self.source)
        self.assertIn("LightType::Directional", self.source)
        self.assertIn("LightType::Point", self.source)
        self.assertIn("LightType::Spot", self.source)
        self.assertIn("light.radius = 12.0f;", self.source)
        self.assertIn("light.innerAngle = 25.0f;", self.source)
        self.assertIn("light.outerAngle = 40.0f;", self.source)
        self.assertIn("entity.light = LightComponent{light};", self.source)

    def test_primitives_create_real_renderer_resources(self):
        self.assertIn("renderer.CreateMaterial(material)", self.source)
        self.assertIn("renderer.CreateMesh(MakePrimitive(primitive))", self.source)
        self.assertIn("Primitive::Cube", self.source)
        self.assertIn("Primitive::Plane", self.source)
        self.assertIn("Primitive::Sphere", self.source)
        self.assertIn("Primitive::Cylinder", self.source)
        self.assertIn("entity.renderable = PrimitiveRenderable(engine, primitive);", self.source)

    def test_physics_presets_are_rendered_and_simulation_ready(self):
        for label in (
            '"Static Box"',
            '"Dynamic Box"',
            '"Kinematic Box"',
            '"Dynamic Sphere"',
            '"Dynamic Cylinder"',
        ):
            self.assertIn(label, self.source)
        self.assertIn("MotionType::Static", self.source)
        self.assertIn("MotionType::Dynamic", self.source)
        self.assertIn("MotionType::Kinematic", self.source)
        self.assertIn("ColliderType::Box", self.source)
        self.assertIn("ColliderType::Sphere", self.source)
        self.assertIn("ColliderType::Cylinder", self.source)
        self.assertIn("CollisionLayer::StaticWorld", self.source)
        self.assertIn("CollisionLayer::Dynamic", self.source)
        self.assertIn("entity.physics = PhysicsBody(motion, collider);", self.source)

    def test_creation_uses_authoring_context_instead_of_stacking_everything_at_origin(self):
        self.assertIn("document.Selection()", self.source)
        self.assertIn("document.Scene().ReadTransform(selected).position", self.source)
        self.assertIn("anchor.x += 1.25f;", self.source)
        self.assertIn("entity.transform.position.y += 0.5f;", self.source)
        self.assertIn("motion == MotionType::Static ? 0.5f : 1.5f", self.source)

    def test_model_create_menu_uses_cooked_model_assets(self):
        self.assertIn("entry.kind != EditorAssetKind::Cooked", self.source)
        self.assertIn('path.extension() != ".plasset"', self.source)
        self.assertIn("LoadModel(engine, entry.path.c_str())", self.source)
        self.assertIn("entity.model = ModelComponent{model, true, true};", self.source)


if __name__ == "__main__":
    unittest.main()