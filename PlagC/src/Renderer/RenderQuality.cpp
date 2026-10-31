#include "pgcpch.h"
#include <PlagC/Renderer/RenderQuality.h>
#include <cmath>

namespace PlagC {

TextureColourSpace RequiredColourSpace(MaterialTextureSemantic semantic) noexcept
{
    switch (semantic)
    {
    case MaterialTextureSemantic::BaseColour:
    case MaterialTextureSemantic::Emissive:
        return TextureColourSpace::Srgb;
    case MaterialTextureSemantic::Normal:
    case MaterialTextureSemantic::Metallic:
    case MaterialTextureSemantic::Roughness:
    case MaterialTextureSemantic::Occlusion:
        return TextureColourSpace::Linear;
    }
    return TextureColourSpace::Linear;
}

bool UsesSrgb(MaterialTextureSemantic semantic) noexcept
{
    return RequiredColourSpace(semantic) == TextureColourSpace::Srgb;
}

bool ValidMsaaSampleCount(unsigned samples) noexcept
{
    return samples == 1u || samples == 2u || samples == 4u || samples == 8u;
}

RenderQualityAudit AuditRenderQuality(const RenderQualityAuditInput& input)
{
    RenderQualityAudit result;
    auto fail = [&](std::string text) {
        result.valid = false;
        result.errors.push_back(std::move(text));
    };

    if (!std::isfinite(input.settings.exposure) || input.settings.exposure <= 0.0f)
        fail("Exposure must be finite and positive.");
    if (!std::isfinite(input.settings.bloomStrength) || input.settings.bloomStrength < 0.0f)
        fail("Bloom strength must be finite and non-negative.");
    if (!std::isfinite(input.settings.bloomThreshold) || input.settings.bloomThreshold <= 0.0f)
        fail("Bloom threshold must be finite and positive.");
    if (!std::isfinite(input.settings.fogDensity) || input.settings.fogDensity < 0.0f)
        fail("Fog density must be finite and non-negative.");

    if (!input.logicalWidth || !input.logicalHeight ||
        !input.framebufferWidth || !input.framebufferHeight)
    {
        fail("Viewport and framebuffer dimensions must be non-zero while rendering.");
    }
    else
    {
        result.dpiScaleX = static_cast<float>(input.framebufferWidth) /
                           static_cast<float>(input.logicalWidth);
        result.dpiScaleY = static_cast<float>(input.framebufferHeight) /
                           static_cast<float>(input.logicalHeight);
        if (!std::isfinite(result.dpiScaleX) || !std::isfinite(result.dpiScaleY) ||
            result.dpiScaleX <= 0.0f || result.dpiScaleY <= 0.0f)
            fail("Framebuffer DPI scale must be finite and positive.");
    }

    if (!ValidMsaaSampleCount(input.msaaSamples))
        fail("MSAA sample count must be one of 1, 2, 4 or 8.");
    else if (input.msaaSamples == 1u)
        result.notices.emplace_back(
            "Editor viewport target is single-sampled (1x MSAA baseline); no multisampling is being claimed.");
    else
        result.notices.emplace_back(
            "Multisampling is requested; backend capability must be confirmed by the native renderer.");

    if (input.settings.bloom && !input.settings.hdr)
        result.notices.emplace_back("Bloom is enabled but only participates in the HDR compositor path.");

    if ((input.environment.skybox || input.environment.reflections) && input.environment.cubemap.empty())
        result.notices.emplace_back("Skybox/reflections are enabled without an environment cubemap.");

    if (input.backend == RendererBackend::Null &&
        (input.settings.hdr || input.environment.skybox || input.environment.reflections))
        result.notices.emplace_back(
            "NULL renderer validates lifecycle only; graphical HDR/environment output is not presented.");

    if (!input.uiCompositing)
        fail("Editor UI compositing is not connected to the renderer submission path.");

    result.notices.emplace_back(
        "PBS colour policy: base-colour/emissive maps are sRGB; normal/metallic/roughness/AO maps are linear.");
    return result;
}

} // namespace PlagC
