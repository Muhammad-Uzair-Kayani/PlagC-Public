"Architecture regression checks for the engine/client boundary."

import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]


class EngineBoundaryTests(unittest.TestCase):
    def read(self, relative: str) -> str:
        return (ROOT / relative).read_text(encoding="utf-8")

    def test_game_implementation_is_not_part_of_engine_repository(self):
        self.assertFalse((ROOT / "Game").exists())

    def test_build_does_not_compile_game_sources(self):
        premake = self.read("premake5.lua")
        for forbidden in ("Game/src", "Game/Framework", "Game/Default", "PC_TPS_VISUAL_MAX"):
            self.assertNotIn(forbidden, premake)

    def test_engine_config_has_no_game_selection(self):
        application = self.read("PlagC/include/PlagC/Application.h")
        for forbidden in (
            "std::string game;",
            "std::string world;",
            "gameAssetRoot",
            "gameplayMs",
            "combatMs",
            "landscapeMs",
            "hudMs",
            "characterWorldMs",
        ):
            self.assertNotIn(forbidden, application)

    def test_entry_point_is_client_agnostic(self):
        entry = self.read("PlagC/src/EntryPoint.cpp")
        for forbidden in (
            "OUTPOST ZERO",
            "Docs/TPS.md",
            "Combat/door",
            "Landscape",
            "Unclassified game",
            "Game read",
            "Movement intent",
        ):
            self.assertNotIn(forbidden, entry)

    def test_runtime_staging_is_engine_only(self):
        manifest = self.read("Scripts/RuntimeAssetManifest.py")
        staging = self.read("Scripts/StageRuntimeAssets.py")
        for forbidden in ("Assets/Cooked/TPS", "AAAWorld", "GameAssets", "SurvivalGame"):
            self.assertNotIn(forbidden, manifest)
            self.assertNotIn(forbidden, staging)

    def test_project_generators_do_not_fetch_game_assets(self):
        generators = self.read("GenerateProjects.bat") + self.read("GenerateProjects.sh")
        for forbidden in (
            "FetchVisualAssets",
            "BuildAAAAssets",
            "PrepareTPSAssets",
            "OUTPOST ZERO",
            "TestTPS",
        ):
            self.assertNotIn(forbidden, generators)

    def test_promoted_facilities_live_under_plagc(self):
        for relative in (
            "PlagC/include/PlagC/Assets/AssetManifest.h",
            "PlagC/src/Assets/AssetManifest.cpp",
            "PlagC/include/PlagC/Debug/FpsOverlay.h",
            "PlagC/src/Debug/FpsOverlay.cpp",
        ):
            self.assertTrue((ROOT / relative).is_file(), relative)


if __name__ == "__main__":
    unittest.main()
