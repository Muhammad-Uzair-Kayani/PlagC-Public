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

    def test_ui_texture_keeps_imgui_target_bit_free(self):
        source = self.read("PlagC/src/Renderer/OgreUiTexture.cpp")
        bridge = self.read("Editor/src/ImGuiRendererBridge.cpp")
        self.assertIn("std::uint64_t{1} << 62", source)
        self.assertIn("std::uint64_t{1} << 63", bridge)
        self.assertIn("PlagC/AssetTexture", source)


if __name__ == "__main__":
    unittest.main()
