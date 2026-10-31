#!/usr/bin/env python3
"""Source-level regressions for editor Milestone 27 Undo / Redo."""

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class EditorMilestone27UndoRedoTests(unittest.TestCase):
    def read(self, relative: str) -> str:
        return (ROOT / relative).read_text(encoding="utf-8")

    def test_core_entity_operations_are_command_backed(self):
        source = self.read("PlagC/src/Editor/SceneDocument.cpp")
        for command in (
            "CreateEntityCommand", "DeleteEntityCommand", "DuplicateEntityCommand",
            "RenameCommand", "ReparentCommand", "TransformCommand",
        ):
            self.assertIn(command, source)
        self.assertIn("history_.Execute", source)
        self.assertIn("history_.RecordApplied", source)

    def test_transform_gestures_coalesce_to_one_record(self):
        source = self.read("PlagC/src/Editor/SceneDocument.cpp")
        history = self.read("PlagC/include/PlagC/Editor/CommandHistory.h")
        transform = self.read("Editor/src/EditorTransformInspector.cpp")
        self.assertIn("transformEditActive_ = true", source)
        self.assertIn("PreviewTransform", source)
        self.assertIn("CommitTransformEdit", source)
        self.assertIn("history_.RecordApplied", source)
        self.assertIn("Interaction transactions", history)
        self.assertIn("edit_->Commit()", transform)

    def test_component_add_remove_edit_are_command_backed_with_action_labels(self):
        source = self.read("PlagC/src/Editor/SceneDocumentComponents.cpp")
        self.assertIn("ComponentLabel", source)
        self.assertIn('return "Add " + std::string(name);', source)
        self.assertIn('return "Remove " + std::string(name);', source)
        self.assertIn('return "Edit " + std::string(name);', source)
        for component in ("RenderableComponent", "PhysicsComponent", "AnimatorComponent"):
            self.assertIn(f"MakeComponentCommand<{component}>", source)
        self.assertIn("history_.Execute", source)

    def test_camera_light_and_model_changes_remain_command_backed(self):
        source = self.read("PlagC/src/Editor/SceneDocument.cpp")
        for command in ("CameraCommand", "LightCommand", "ModelCommand", "ActiveCameraCommand"):
            self.assertIn(f"std::make_unique<{command}>", source)
        for label in ("Edit camera", "Edit light", "Edit model", "Set active camera"):
            self.assertIn(f'return "{label}";', source)

    def test_material_and_model_properties_are_undoable(self):
        source = self.read("PlagC/src/Editor/SceneDocumentModel.cpp")
        for label in (
            "Edit model shadows", "Reload model asset", "Assign material", "Clear material"
        ):
            self.assertIn(f'"{label}"', source)
        self.assertIn("history_.Execute", source)

    def test_history_labels_are_exposed_in_menu_and_toolbar(self):
        layer = self.read("Editor/src/EditorLayer.cpp")
        toolbar = self.read("Editor/src/EditorToolbar.cpp")
        session = self.read("PlagC/src/Editor/EditorSession.cpp")
        self.assertIn("UndoLabel()", layer)
        self.assertIn("RedoLabel()", layer)
        self.assertIn("session.Document().UndoLabel()", toolbar)
        self.assertIn("session.Document().RedoLabel()", toolbar)
        self.assertIn("return document_->Undo();", session)
        self.assertIn("return document_->Redo();", session)

    def test_history_branching_capacity_and_saved_state_are_stable(self):
        history = self.read("PlagC/include/PlagC/Editor/CommandHistory.h")
        self.assertIn("commands_.erase(commands_.begin() + static_cast<std::ptrdiff_t>(cursor_)", history)
        self.assertIn("if (commands_.size() == capacity_)", history)
        self.assertIn("saved_ = cursor_", history)
        self.assertIn("bool Dirty() const noexcept { return saved_ != cursor_; }", history)
        self.assertIn("Editor command history re-entry", history)

    def test_play_mode_cannot_mutate_authoring_history(self):
        session = self.read("PlagC/src/Editor/EditorSession.cpp")
        self.assertIn("RequireStopped();", session)
        self.assertIn('"Editor authoring operation is disabled during Play"', session)
        self.assertIn("return !play_.Running() && document_->CanUndo();", session)
        self.assertIn("return !play_.Running() && document_->CanRedo();", session)

    def test_validation_wiring(self):
        validate = self.read("ValidateEditor.bat")
        workflow = self.read(".github/workflows/editor-windows.yml")
        self.assertIn("EditorMilestone27UndoRedoTests.py", validate)
        self.assertIn("EditorMilestone27UndoRedoTests.py", workflow)


if __name__ == "__main__":
    unittest.main()
