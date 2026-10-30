#!/usr/bin/env python3
"""Regression checks for the standalone immediate-mode UI texture path."""

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class EditorUiTextureTests(unittest.TestCase):
    def read(self, relative: str) -> str:
        return (ROOT / relative).read_text(encoding="utf-8")

    def test_imgui_font_uses_standalone_texture(self):
        bridge = self.read("Editor/src/ImGuiRendererBridge.cpp")
        source = self.read("PlagC/src/Renderer/OgreUiTexture.cpp")
        self.assertIn("CreateUiTexture(renderer", bridge)
        self.assertIn("DestroyUiTexture(renderer, fontTexture_)", bridge)
        self.assertIn("Ogre::TextureTypes::Type2D", source)
        self.assertIn("texture->setResolution(width, height)", source)
        self.assertIn("texture->setPixelFormat(format)", source)
        self.assertIn("manager.waitForStreamingCompletion()", source)
        self.assertNotIn("Ogre::TextureFlags::AutomaticBatching", source)

    def test_ui_texture_contract_is_checked_at_runtime(self):
        source = self.read("PlagC/src/Renderer/OgreUiTexture.cpp")
        self.assertIn("texture->hasAutomaticBatching()", source)
        self.assertIn("texture->getTextureType() != Ogre::TextureTypes::Type2D", source)
        self.assertIn("texture->isDataReady()", source)
        self.assertIn("editor UI texture must be a standalone Ogre Type2D resource", source)
        self.assertIn("editor UI texture upload did not become ready", source)

    def test_ui_texture_creation_is_diagnosable(self):
        source = self.read("PlagC/src/Renderer/OgreUiTexture.cpp")
        self.assertIn("[PlagC UI] standalone texture ready name=", source)
        self.assertIn("type=Type2D batching=false ready=true", source)
        self.assertIn("pixels=", source)
        self.assertIn("handle=", source)

    def test_ui_texture_keeps_imgui_target_bit_free(self):
        source = self.read("PlagC/src/Renderer/OgreUiTexture.cpp")
        bridge = self.read("Editor/src/ImGuiRendererBridge.cpp")
        self.assertIn("std::uint64_t{1} << 62", source)
        self.assertIn("std::uint64_t{1} << 63", source)
        self.assertIn("std::uint64_t{1} << 63", bridge)
        self.assertIn("PlagC/AssetTexture", source)

    def test_bridge_rejects_unknown_low_bit_texture_handles(self):
        bridge = self.read("Editor/src/ImGuiRendererBridge.cpp")
        self.assertIn("out.texture.kind == UiTextureKind::Texture", bridge)
        self.assertIn("out.texture.value != fontTexture_.value", bridge)
        self.assertIn("ImGui submitted an unknown editor UI texture handle", bridge)

    def test_submission_resolves_and_validates_ogre_textures(self):
        submission = self.read("PlagC/src/Renderer/UiSubmission.cpp")
        internal = self.read("PlagC/src/Renderer/UiSubmissionInternal.h")
        texture = self.read("PlagC/src/Renderer/OgreUiTexture.cpp")
        self.assertIn("Detail::ValidateOgreUITextures(data)", submission)
        self.assertIn("void ValidateOgreUITextures(const UiDrawData&);", internal)
        self.assertIn("manager.findTextureNoThrow(Ogre::IdString(name))", texture)
        self.assertIn("[PlagC UI] ERROR: draw texture did not resolve", texture)
        self.assertIn("normal editor UI draws require a ready standalone Ogre Type2D texture", texture)
        self.assertIn("[PlagC UI] resolved draw texture kind=", texture)
        self.assertIn('ref.kind == UiTextureKind::Target ? "Target" : "AssetTexture"', texture)
        self.assertIn('std::string("PlagC/") + kind', texture)
        self.assertIn("std::unordered_set<std::uint64_t> validatedThisFrame", texture)
        self.assertIn("const std::uint64_t key = UiDrawTextureKey(command.texture)", texture)
        self.assertIn("if (!validatedThisFrame.insert(key).second)", texture)

    def test_ui_presentation_requires_an_executed_draw(self):
        backend = self.read("PlagC/src/Renderer/OgreUiBackend.cpp")
        self.assertIn("bool Render(const UiDrawData& frame", backend)
        self.assertIn("if (Render(*frame, target, renderer->Backend()))", backend)
        self.assertIn("presented_.erase(renderer);", backend)
        self.assertIn("drew = true;", backend)
        self.assertIn("return drew;", backend)
        self.assertNotIn(
            "Render(*frame, target, renderer->Backend());\n        presented_.insert(renderer);",
            backend,
        )


if __name__ == "__main__":
    unittest.main()
