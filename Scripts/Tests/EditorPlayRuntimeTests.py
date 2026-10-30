#!/usr/bin/env python3
"""Source-level regressions for editor Play-mode runtime orchestration."""

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class EditorPlayRuntimeTests(unittest.TestCase):
    def read(self, relative: str) -> str:
        return (ROOT / relative).read_text(encoding="utf-8")

    def test_session_exposes_runtime_generation(self):
        header = self.read("PlagC/include/PlagC/Editor/EditorSession.h")
        source = self.read("PlagC/src/Editor/EditorSession.cpp")
        self.assertIn("std::uint64_t PlayGeneration() const;", header)
        self.assertIn("return play_.Generation();", source)

    def test_play_runtime_owns_physics_for_isolated_clone(self):
        header = self.read("Editor/src/EditorPlayRuntime.h")
        source = self.read("Editor/src/EditorPlayRuntime.cpp")
        self.assertIn("std::unique_ptr<PhysicsSystem> physics_", header)
        self.assertIn("session.RuntimeWorld()", source)
        self.assertIn("session.PlayGeneration()", source)
        self.assertIn("generation_ != generation", source)
        self.assertIn("physicsConfig_.sharedJobs = useSharedJobs_ ? &engine.Jobs() : nullptr", source)

    def test_pause_and_step_drive_fixed_physics_without_authoring_mutation(self):
        source = self.read("Editor/src/EditorPlayRuntime.cpp")
        self.assertIn("if (session.Paused())", source)
        self.assertIn("session.ConsumeStep()", source)
        self.assertIn("physics_->Advance(1.0 / physicsConfig_.fixedHz)", source)
        self.assertIn("physics_->Synchronize()", source)
        self.assertIn("physics_->Advance(frameSeconds)", source)
        self.assertIn("return physics_->Presentation();", source)
        self.assertNotIn("session.EditDocument()", source)

    def test_editor_feeds_physics_presentation_to_scene_renderer(self):
        app = self.read("Editor/src/EditorApp.cpp")
        self.assertIn("presentation = playRuntime_.Update(engine, *session_, dt)", app)
        self.assertIn("sceneRenderer_->Sync(session_->ViewWorld(), presentation)", app)
        self.assertIn('"EDITOR-PLAY"', app)
        self.assertIn("session_->StopPlay()", app)
        self.assertIn("playRuntime_.Reset()", app)
        self.assertIn("std::make_unique<EditorApplication>(config)", app)


if __name__ == "__main__":
    unittest.main()
