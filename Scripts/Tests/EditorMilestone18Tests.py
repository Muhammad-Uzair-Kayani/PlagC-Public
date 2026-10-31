#!/usr/bin/env python3
"""Focused source regressions for Milestone 18 Runtime / Game Logic."""

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class EditorMilestone18Tests(unittest.TestCase):
    def read(self, relative: str) -> str:
        return (ROOT / relative).read_text(encoding="utf-8")

    def test_engine_owned_gameplay_contract_has_full_lifecycle(self):
        header = self.read("PlagC/include/PlagC/Gameplay/GameplayRuntime.h")
        for hook in (
            "OnStart(GameplayContext&)",
            "OnStop(Engine&)",
            "OnPause(GameplayContext&)",
            "OnResume(GameplayContext&)",
            "OnInput(GameplayContext&, const Event&)",
            "OnUpdate(GameplayContext&, double)",
            "OnFixedUpdate(GameplayContext&, double)",
            "OnTrigger(GameplayContext&, Entity sensor, Entity other, TriggerPhase)",
            "OnCollision(GameplayContext&, Entity a, Entity b, CollisionPhase)",
            "OnLateUpdate(GameplayContext&, double)",
        ):
            self.assertIn(hook, header)
        self.assertIn("World& world", header)
        self.assertIn("PhysicsSystem& physics", header)
        self.assertIn("const InputState& input", header)

    def test_game_input_is_separate_and_capture_gated(self):
        input_header = self.read("PlagC/include/PlagC/Input/Input.h")
        gameplay = self.read("PlagC/src/Gameplay/GameplayRuntime.cpp")
        app = self.read("Editor/src/EditorApp.cpp")
        self.assertIn("void ResetKeyboard();", input_header)
        self.assertIn("void ResetMouse();", input_header)
        self.assertIn("events.Subscribe(type", gameplay)
        self.assertIn("keyboardInputEnabled_", gameplay)
        self.assertIn("mouseInputEnabled_", gameplay)
        self.assertIn("input_.ResetKeyboard()", gameplay)
        self.assertIn("input_.ResetMouse()", gameplay)
        self.assertIn("event.type == EventType::Focus", gameplay)
        self.assertIn("playRuntime_.SetGameInputRouting(", app)
        self.assertIn("session_->Playing() && !session_->Paused()", app)
        self.assertIn("viewport.input.viewportFocused", app)
        self.assertIn("viewport.input.viewportHovered", app)
        self.assertIn("io.WantTextInput", app)
        self.assertIn("popupBlocksGame", app)

    def test_fixed_update_uses_physics_scheduler_and_pause_step_is_exact(self):
        runtime = self.read("Editor/src/EditorPlayRuntime.cpp")
        dispatch = runtime.index("gameplay_.DispatchInput(context)")
        update = runtime.index("gameplay_.Update(context, simulationDelta_)")
        advance = runtime.index("physics_->Advance(simulationDelta_,")
        fixed = runtime.index("gameplay_.FixedUpdate(context, fixedSeconds)", advance)
        physics_events = runtime.index("gameplay_.DispatchPhysicsEvents(context)")
        self.assertLess(dispatch, update)
        self.assertLess(update, advance)
        self.assertLess(advance, fixed)
        self.assertLess(fixed, physics_events)
        self.assertIn("session.Paused() && !session.ConsumeStep()", runtime)
        self.assertIn("physics_->Synchronize()", runtime)
        self.assertIn("1.0 / physicsConfig_.fixedHz", runtime)

    def test_trigger_and_collision_events_route_by_stable_entities(self):
        types = self.read("PlagC/include/PlagC/Physics/PhysicsTypes.h")
        system_header = self.read("PlagC/include/PlagC/Physics/PhysicsSystem.h")
        contacts = self.read("PlagC/src/Physics/JoltContacts.h")
        system = self.read("PlagC/src/Physics/PhysicsSystem.cpp")
        gameplay = self.read("PlagC/src/Gameplay/GameplayRuntime.cpp")
        self.assertIn("enum class CollisionPhase { Enter,Stay,Exit }", types)
        self.assertIn("struct CollisionEvent", types)
        self.assertIn("std::span<const CollisionEvent> CollisionEvents() const", system_header)
        self.assertIn("PhysicsContactRegistry::Add", contacts)
        self.assertIn("PhysicsContactRegistry::Remove", contacts)
        self.assertIn("void CollectCollisions()", system)
        self.assertIn("CollisionPhase::Enter", system)
        self.assertIn("CollisionPhase::Stay", system)
        self.assertIn("CollisionPhase::Exit", system)
        self.assertIn("context.physics.TriggerEvents()", gameplay)
        self.assertIn("context.physics.CollisionEvents()", gameplay)
        self.assertIn("context.physics.EntityFor", gameplay)
        self.assertIn("system->OnTrigger", gameplay)
        self.assertIn("system->OnCollision", gameplay)

    def test_animation_late_gameplay_and_audio_have_declared_order(self):
        app = self.read("Editor/src/EditorApp.cpp")
        runtime = self.read("Editor/src/EditorPlayRuntime.cpp")
        animation = app.index("assetScene_->Update(session_->SystemWorld()")
        completion = app.index("playRuntime_.CompleteFrame(engine, *session_)")
        self.assertLess(animation, completion)
        late = runtime.index("gameplay_.LateUpdate(context, simulationDelta_)")
        audio = runtime.index("engine.Audio().CollectUnused()")
        self.assertLess(late, audio)
        self.assertIn("session.Playing() && engine.Audio().Available()", runtime)
        self.assertIn("gameplay_.Pause(context)", runtime)
        self.assertIn("gameplay_.Resume(context)", runtime)

    def test_milestone_is_wired_into_validation(self):
        local = self.read("ValidateEditor.bat")
        ci = self.read(".github/workflows/editor-windows.yml")
        docs = self.read("Docs/Editor.md")
        self.assertIn("EditorMilestone18Tests.py", local)
        self.assertIn("EditorMilestone18Tests.py", ci)
        self.assertIn("## 18. Runtime / Game Logic — `DONE`", docs)


if __name__ == "__main__":
    unittest.main()
