#!/usr/bin/env python3
"""Milestone 12 regressions: Animation State Machine editor and persistence."""

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class EditorMilestone12Tests(unittest.TestCase):
    def read(self, relative: str) -> str:
        return (ROOT / relative).read_text(encoding="utf-8")

    def setUp(self):
        self.editor = self.read("Editor/src/EditorAnimationStateMachine.cpp")
        self.header = self.read("PlagC/include/PlagC/Animation/AnimationControllerAsset.h")
        self.persistence = self.read("PlagC/src/Animation/AnimationControllerAsset.cpp")
        self.runtime = self.read("PlagC/src/Animation/OzzAnimator.cpp")
        self.controller = self.read("PlagC/include/PlagC/Animation/AnimationController.h")

    def test_controller_asset_persists_graph_and_node_layout(self):
        self.assertIn("AnimationControllerAsset", self.header)
        self.assertIn("AnimationStateMachine graph", self.header)
        self.assertIn("std::vector<AnimationNodeLayout> layout", self.header)
        self.assertIn('"PGANIMCTRL "', self.persistence)
        for tag in ('"PARAMETERS "', '"STATES "', '"TRANSITIONS "', '"ENTRY "'):
            self.assertIn(tag, self.persistence)
        self.assertIn("std::filesystem::rename(temporary, path", self.persistence)
        self.assertIn("asset.Validate(clips)", self.persistence)

    def test_editor_is_node_based_and_nodes_are_draggable(self):
        self.assertIn('ImGui::Begin("Animation State Machine"', self.editor)
        self.assertIn("DrawGraphCanvas", self.editor)
        self.assertIn("ImGui::InvisibleButton(\"node\"", self.editor)
        self.assertIn("ImGui::IsMouseDragging(ImGuiMouseButton_Left)", self.editor)
        self.assertIn("draw->AddLine", self.editor)
        self.assertIn("animationGraph_.layout", self.editor)

    def test_state_parameter_and_transition_authoring_is_present(self):
        for label in (
            '"Add State"', '"Entry State"', '"Add Parameter"',
            '"Add Transition"', '"Transition Duration"', '"Add Condition"'
        ):
            self.assertIn(label, self.editor)
        self.assertIn("AnimationParameterType::Float", self.editor)
        self.assertIn("AnimationParameterType::Bool", self.editor)
        self.assertIn("AnimationParameterType::Trigger", self.editor)
        self.assertIn("AnimationCompare::Greater", self.editor)
        self.assertIn("AnimationCompare::Less", self.editor)
        self.assertIn("AnimationCompare::Equal", self.editor)
        self.assertIn("AnimationCompare::NotEqual", self.editor)

    def test_editor_compiles_graph_into_real_runtime_controller(self):
        self.assertIn("animator.controller.emplace(asset.graph, clips)", self.editor)
        self.assertIn("session.EditDocument().SetAnimator", self.editor)
        self.assertIn("controller.Evaluate()", self.runtime)
        self.assertIn("component.fadeDuration = transition->crossfade", self.runtime)
        self.assertIn("current_ = t.to", self.controller)

    def test_runtime_active_state_is_visualized_and_play_authoring_is_locked(self):
        self.assertIn("controller->Current().name.c_str()", self.editor)
        self.assertIn('"Runtime state: %s"', self.editor)
        self.assertIn("const bool editable = session.EditingEnabled()", self.editor)
        self.assertIn('"Play mode: graph authoring is locked; active runtime state remains visible."', self.editor)

    def test_save_load_and_apply_actions_are_real(self):
        self.assertIn('"Controller Asset"', self.editor)
        self.assertIn('ImGui::Button("Load")', self.editor)
        self.assertIn('ImGui::Button("Save")', self.editor)
        self.assertIn('ImGui::Button("Apply to Animator")', self.editor)
        self.assertIn("LoadAnimationControllerAsset", self.editor)
        self.assertIn("SaveAnimationControllerAsset", self.editor)
        self.assertNotIn("Ogre::", self.editor)


if __name__ == "__main__":
    unittest.main()
