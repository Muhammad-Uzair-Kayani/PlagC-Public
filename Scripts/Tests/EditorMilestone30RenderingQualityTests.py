#!/usr/bin/env python3
"""Source-level regressions for editor Milestone 30 Rendering Quality."""

import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


class EditorMilestone30RenderingQualityTests(unittest.TestCase):
    def read(self, relative: str) -> str:
        return (ROOT / relative).read_text(encoding="utf-8")

    def test_pbs_material_surface_and_texture_colour_spaces(self):
        renderer = self.read("PlagC/include/PlagC/Renderer/Renderer.h")
        scene = self.read("PlagC/src/Assets/AssetScene.cpp")
        quality = self.read("PlagC/src/Renderer/RenderQuality.cpp")
        for field in (
            "baseColourTexture", "normalTexture", "metallicTexture",
            "roughnessTexture", "occlusionTexture", "emissiveTexture",
            "roughness", "metallic", "normalScale", "alphaCutoff", "doubleSided",
        ):
            self.assertIn(field, renderer)
        self.assertIn("d.baseColourTexture = texture(m.baseColourTexture, true);", scene)
        self.assertIn("d.normalTexture = texture(m.normalTexture, false);", scene)
        self.assertIn("d.metallicTexture = texture(m.metallicTexture, false);", scene)
        self.assertIn("d.roughnessTexture = texture(m.roughnessTexture, false);", scene)
        self.assertIn("d.occlusionTexture = texture(m.occlusionTexture, false);", scene)
        self.assertIn("d.emissiveTexture = texture(m.emissiveTexture, true);", scene)
        self.assertIn("MaterialTextureSemantic::BaseColour", quality)
        self.assertIn("MaterialTextureSemantic::Emissive", quality)
        self.assertIn("TextureColourSpace::Srgb", quality)
        self.assertIn("TextureColourSpace::Linear", quality)

    def test_hdr_tone_mapping_bloom_shadows_and_environment_are_real_renderer_paths(self):
        renderer = self.read("PlagC/src/Renderer/OgreRenderer.cpp")
        self.assertIn('addTextureDefinition("HDRScene")', renderer)
        self.assertIn("Ogre::PFG_RGBA16_FLOAT", renderer)
        self.assertIn('tone->mMaterialName = "PlagC/ToneMap"', renderer)
        self.assertIn('"exposure", settings.exposure', renderer)
        self.assertIn('"bloomStrength", settings.bloom ? settings.bloomStrength : 0.f', renderer)
        self.assertIn('settings.shadows ? Ogre::IdString("PlagCShadow")', renderer)
        self.assertIn("environmentTexture && environment.skybox", renderer)
        self.assertIn("desc.reflections ? next : nullptr", renderer)

    def test_quality_surface_exposes_live_renderer_controls(self):
        toolbar = self.read("Editor/src/EditorToolbar.cpp")
        self.assertIn('ImGui::Checkbox("HDR + Tone Mapping", &settings.hdr)', toolbar)
        self.assertIn('ImGui::Checkbox("Bloom", &settings.bloom)', toolbar)
        self.assertIn('ImGui::Checkbox("Scene Shadows", &settings.shadows)', toolbar)
        self.assertIn('ImGui::DragFloat("Exposure"', toolbar)
        self.assertIn("engine.Graphics().SetSettings(settings)", toolbar)
        self.assertIn("engine.Graphics().Environment()", toolbar)
        self.assertIn("AuditRenderQuality(input)", toolbar)

    def test_msaa_validation_is_explicit_and_does_not_make_a_false_claim(self):
        header = self.read("PlagC/include/PlagC/Renderer/RenderQuality.h")
        source = self.read("PlagC/src/Renderer/RenderQuality.cpp")
        toolbar = self.read("Editor/src/EditorToolbar.cpp")
        self.assertIn("bool ValidMsaaSampleCount(unsigned samples) noexcept;", header)
        for samples in ("1u", "2u", "4u", "8u"):
            self.assertIn(samples, source)
        self.assertIn("input.msaaSamples = 1u", toolbar)
        self.assertIn('ImGui::Text("MSAA: 1x validated baseline")', toolbar)
        self.assertIn("no multisampling is being claimed", source)

    def test_viewport_resize_dpi_and_ui_compositing_use_framebuffer_pixels(self):
        app = self.read("Editor/src/EditorApp.cpp")
        presenter = self.read("Editor/src/EditorViewportPresenter.cpp")
        bridge = self.read("Editor/src/ImGuiRendererBridge.cpp")
        self.assertIn("io.DisplayFramebufferScale", app)
        self.assertIn("viewport.width * io.DisplayFramebufferScale.x", app)
        self.assertIn("viewport.height * io.DisplayFramebufferScale.y", app)
        self.assertIn("viewportTarget_->Resize", app)
        self.assertIn("AddImage", presenter)
        self.assertIn("SubmitUI", bridge)

    def test_opengl_and_vulkan_share_renderer_and_editor_quality_contracts(self):
        renderer = self.read("PlagC/src/Renderer/OgreRenderer.cpp")
        premake = self.read("premake5.lua")
        vulkan = self.read("Build/Vulkan.lua")
        self.assertIn("Ogre::GL3PlusPlugin", renderer)
        self.assertIn("Ogre::VulkanPlugin", renderer)
        self.assertIn("RendererBackend::OpenGL", renderer)
        self.assertIn("RendererBackend::Vulkan", renderer)
        self.assertIn("vulkanEngineSources()", premake)
        self.assertIn("ogreLinks()", premake)
        self.assertIn("PC_OGRE_VULKAN", vulkan)

    def test_quality_audit_rejects_invalid_numeric_and_dpi_state(self):
        source = self.read("PlagC/src/Renderer/RenderQuality.cpp")
        self.assertIn("Exposure must be finite and positive", source)
        self.assertIn("Bloom strength must be finite and non-negative", source)
        self.assertIn("Fog density must be finite and non-negative", source)
        self.assertIn("Viewport and framebuffer dimensions must be non-zero", source)
        self.assertIn("Framebuffer DPI scale must be finite and positive", source)
        self.assertIn("Editor UI compositing is not connected", source)

    def test_validation_wiring(self):
        validate = self.read("ValidateEditor.bat")
        workflow = self.read(".github/workflows/editor-windows.yml")
        self.assertIn("EditorMilestone30RenderingQualityTests.py", validate)
        self.assertIn("EditorMilestone30RenderingQualityTests.py", workflow)


if __name__ == "__main__":
    unittest.main()
