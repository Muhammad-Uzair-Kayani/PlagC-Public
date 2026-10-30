#!/usr/bin/env python3
"""Focused source regressions for Milestone 13 Content Browser."""

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class EditorMilestone13ContentBrowserTests(unittest.TestCase):
    def read(self, relative: str) -> str:
        return (ROOT / relative).read_text(encoding="utf-8")

    def test_browser_replaces_flat_registry_table_with_navigation_views(self):
        panel = self.read("Editor/src/EditorDataPanels.cpp")
        self.assertNotIn('BeginTable("AssetTable"', panel)
        self.assertIn('DrawBreadcrumbs(browserFolder_)', panel)
        self.assertIn('ImGui::Button("Up")', panel)
        self.assertIn('"ContentBrowserGrid"', panel)
        self.assertIn('"ContentBrowserList"', panel)
        self.assertIn('assetGridView_', panel)

    def test_search_filter_sort_and_internal_filtering_are_real(self):
        header = self.read("Editor/src/EditorDataPanels.h")
        panel = self.read("Editor/src/EditorDataPanels.cpp")
        self.assertIn("assetOriginFilter_", header)
        self.assertIn("assetTypeFilter_", header)
        self.assertIn("assetSort_", header)
        self.assertIn("assetSortAscending_", header)
        self.assertIn("showInternalAssets_", header)
        self.assertIn('"Search current folder..."', panel)
        self.assertIn('"All origins", "Source", "Cooked", "Project"', panel)
        self.assertIn('"All types"', panel)
        self.assertIn('SortItems(items, assetSort_, assetSortAscending_)', panel)
        self.assertIn('lowered.starts_with("assets://manifests/")', panel)
        self.assertIn('lowered.starts_with("assets://shaders/")', panel)
        self.assertIn('name.ends_with(".import")', panel)
        self.assertIn('"Show Internal"', panel)

    def test_cards_details_context_and_double_click_are_present(self):
        panel = self.read("Editor/src/EditorDataPanels.cpp")
        self.assertIn('KindBadge(item.asset->kind)', panel)
        self.assertIn('ImGui::BeginPopupContextItem', panel)
        self.assertIn('ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)', panel)
        self.assertIn("cookedModelActivation_ = item.path", panel)
        self.assertIn("ConsumeCookedModelActivation", panel)
        self.assertIn('ImGui::SeparatorText("Details")', panel)
        self.assertIn('"Reload Runtime Asset"', panel)
        self.assertIn('ImGui::SetClipboardText', panel)

    def test_existing_model_drag_contract_is_preserved_without_legacy_flat_list(self):
        panel = self.read("Editor/src/EditorDataPanels.cpp")
        interactions = self.read("Editor/src/EditorAssetInteractions.cpp")
        viewport_drop = self.read("Editor/src/EditorViewportAssetDrop.cpp")
        self.assertIn('constexpr const char* CookedModelPayload = "PLAGC_COOKED_MODEL"', panel)
        self.assertIn('ImGui::SetDragDropPayload(CookedModelPayload', panel)
        self.assertIn("dataPanels_.ConsumeCookedModelActivation()", interactions)
        self.assertNotIn('SeparatorText("Cooked runtime assets")', interactions)
        self.assertIn('"PLAGC_COOKED_MODEL"', viewport_drop)

    def test_source_cooked_relationship_uses_existing_import_receipts(self):
        panel = self.read("Editor/src/EditorDataPanels.cpp")
        self.assertIn("#include <PlagC/Assets/AssetImport.h>", panel)
        self.assertIn("LoadAssetImportReceipt", panel)
        self.assertIn("ImportReceiptPath", panel)
        self.assertIn('"Source / Cooked relationship"', panel)
        self.assertIn('"Reveal Source"', panel)
        self.assertIn('"Reveal Cooked"', panel)
        self.assertIn("receipt->kind == AssetImportKind::Model", panel)

    def test_browser_deduplicates_registry_and_filesystem_paths(self):
        panel = self.read("Editor/src/EditorDataPanels.cpp")
        self.assertIn("CanonicalBrowserPath", panel)
        self.assertIn('"Assets://Cooked/" + path', panel)
        self.assertIn("MergeBrowserEntries", panel)
        self.assertIn("std::unordered_map<std::string, std::size_t> byPath", panel)

    def test_m13_is_wired_and_scope_stops_before_thumbnail_and_import_work(self):
        local = self.read("ValidateEditor.bat")
        ci = self.read(".github/workflows/editor-windows.yml")
        docs = self.read("Docs/Editor.md")
        panel = self.read("Editor/src/EditorDataPanels.cpp")
        self.assertIn("EditorMilestone13ContentBrowserTests.py", local)
        self.assertIn("EditorMilestone13ContentBrowserTests.py", ci)
        self.assertIn("## 13. Content Browser — `DONE`", docs)
        self.assertIn("## 14. Thumbnail / Preview System — `NOT STARTED`", docs)
        self.assertIn("## 17. Play Toolbar — `FOUNDATION`", docs)
        self.assertNotIn("CreateUiTexture", panel)
        self.assertNotIn("CookImportedAsset(", panel)


if __name__ == "__main__":
    unittest.main()
