#!/usr/bin/env python3
"""Milestone 10 regressions: physics authoring, collision masks and collider debug drawing."""

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class EditorMilestone10Tests(unittest.TestCase):
    def read(self, relative: str) -> str:
        return (ROOT / relative).read_text(encoding="utf-8")

    def setUp(self):
        self.inspector = self.read("Editor/src/EditorPhysicsInspector.cpp")
        self.layer = self.read("Editor/src/EditorLayer.h")
        self.viewport = self.read("Editor/src/EditorViewportExperience.cpp")
        self.types = self.read("PlagC/include/PlagC/Physics/PhysicsTypes.h")
        self.validation = self.read("PlagC/src/Physics/PhysicsTypes.cpp")
        self.contacts = self.read("PlagC/src/Physics/JoltContacts.h")
        self.jolt = self.read("PlagC/src/Physics/JoltPhysicsWorld.cpp")
        self.scene = self.read("PlagC/src/Editor/SceneSerialization.cpp")
        self.docs = self.read("Docs/Editor.md")

    def test_physics_inspector_is_composed_into_scene_view(self):
        self.assertIn(
            "void AppendPhysicsInspector(Engine&, EditorSession&, const CameraDesc& editorCamera);",
            self.layer,
        )
        self.assertIn("AppendPhysicsInspector(engine,session,editorCamera);", self.viewport)
        self.assertNotIn("Ogre::", self.inspector)
        self.assertNotIn("JPH::", self.inspector)

    def test_complete_rigidbody_authoring_surface_is_present(self):
        for text in (
            '"Body Type"',
            '"Mass"',
            '"Gravity Scale"',
            '"Trigger"',
            '"Layer"',
            '"Collision Mask"',
            '"Linear Damping"',
            '"Angular Damping"',
            '"Allow Sleep"',
            '"Continuous CCD"',
            '"Collision Group"',
        ):
            self.assertIn(text, self.inspector)
        for motion in ("MotionType::Static", "MotionType::Dynamic", "MotionType::Kinematic"):
            self.assertIn(motion, self.inspector)

    def test_collider_and_material_authoring_is_command_backed(self):
        for text in (
            '"Collider Type"',
            '"Dimensions"',
            '"Friction"',
            '"Restitution"',
            '"Center of Mass"',
        ):
            self.assertIn(text, self.inspector)
        for collider in (
            "ColliderType::Box",
            "ColliderType::Sphere",
            "ColliderType::Capsule",
            "ColliderType::Cylinder",
            "ColliderType::ConvexHull",
            "ColliderType::Compound",
            "ColliderType::Mesh",
            "ColliderType::Heightfield",
        ):
            self.assertIn(collider, self.inspector)
        self.assertIn("session.EditDocument().SetPhysics(selected->entity,physics)", self.inspector)
        self.assertGreaterEqual(self.inspector.count("ImGui::IsItemDeactivatedAfterEdit()"), 8)

    def test_collision_mask_is_real_runtime_state_not_editor_only(self):
        self.assertIn("using CollisionLayerMask = std::uint32_t;", self.types)
        self.assertIn("CollisionLayerMask collisionMask=AllCollisionLayers;", self.types)
        self.assertIn("(b.collisionMask&~AllCollisionLayers)==0", self.validation)
        self.assertIn("OnContactValidate", self.contacts)
        self.assertIn("aAccepts&&bAccepts", self.contacts)
        self.assertIn("std::shared_mutex filterMutex", self.contacts)
        self.assertIn("p.contacts.RegisterBody(handle,c.body.layer,c.body.collisionMask);", self.jolt)
        self.assertIn("p.contacts.UnregisterBody(h.value)", self.jolt)

    def test_layer_trigger_rules_and_validation_are_explicit(self):
        self.assertIn("CollisionLayer::Sensor", self.inspector)
        self.assertIn("Static bodies must use Static World", self.inspector)
        self.assertIn("Sphere and capsule colliders require uniform Transform scale", self.inspector)
        self.assertIn("Mesh and heightfield colliders are static-only", self.inspector)
        self.assertIn("geometry-backed collider has no collider geometry", self.inspector)

    def test_collider_debug_visualization_is_editor_only(self):
        self.assertIn("void DrawCollider", self.inspector)
        self.assertIn("physicsDebugVisible_&&!session.Playing()", self.inspector)
        self.assertIn('"Show collider debug"', self.inspector)
        self.assertIn('"All physics entities"', self.inspector)
        self.assertIn("DrawCollider(viewportWindow->DrawList", self.inspector)
        self.assertIn("physicsDebugVisible_", self.layer)
        self.assertIn("physicsDebugAll_", self.layer)

    def test_milestone_16_owns_scene_persistence_without_regressing_physics(self):
        self.assertIn("CurrentSceneVersion = 4", self.scene)
        self.assertIn('w.Key("physics"); WritePhysics', self.scene)
        self.assertIn("ReadPhysics", self.scene)
        self.assertIn("ValidatePhysicsBody", self.scene)
        self.assertIn("## 16. Scene File Workflow — `DONE`", self.docs)

    def test_roadmap_marks_milestone_complete(self):
        self.assertIn("## 10. Physics Inspector — `DONE`", self.docs)
        self.assertIn("## Milestone 10 — Physics Inspector — `DONE`", self.docs)


if __name__ == "__main__":
    unittest.main()
