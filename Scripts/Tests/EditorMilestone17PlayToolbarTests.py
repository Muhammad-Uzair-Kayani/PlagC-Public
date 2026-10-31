#!/usr/bin/env python3
"""Source-level regressions for editor Milestone 17 Play Toolbar."""

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class EditorMilestone17PlayToolbarTests(unittest.TestCase):
    def read(self, relative: str) -> str:
        return (ROOT / relative).read_text(encoding="utf-8")

    def test_transport_is_always_visible_and_state_driven(self):
        source = self.read("Editor/src/EditorLayer.cpp")
        header = self.read("Editor/src/EditorLayer.h")
        self.assertIn("enum class TransportAction", header)
        self.assertIn("void HandleTransportShortcuts", header)
        self.assertIn("void ExecuteTransport", header)
        self.assertIn('ImGui::Button("Play##Transport")', source)
        self.assertIn('paused ? "Resume##Transport" : "Pause##Transport"', source)
        self.assertIn('ImGui::Button("Step##Transport")', source)
        self.assertIn('ImGui::Button("Stop##Transport")', source)
        self.assertIn('"PLAYING"', source)
        self.assertIn('"PAUSED"', source)
        self.assertIn('"EDIT"', source)
        self.assertIn("ImGui::BeginDisabled(running)", source)
        self.assertIn("ImGui::BeginDisabled(!paused)", source)
        self.assertIn("ImGui::BeginDisabled(!running)", source)

    def test_transport_shortcuts_are_context_safe(self):
        source = self.read("Editor/src/EditorLayer.cpp")
        self.assertIn("HandleTransportShortcuts(engine, session);", source)
        self.assertIn("io.WantTextInput || ImGui::IsAnyItemActive()", source)
        self.assertIn("ImGui::IsPopupOpen(nullptr", source)
        self.assertIn("ImGuiKey_F5", source)
        self.assertIn("ImGuiKey_F6", source)
        self.assertIn("ImGuiKey_F7", source)
        self.assertIn("TransportAction::PlayStop", source)
        self.assertIn("TransportAction::PauseResume", source)
        self.assertIn("TransportAction::Step", source)

    def test_play_starts_from_committed_snapshot_and_stop_restores_authoring(self):
        layer = self.read("Editor/src/EditorLayer.cpp")
        session = self.read("PlagC/src/Editor/EditorSession.cpp")
        play = self.read("PlagC/src/Editor/PlaySession.cpp")
        self.assertIn("transformInspector_.Finish(engine, true);", layer)
        self.assertIn("FinishGizmoEdit(engine, true);", layer)
        self.assertIn("session.StartPlay();", layer)
        self.assertIn("session.StopPlay();", layer)
        self.assertIn("play_.Start(*document_);", session)
        self.assertIn("return play_.Running() ? play_.RuntimeDocument() : *document_;", session)
        self.assertIn("RequireStopped();", session)
        self.assertIn("auto clone = authoring.CloneForRuntime();", play)
        self.assertIn("runtime_.reset();", play)

    def test_runtime_state_is_visible_above_scene_view_and_authoring_is_locked(self):
        layer = self.read("Editor/src/EditorLayer.cpp")
        overlay = self.read("Editor/src/EditorViewportAssetDrop.cpp")
        app = self.read("Editor/src/EditorApp.cpp")
        session = self.read("PlagC/src/Editor/EditorSession.cpp")
        self.assertIn('"PLAYING  |  Authoring locked', overlay)
        self.assertIn('"PAUSED  |  Authoring locked', overlay)
        self.assertIn("DrawPlayStateOverlay(session,rect,*window->DrawList);", overlay)
        self.assertIn("draw.AddRect(rect.Min,rect.Max,accent", overlay)
        self.assertLess(app.index("viewportPresenter_.Draw(viewportTarget_->Handle());"),
                        app.index("layer_.AppendViewportAssetDrop(engine, *session_, editorCamera_);"))
        self.assertIn("return !play_.Running() && !document_->HasTransientEdit();", session)
        self.assertIn('"Editor authoring operation is disabled during Play"', session)
        self.assertIn("session.Playing()", layer)

    def test_runtime_camera_physics_animation_and_step_remain_existing_runtime_contracts(self):
        app = self.read("Editor/src/EditorApp.cpp")
        runtime = self.read("Editor/src/EditorPlayRuntime.cpp")
        self.assertIn("runtimeCamera_.Sync(session_->SystemWorld(), engine.Graphics())", app)
        self.assertIn("presentation = playRuntime_.Update(engine, *session_, dt)", app)
        self.assertIn("assetScene_->Update(session_->SystemWorld()", app)
        self.assertIn("playRuntime_.CompleteFrame(engine, *session_)", app)
        self.assertIn("session.Paused() && !session.ConsumeStep()", runtime)
        self.assertIn("physics_->Advance(simulationDelta_,", runtime)
        self.assertIn("gameplay_.FixedUpdate(context, fixedSeconds)", runtime)

    def test_validation_and_roadmap_wiring(self):
        validate = self.read("ValidateEditor.bat")
        workflow = self.read(".github/workflows/editor-windows.yml")
        docs = self.read("Docs/Editor.md")
        self.assertIn("EditorMilestone17PlayToolbarTests.py", validate)
        self.assertIn("EditorMilestone17PlayToolbarTests.py", workflow)
        self.assertIn("## 17. Play Toolbar — `DONE`", docs)


if __name__ == "__main__":
    unittest.main()
