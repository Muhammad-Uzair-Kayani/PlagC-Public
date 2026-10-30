#!/usr/bin/env python3
"""Source-level regressions for editor Play-mode runtime orchestration."""

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class EditorPlayRuntimeTests(unittest.TestCase):
    def read(self, relative: str) -> str:
        return (ROOT / relative).read_text(encoding="utf-8")

    def test_session_exposes_runtime_generation_and_system_world(self):
        header = self.read("PlagC/include/PlagC/Editor/EditorSession.h")
        source = self.read("PlagC/src/Editor/EditorSession.cpp")
        document = self.read("PlagC/include/PlagC/Editor/SceneDocument.h")
        self.assertIn("std::uint64_t PlayGeneration() const;", header)
        self.assertIn("World& SystemWorld();", header)
        self.assertIn("return play_.Generation();", source)
        self.assertIn("return play_.Running() ? play_.RuntimeWorld() : *document_->world_;", source)
        self.assertIn("friend class EditorSession;", document)

    def test_play_runtime_owns_physics_for_isolated_clone(self):
        header = self.read("Editor/src/EditorPlayRuntime.h")
        source = self.read("Editor/src/EditorPlayRuntime.cpp")
        self.assertIn("std::unique_ptr<PhysicsSystem> physics_", header)
        self.assertIn("session.RuntimeWorld()", source)
        self.assertIn("session.PlayGeneration()", source)
        self.assertIn("generation_ != generation", source)
        self.assertIn("physicsConfig_.sharedJobs = useSharedJobs_ ? &engine.Jobs() : nullptr", source)

    def test_pause_step_and_animation_share_effective_simulation_delta(self):
        header = self.read("Editor/src/EditorPlayRuntime.h")
        source = self.read("Editor/src/EditorPlayRuntime.cpp")
        app = self.read("Editor/src/EditorApp.cpp")
        self.assertIn("double SimulationDelta() const noexcept", header)
        self.assertIn("simulationDelta_ = 0.0", source)
        self.assertIn("session.Paused() && !session.ConsumeStep()", source)
        self.assertIn("simulationDelta_ = session.Paused() ? (1.0 / physicsConfig_.fixedHz) : frameSeconds", source)
        self.assertIn("physics_->Advance(simulationDelta_,", source)
        self.assertIn("gameplay_.FixedUpdate(context, fixedSeconds)", source)
        self.assertIn("return physics_->Presentation();", source)
        self.assertIn("playRuntime_.SimulationDelta()", app)
        self.assertNotIn("session.EditDocument()", source)

    def test_editor_feeds_physics_presentation_model_animation_and_completion(self):
        app = self.read("Editor/src/EditorApp.cpp")
        self.assertIn("presentation = playRuntime_.Update(engine, *session_, dt)", app)
        self.assertIn("sceneRenderer_->Sync(session_->ViewWorld(), presentation)", app)
        self.assertIn("std::make_unique<PlagC::AssetScene>(engine.Graphics())", app)
        self.assertIn("assetScene_->Update(session_->SystemWorld()", app)
        self.assertIn("session_->Playing() ? playRuntime_.SimulationDelta() : 0.0", app)
        self.assertIn("engine.timings.animationMs = assetScene_->Statistics().animationMs", app)
        self.assertIn("playRuntime_.CompleteFrame(engine, *session_)", app)
        self.assertIn("assetScene_->Clear()", app)
        self.assertIn('"EDITOR-PLAY"', app)
        self.assertIn("session_->StopPlay()", app)
        self.assertIn("playRuntime_.Reset(engine)", app)
        self.assertIn("std::make_unique<EditorApplication>(config)", app)

    def test_play_mode_uses_runtime_active_camera_and_editor_fallback(self):
        app = self.read("Editor/src/EditorApp.cpp")
        self.assertIn("PlagC::CameraSystem runtimeCamera_", app)
        self.assertIn("runtimeCamera_.SetViewport(viewport.width, viewport.height)", app)
        self.assertIn("runtimeCamera_.Sync(session_->SystemWorld(), engine.Graphics())", app)
        self.assertIn("engine.Graphics().SetCamera(editorCamera_)", app)
        self.assertIn("CameraProjection::Perspective", app)

        play = app.index("if (session_->Playing())", app.index("void UpdateEditorCamera"))
        sync = app.index("runtimeCamera_.Sync(session_->SystemWorld(), engine.Graphics())", play)
        leave = app.index("return;", sync)
        movement = app.index("viewport.input.AcceptCameraMouse()", play)
        self.assertLess(sync, leave)
        self.assertLess(leave, movement)


if __name__ == "__main__":
    unittest.main()
