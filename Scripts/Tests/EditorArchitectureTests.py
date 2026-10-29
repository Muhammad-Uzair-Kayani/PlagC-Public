#!/usr/bin/env python3
"""Fast source-level editor architecture regressions used on every platform."""

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class EditorArchitectureTests(unittest.TestCase):
    def read(self, relative: str) -> str:
        return (ROOT / relative).read_text(encoding="utf-8")

    def test_authoring_backend_exposes_real_scene_operations(self):
        header = self.read("PlagC/include/PlagC/Editor/SceneDocument.h")
        for required in (
            "Entity Create(",
            "void Destroy(Entity)",
            "Entity Duplicate(Entity)",
            "TransformEdit BeginTransformEdit(Entity)",
            "std::vector<AuthoringEntity> Snapshot() const",
            "CloneForRuntime() const",
            "SelectPersistent(std::uint64_t)",
        ):
            self.assertIn(required, header)

    def test_multithreaded_editor_services_are_present(self):
        header = self.read("PlagC/include/PlagC/Editor/EditorModels.h")
        for required in (
            "class MainThreadMailbox",
            "class GenerationGate",
            "class AssetBrowserModel",
            "struct ConsoleFilter",
            "struct EditorTelemetry",
            "struct InputCaptureState",
            "class PickingProvider",
        ):
            self.assertIn(required, header)

    def test_editor_session_owns_file_and_transport_policy(self):
        header = self.read("PlagC/include/PlagC/Editor/EditorSession.h")
        source = self.read("PlagC/src/Editor/EditorSession.cpp")
        for required in (
            "class EditorSession",
            "void NewEmpty(",
            "void Open(",
            "void Save();",
            "void SaveAs(",
            "void StartPlay();",
            "const SceneDocument& ViewDocument() const",
            "const World& ViewWorld() const",
        ):
            self.assertIn(required, header)
        self.assertIn("SceneDocument::Load", source)
        self.assertIn("play_.Start(*document_)", source)

    def test_editor_bridge_uses_existing_engine_services(self):
        bridge = self.read("PlagC/include/PlagC/Editor/EngineEditorBridge.h")
        for required in (
            "engine.Assets().Registry()",
            "engine.Logger().Snapshot()",
            "engine.Profile().Latest()",
            "engine.Jobs().Stats()",
        ):
            self.assertIn(required, bridge)

    def test_viewport_contract_is_renderer_backend_neutral(self):
        header = self.read("PlagC/include/PlagC/Editor/ViewportTarget.h")
        source = self.read("PlagC/src/Editor/ViewportTarget.cpp")
        combined = header + source
        self.assertIn("TargetHandle", combined)
        for forbidden in (
            "glBindTexture", "GLuint", "VkImage", "VkImageView",
            "ImGui_ImplOpenGL", "ImGui_ImplVulkan",
        ):
            self.assertNotIn(forbidden, combined)

    def test_engine_backend_does_not_depend_on_imgui(self):
        paths = [
            ROOT / "PlagC/include/PlagC/Editor",
            ROOT / "PlagC/src/Editor",
        ]
        for folder in paths:
            for path in folder.glob("*"):
                if path.suffix not in {".h", ".cpp"}:
                    continue
                text = path.read_text(encoding="utf-8")
                self.assertNotIn("#include <imgui", text.lower(), str(path))
                self.assertNotIn("#include \"imgui", text.lower(), str(path))

    def test_play_session_isolated_from_authoring_document(self):
        source = self.read("PlagC/src/Editor/PlaySession.cpp")
        self.assertIn("authoring.CloneForRuntime()", source)
        self.assertIn("runtime_.reset()", source)

    def test_plagc_target_auto_includes_editor_backend(self):
        premake = self.read("premake5.lua")
        self.assertIn('"PlagC/include/**.h"', premake)
        self.assertIn('"PlagC/src/**.cpp"', premake)


if __name__ == "__main__":
    unittest.main()
