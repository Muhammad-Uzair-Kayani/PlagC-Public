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
            "void SetParent(Entity child, Entity parent)",
            "std::uint64_t ParentPersistentID(Entity) const",
        ):
            self.assertIn(required, header)

    def test_component_authoring_commands_cover_world_components(self):
        header = self.read("PlagC/include/PlagC/Editor/SceneDocument.h")
        source = self.read("PlagC/src/Editor/SceneDocumentComponents.cpp")
        for required in (
            "void SetRenderable(Entity, const RenderableComponent&);",
            "void RemoveRenderable(Entity);",
            "void SetPhysics(Entity, const PhysicsComponent&);",
            "void RemovePhysics(Entity);",
            "void SetAnimator(Entity, const AnimatorComponent&);",
            "void RemoveAnimator(Entity);",
        ):
            self.assertIn(required, header)
        self.assertIn("class ComponentCommand final : public Command", source)
        self.assertIn("history_.Execute", source)
        self.assertIn("PersistentID(entity)", source)
        self.assertIn("FindByPersistentID(persistentID)", source)
        self.assertIn("world_->SetPhysics", source)
        self.assertIn("world_->RemovePhysics", source)
        self.assertIn("world_->SetAnimator", source)
        self.assertIn("world_->RemoveAnimator", source)
        self.assertIn("world_->SetRenderable", source)
        self.assertIn("world_->RemoveRenderable", source)

    def test_component_inspector_uses_scene_document_commands(self):
        header = self.read("Editor/src/EditorLayer.h")
        source = self.read("Editor/src/EditorComponentInspector.cpp")
        app = self.read("Editor/src/EditorApp.cpp")
        self.assertIn("void AppendComponentInspector(Engine&, EditorSession&);", header)
        self.assertIn("InspectorVisible() const noexcept", header)
        self.assertIn('ImGui::Begin("Inspector")', source)
        for required in (
            "session.EditDocument().SetCamera", "session.EditDocument().RemoveCamera",
            "session.EditDocument().SetLight", "session.EditDocument().RemoveLight",
            "session.EditDocument().SetModelVisible", "session.EditDocument().RemoveModel",
            "session.EditDocument().SetPhysics", "session.EditDocument().RemovePhysics",
            "session.EditDocument().SetAnimator", "session.EditDocument().RemoveAnimator",
        ):
            self.assertIn(required, source)
        for forbidden in (
            "world_->SetCamera", "world_->SetLight", "world_->SetPhysics",
            "world_->SetAnimator", ".Scene().Set",
        ):
            self.assertNotIn(forbidden, source)
        self.assertIn("layer_.AppendComponentInspector(engine, *session_)", app)

    def test_world_hierarchy_is_engine_owned_and_world_space_compatible(self):
        header = self.read("PlagC/include/PlagC/World/World.h")
        source = self.read("PlagC/src/World/World.cpp")
        for required in (
            "Entity Parent(Entity) const",
            "std::vector<Entity> Children(Entity) const",
            "void SetParent(Entity child, Entity parent)",
            "void ClearParent(Entity child)",
            "world-space",
        ):
            self.assertIn(required, header)
        self.assertIn('"world hierarchy cycle"', source)
        self.assertIn("parentByChild", source)

    def test_scene_v4_persists_hierarchy_and_keeps_v1_v3_readable(self):
        source = self.read("PlagC/src/Editor/SceneSerialization.cpp")
        self.assertIn("CurrentSceneVersion = 4", source)
        self.assertIn("if(version<1 || version>CurrentSceneVersion)", source)
        self.assertIn("if(version==1)", source)
        self.assertIn("else if(version<4)", source)
        self.assertIn('w.Key("parent")', source)
        self.assertIn("ValidateHierarchy(records,ids)", source)

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

    def test_editor_bridge_uses_existing_engine_services(self):
        bridge = self.read("PlagC/include/PlagC/Editor/EngineEditorBridge.h")
        for required in (
            "engine.Assets().Registry()",
            "engine.Logger().Snapshot()",
            "engine.Profile().Latest()",
            "engine.Jobs().Stats()",
        ):
            self.assertIn(required, bridge)

    def test_scene_renderer_exposes_object_identity_bridge(self):
        header = self.read("PlagC/include/PlagC/World/SceneRenderer.h")
        source = self.read("PlagC/src/World/SceneRenderer.cpp")
        for required in (
            "ObjectHandle ObjectFor(Entity) const",
            "Entity EntityForObject(ObjectHandle) const",
            "entityByObject",
        ):
            self.assertIn(required, header + source)
        self.assertIn("p.entityByObject.erase", source)
        self.assertIn("p.entityByObject.emplace", source)

    def test_scene_pick_resolves_renderer_objects_to_persistent_ids(self):
        header = self.read("PlagC/include/PlagC/Editor/ScenePicking.h")
        source = self.read("PlagC/src/Editor/ScenePicking.cpp")
        self.assertIn("ResolveSceneObjectPick", header)
        self.assertIn("sceneRenderer.EntityForObject(object)", source)
        self.assertIn("document.PersistentID(entity)", source)
        self.assertIn("document.Scene().Alive(entity)", source)

    def test_physics_picker_is_a_collider_backed_fallback(self):
        math_header = self.read("PlagC/include/PlagC/Editor/PickingMath.h")
        math_source = self.read("PlagC/src/Editor/PickingMath.cpp")
        header = self.read("PlagC/include/PlagC/Editor/PhysicsPicking.h")
        source = self.read("PlagC/src/Editor/PhysicsPicking.cpp")
        self.assertIn("PickRay BuildPickRay", math_header)
        self.assertIn("class PhysicsPickingProvider final : public PickingProvider", header)
        self.assertIn("physics_.Raycast", source)
        self.assertIn("physics_.EntityFor", source)
        self.assertIn("document_.PersistentID", source)
        self.assertIn("camera.farClip - camera.nearClip", math_source)

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

    def test_ogre_viewport_interop_is_isolated_and_revision_aware(self):
        header = self.read("PlagC/include/PlagC/Editor/OgreViewportInterop.h")
        source = self.read("PlagC/src/Editor/OgreViewportInterop.cpp")
        viewport = self.read("PlagC/src/Editor/ViewportTarget.cpp")
        self.assertIn("Ogre::TextureGpu* texture", header)
        self.assertIn("ResolveOgreViewportBinding", header + source)
        self.assertIn("findTextureNoThrow", source)
        self.assertIn('"PlagC/Target"', source)
        self.assertIn("viewport.Revision()", source)
        self.assertIn("const auto replacement = renderer_.CreateTarget", viewport)
        self.assertNotIn("renderer_.ResizeTarget(handle_", viewport)

    def test_ogre_ui_listener_is_workspace_local_for_vulkan(self):
        backend = self.read("PlagC/src/Renderer/OgreUiBackend.cpp")
        patch = self.read("Build/Patches/OgreRendererPerformance.lua")
        self.assertIn("workspace->addListener(this)", backend)
        self.assertIn("workspace->removeListener(this)", backend)
        self.assertNotIn("compositorManager_->addListener(this)", backend)
        self.assertIn("Detail::OgreUIWorkspaceScope uiWorkspace(this, p.workspace);", patch)

    def test_renderer_preferences_are_persistent_and_restart_gated(self):
        header = self.read("Editor/src/EditorPreferences.h")
        source = self.read("Editor/src/EditorPreferences.cpp")
        restart = self.read("Editor/src/EditorRestart.cpp")
        app = self.read("Editor/src/EditorApp.cpp")
        self.assertIn("class EditorPreferences final", header)
        self.assertIn("RestartRequired(RendererBackend active)", header)
        self.assertIn("SaveRendererPreference(DefaultRendererPreferencePath()", source)
        self.assertIn('ImGui::Begin("Preferences"', source)
        self.assertIn('ImGui::BeginTabItem("Graphics")', source)
        self.assertIn("Restart the Editor to apply this setting", source)
        self.assertIn('ImGui::Button("Restart Now")', source)
        self.assertIn('ImGui::Button("Save & Restart")', source)
        self.assertIn('ImGui::Button("Restart Without Saving")', source)
        self.assertIn("session.Dirty()", source)
        self.assertIn("session.Save()", source)
        self.assertIn("CreateProcessW", restart)
        self.assertIn('argument == L"--renderer"', restart)
        self.assertIn("engine.Stop()", source)
        self.assertIn("ImGuiKey_Comma", app)
        self.assertIn("preferences_.Draw(engine, *session_)", app)

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

    def test_editor_session_owns_file_and_play_lifecycle(self):
        header = self.read("PlagC/include/PlagC/Editor/EditorSession.h")
        for required in (
            "NewEmpty(", "Open(", "Save()", "SaveAs(",
            "StartPlay()", "TogglePause()", "RequestStep()", "ConsumeStep()",
            "StopPlay()", "ViewDocument() const",
        ):
            self.assertIn(required, header)

    def test_plagc_target_auto_includes_editor_backend(self):
        premake = self.read("premake5.lua")
        self.assertIn('"PlagC/include/**.h"', premake)
        self.assertIn('"PlagC/src/**.cpp"', premake)


if __name__ == "__main__":
    unittest.main()
