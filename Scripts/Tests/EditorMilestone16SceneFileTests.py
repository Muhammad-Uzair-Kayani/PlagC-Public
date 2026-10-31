#!/usr/bin/env python3
"""Milestone 16 regressions: scene-file workflow and persistent authoring state."""

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class EditorMilestone16SceneFileTests(unittest.TestCase):
    def read(self, relative: str) -> str:
        return (ROOT / relative).read_text(encoding="utf-8")

    def setUp(self):
        self.files = self.read("Editor/src/EditorFileController.cpp")
        self.header = self.read("Editor/src/EditorFileController.h")
        self.scene = self.read("PlagC/src/Editor/SceneSerialization.cpp")
        self.session = self.read("PlagC/src/Editor/EditorSession.cpp")
        self.native_tests = self.read("Scripts/Tests/EditorSerializationTests.cpp")
        self.docs = self.read("Docs/Editor.md")

    def test_file_menu_has_complete_scene_workflow_shortcuts_and_dirty_feedback(self):
        for text in (
            '"New"', '"Open..."', '"Open Recent"', '"Save"', '"Save As..."',
            '"Unsaved Changes"', 'session.Dirty() ? "File *" : "File"',
            '"Untitled.plscene"', '"Last file error:"',
            "ImGuiKey_N", "ImGuiKey_O", "ImGuiKey_S",
        ):
            self.assertIn(text, self.files)
        self.assertIn("PendingAction::OpenRecent", self.files)
        self.assertIn("RecentScenesPath", self.files)
        self.assertIn("MaxRecentScenes = 8", self.files)

    def test_unsaved_save_path_handles_untitled_scenes_through_save_as(self):
        self.assertIn("if (!session.HasPath())", self.files)
        self.assertIn("return SaveAs(engine, session);", self.files)
        self.assertIn('ImGui::Button("Save"', self.files)
        self.assertIn("if (SaveCurrent(engine, session))", self.files)
        self.assertIn("ReplacePolicy::RequireClean", self.files)
        self.assertIn("ReplacePolicy::DiscardChanges", self.files)

    def test_recent_scenes_are_persisted_and_use_same_unsaved_policy(self):
        for text in (
            "RememberRecent(engine, session.Path())",
            "RememberRecent(engine, path)",
            "RequestRecent",
            "pendingPath_",
            "SaveRecent(engine)",
            'std::filesystem::path("Saved") / "EditorRecentScenes.txt"',
        ):
            self.assertIn(text, self.files + self.header)
        self.assertIn("if (session.Dirty())", self.files)
        self.assertIn("ReplacePolicy::DiscardChanges", self.files)

    def test_replacement_stays_transactional(self):
        compact = "".join(self.session.split())
        load = "autoreplacement=SceneDocument::Load(path,std::move(resolver));"
        swap = "document_=std::move(replacement);"
        self.assertIn(load, compact)
        self.assertIn(swap, compact)
        self.assertLess(compact.index(load), compact.index(swap))
        self.assertIn("EditorSession::Open constructs and validates the replacement document first", self.files)

    def test_scene_v4_persists_physics_and_animator_components(self):
        self.assertIn("CurrentSceneVersion = 4", self.scene)
        self.assertIn('w.Key("physics"); WritePhysics', self.scene)
        self.assertIn('w.Key("animator"); WriteAnimator', self.scene)
        self.assertIn("ReadPhysics", self.scene)
        self.assertIn("ReadAnimator", self.scene)
        self.assertIn("ValidatePhysicsBody", self.scene)
        self.assertIn("AnimationController(r.animator->controller->graph,clips)", self.scene)
        self.assertNotIn("Scene v3 cannot save renderable, physics or animator components yet", self.scene)

    def test_scene_v4_keeps_asset_backed_model_and_material_assignments(self):
        self.assertIn('w.Key("materials")', self.scene)
        self.assertIn("LoadMaterialAsset", self.scene)
        self.assertIn("materialOverrides", self.scene)
        self.assertIn("animationSource", self.scene)
        self.assertIn("Model resolver required to load animator source", self.scene)

    def test_raw_renderer_handles_remain_explicitly_nonserializable(self):
        self.assertIn("if(r.renderable)", self.scene)
        self.assertIn("raw RenderableComponent renderer handles", self.scene)
        self.assertIn("use a ModelComponent asset reference", self.scene)

    def test_version_validation_backward_compatibility_and_id_continuity(self):
        self.assertIn("if(version<1 || version>CurrentSceneVersion)", self.scene)
        self.assertIn("if(version==1)", self.scene)
        self.assertIn("else if(version<4)", self.scene)
        self.assertIn("document->nextPersistentID_=maxID+1", self.scene)
        self.assertIn("Persistent entity ID space exhausted", self.scene)

    def test_native_roundtrip_covers_hierarchy_materials_physics_animator_and_new_ids(self):
        for text in (
            "loaded->ParentPersistentID(loadedChild)",
            "loadedRecord.model->materialOverrides",
            "loadedRecord.physics",
            "loadedRecord.animator",
            "loaded->PersistentID(createdAfterLoad)",
        ):
            self.assertIn(text, self.native_tests)

    def test_atomic_save_and_useful_errors_remain_enforced(self):
        self.assertIn("MoveFileExW", self.scene)
        self.assertIn("MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH", self.scene)
        self.assertIn("history_.MarkSaved()", self.scene)
        self.assertIn("ReportError", self.files)
        self.assertIn('Severity::Error, "EDITOR-FILE"', self.files)
        self.assertIn("Scene file does not exist:", self.files)

    def test_roadmap_marks_m16_complete(self):
        self.assertIn("## 16. Scene File Workflow — `DONE`", self.docs)
        self.assertIn("## Milestone 16 — Scene File Workflow — `DONE`", self.docs)


if __name__ == "__main__":
    unittest.main()
