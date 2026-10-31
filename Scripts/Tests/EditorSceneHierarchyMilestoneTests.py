#!/usr/bin/env python3
"""Source regressions for usable Editor milestone 2: Scene Hierarchy."""

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class EditorSceneHierarchyMilestoneTests(unittest.TestCase):
    def read(self, relative: str) -> str:
        return (ROOT / relative).read_text(encoding="utf-8")

    def test_hierarchy_is_real_editor_pass_over_scene_document(self):
        source = self.read("Editor/src/EditorSceneHierarchy.cpp")
        drag_drop = self.read("Editor/src/EditorDragDrop.h")
        for required in (
            "session.ViewDocument().Snapshot()",
            "SelectPersistent",
            "FindByPersistentID",
            "SetParent",
            "ClearParent",
            "DragDrop::Entity",
            "DragDrop::EntityIDPayload",
        ):
            self.assertIn(required, source)
        self.assertIn('Entity = "PLAGC_EDITOR_ENTITY"', drag_drop)

    def test_search_types_expansion_rename_and_context_workflow_exist(self):
        source = self.read("Editor/src/EditorSceneHierarchy.cpp")
        header = self.read("Editor/src/EditorSceneHierarchy.h")
        for required in (
            "InputTextWithHint",
            "Search name or type",
            "TypeBadge",
            "Expand All",
            "Collapse All",
            "ImGuiKey_F2",
            "##rename",
            "BeginPopupContextItem",
            "Move to Scene Root",
            "expanded_",
            "renamingID_",
        ):
            self.assertIn(required, source + header)

    def test_visibility_uses_scene_document_component_commands(self):
        source = self.read("Editor/src/EditorSceneHierarchy.cpp")
        self.assertIn("SetModelVisible", source)
        self.assertIn("SetRenderable", source)
        self.assertIn("renderable.visible = pending.flag", source)

    def test_lock_is_editor_only_and_blocks_viewport_picking(self):
        header = self.read("Editor/src/EditorSceneHierarchy.h")
        app = self.read("Editor/src/EditorApp.cpp")
        source = self.read("Editor/src/EditorSceneHierarchy.cpp")
        self.assertIn("std::unordered_set<std::uint64_t> locked_", header)
        self.assertIn("!layer_.HierarchyEntityLocked(hit->persistentID)", app)
        self.assertIn("document.Select({})", source)
        self.assertNotIn("locked_", self.read("PlagC/include/PlagC/World/World.h"))

    def test_editor_layer_uses_single_authoritative_hierarchy_frontend(self):
        layer = self.read("Editor/src/EditorLayer.cpp")
        header = self.read("Editor/src/EditorLayer.h")
        self.assertIn("hierarchy_.Draw(engine, session, showHierarchy_)", layer)
        self.assertIn("EditorSceneHierarchy hierarchy_", header)
        self.assertNotIn('TreeNodeEx("##entity"', layer)

    def test_viewport_picking_respects_hierarchy_lock(self):
        app = self.read("Editor/src/EditorApp.cpp")
        self.assertIn("!layer_.HierarchyEntityLocked(hit->persistentID)", app)

    def test_regression_is_wired_into_local_and_windows_validation(self):
        workflow = self.read(".github/workflows/editor-windows.yml")
        local = self.read("ValidateEditor.bat")
        self.assertIn("EditorSceneHierarchyMilestoneTests.py", workflow)
        self.assertIn("EditorSceneHierarchyMilestoneTests.py", local)

    def test_roadmap_marks_scene_hierarchy_done(self):
        doc = self.read("Docs/Editor.md")
        self.assertGreaterEqual(doc.count("## 2. Scene Hierarchy — `DONE`"), 2)


if __name__ == "__main__":
    unittest.main()
