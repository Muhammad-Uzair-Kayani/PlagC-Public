#!/usr/bin/env python3
"""Milestone 13 regressions: usable Content Browser workflow."""

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class EditorMilestone13Tests(unittest.TestCase):
    def read(self, relative: str) -> str:
        return (ROOT / relative).read_text(encoding="utf-8")

    def setUp(self):
        self.cpp = self.read("Editor/src/EditorDataPanels.cpp")
        self.header = self.read("Editor/src/EditorDataPanels.h")
        self.interactions = self.read("Editor/src/EditorAssetInteractions.cpp")
        self.docs = self.read("Docs/Editor.md")

    def test_folder_navigation_and_breadcrumbs(self):
        for text in (
            '"Assets"',
            "contentFolder_",
            "DrawBreadcrumbs",
            "ImmediateFolders",
            '"Back##ContentBrowser"',
            '"Up##ContentBrowser"',
        ):
            self.assertIn(text, self.cpp + self.header)

    def test_grid_list_search_filter_and_sort_controls(self):
        for text in (
            '"Grid"',
            '"List"',
            '"Search assets..."',
            '"All Types"',
            '"Name A-Z"',
            '"Name Z-A"',
            '"Largest"',
            '"Smallest"',
        ):
            self.assertIn(text, self.cpp)
        self.assertIn("contentGridView_", self.header)
        self.assertIn("contentKindFilter_", self.header)
        self.assertIn("contentSort_", self.header)

    def test_asset_cards_details_source_cooked_and_internal_filtering(self):
        self.assertIn("DrawAssetCard", self.cpp)
        self.assertIn("DrawAssetDetails", self.cpp)
        self.assertIn('"SOURCE"', self.cpp)
        self.assertIn('"COOKED"', self.cpp)
        self.assertIn("IsInternalAsset", self.cpp)
        self.assertIn('".import.json"', self.cpp)
        self.assertIn("contentShowInternal_", self.header)

    def test_context_menu_double_click_and_drag_drop_are_real(self):
        self.assertIn("BeginPopupContextItem", self.cpp)
        self.assertIn('"Copy Asset Path"', self.cpp)
        self.assertIn("ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)", self.cpp)
        self.assertIn("QueueCookedModelActivation", self.cpp)
        self.assertIn("ConsumeCookedModelActivation", self.header)
        self.assertIn('"PLAGC_COOKED_MODEL"', self.cpp)
        self.assertIn("ImGui::SetDragDropPayload", self.cpp)
        self.assertIn("const std::string activation = dataPanels_.ConsumeCookedModelActivation();", self.interactions)

    def test_reveal_asset_navigates_and_selects(self):
        self.assertIn("void EditorDataPanels::RevealAsset", self.cpp)
        self.assertIn("contentFolder_", self.cpp)
        self.assertIn("selectedAsset_", self.header)
        self.assertIn("revealAsset_", self.header)

    def test_thumbnail_and_scene_workflow_scope_remain_separate(self):
        self.assertNotIn("CreateTexture", self.cpp)
        self.assertNotIn("SceneDocument::Deserialize", self.cpp)
        self.assertIn("## 14. Thumbnail / Preview System — `NOT STARTED`", self.docs)
        self.assertIn("## 16. Scene File Workflow — `FOUNDATION`", self.docs)

    def test_roadmap_marks_milestone_complete(self):
        self.assertIn("## 13. Content Browser — `DONE`", self.docs)
        self.assertIn("## Milestone 13 — Content Browser — `DONE`", self.docs)


if __name__ == "__main__":
    unittest.main()
