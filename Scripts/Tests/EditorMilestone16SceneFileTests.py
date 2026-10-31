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
        self.layer = self.read("Editor/src/EditorLayer.h")
        self.app = self.read("Editor/src/EditorApp.cpp")
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

    def test_window_close_uses_the_same_save_workflow_for_untitled_scenes(self):
        self.assertIn("bool SaveScene(Engine& engine, EditorSession& session)", self.layer)
        self.assertIn("return files_.Save(engine, session);", self.layer)
        self.assertIn("const bool canSave = session_->EditingEnabled();", self.app)
        self.assertIn("if (layer_.SaveScene(engine, *session_))", self.app)
        self.assertNotIn("session_->HasPath() && session_->EditingEnabled()", self.app)
        self.assertNotIn("session_->Save(); closePrompt_", self.app)
        self.assertNotIn("Use File > Save As before closing", self.app)

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
        load_index = compact.index(load)
        swap_index = compact.find(swap, load_index)
        self.assertNotEqual(-1, swap_index)
        self.assertLess(load_index, swap_index)
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
            "restored->Scene().Parent(restoredModel)==restoredPlain",
            "restoredPhysics && restoredPhysics->body.mass==3.0f",
            "restoredAnimator && restoredAnimator->controller && restoredAnimator->animationSource",
            'restoredAnimator->controller->Value("Speed")==0.6f',
            "restored->PersistentID(createdAfterLoad)>d.PersistentID(physical)",
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
