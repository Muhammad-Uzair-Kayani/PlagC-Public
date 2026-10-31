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

    def test_renderer_preference_is_restored_without_overriding_explicit_selection(self):
        application_h = self.read("PlagC/include/PlagC/Application.h")
        application_cpp = self.read("PlagC/src/Application.cpp")
        selection_h = self.read("PlagC/include/PlagC/Renderer/BackendSelection.h")
        self.assertIn("bool rendererSelectionPrompt = false", application_h)
        self.assertIn("LoadRendererPreference", selection_h)
        self.assertIn("SaveRendererPreference", selection_h)
        self.assertIn("requestedBackend == RendererBackend::Auto", application_cpp)
        self.assertIn("LoadRendererPreference(DefaultRendererPreferencePath()", application_cpp)
        self.assertIn("availableBackends.Supports(preferred)", application_cpp)
        self.assertIn("ResolveRendererBackend(requestedBackend", application_cpp)

    def test_automatic_vulkan_failure_falls_back_but_explicit_requests_do_not(self):
        application = self.read("PlagC/src/Application.cpp")
        self.assertIn("config.rendererBackend == RendererBackend::Auto", application)
        self.assertIn("allowAutomaticFallback", application)
        self.assertIn("backend == RendererBackend::Vulkan", application)
        self.assertIn("availableBackends.Supports(RendererBackend::OpenGL)", application)
        self.assertIn("Vulkan initialization failed; falling back to OpenGL", application)
        self.assertIn("createRenderer(RendererBackend::OpenGL)", application)
        self.assertIn("SaveRendererPreference(DefaultRendererPreferencePath(), RendererBackend::OpenGL)", application)
        self.assertIn("if (!canFallback)\n            throw;", application)

    def test_non_vulkan_renderer_generation_keeps_workspace_ui_hook(self):
        dependencies = self.read("Build/Dependencies.lua")
        renderer = self.read("Build/Renderer.lua")
        self.assertIn('include "Renderer.lua"', dependencies)
        self.assertIn("if PLAGC_VULKAN then", renderer)
        self.assertIn("return vulkanEngineSourcesImpl()", renderer)
        self.assertIn("Detail::OgreUIWorkspaceScope uiWorkspace(this, p.workspace);", renderer)
        self.assertIn("assert(first, 'Common Ogre renderer UI hook changed')", renderer)
        self.assertIn("removefiles { original }", renderer)
        self.assertIn("files { output }", renderer)

    def test_native_editor_ui_smoke_catches_unpresented_commands(self):
        script = self.read("Scripts/TestEditorUI.bat")
        self.assertIn("--renderer %BACKEND% --frames 12", script)
        self.assertIn("vulkan", script)
        self.assertIn("opengl", script)
        self.assertIn("[PlagC UI] ERROR: submitted UI was not presented", script)
        self.assertIn("[PlagC UI] explicit output=", script)
        self.assertIn("vertices=0", script)
        self.assertIn("commands=0", script)

    def test_editor_can_cancel_os_close_for_unsaved_scene_prompt(self):
        window_h = self.read("PlagC/include/PlagC/Window.h")
        window_cpp = self.read("PlagC/src/Window.cpp")
        editor = self.read("Editor/src/EditorApp.cpp")
        layer = self.read("Editor/src/EditorLayer.h")
        self.assertIn("void CancelClose();", window_h)
        self.assertIn("glfwSetWindowShouldClose(impl_->handle, GLFW_FALSE)", window_cpp)
        self.assertIn("EventType::Close", editor)
        self.assertIn("session_->Dirty()", editor)
        self.assertIn("session_->Document().HasTransientEdit()", editor)
        self.assertIn("window->CancelClose()", editor)
        self.assertIn('ImGui::Button("Save & Exit")', editor)
        self.assertIn('ImGui::Button("Discard & Exit")', editor)
        self.assertIn('ImGui::Button("Cancel")', editor)
        self.assertIn("const bool canSave = session_->EditingEnabled();", editor)
        self.assertIn("layer_.SaveScene(engine, *session_)", editor)
        self.assertIn("bool SaveScene(Engine& engine, EditorSession& session)", layer)
        self.assertIn("return files_.Save(engine, session);", layer)
        self.assertIn("window->RequestClose()", editor)

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
