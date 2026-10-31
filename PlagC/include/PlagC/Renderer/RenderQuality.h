#pragma once

#include <PlagC/Renderer/Renderer.h>
#include <string>
#include <vector>

namespace PlagC {

enum class MaterialTextureSemantic {
    BaseColour,
    Normal,
    Metallic,
    Roughness,
    Occlusion,
    Emissive
};

enum class TextureColourSpace {
    Linear,
    Srgb
};

TextureColourSpace RequiredColourSpace(MaterialTextureSemantic) noexcept;
bool UsesSrgb(MaterialTextureSemantic) noexcept;
bool ValidMsaaSampleCount(unsigned samples) noexcept;

struct RenderQualityAuditInput {
    RenderSettings settings{};
    EnvironmentDesc environment{};
    RendererBackend backend = RendererBackend::Auto;
    unsigned logicalWidth = 1;
    unsigned logicalHeight = 1;
    unsigned framebufferWidth = 1;
    unsigned framebufferHeight = 1;
    // The current Editor viewport target is intentionally single-sampled. Keeping this
    // explicit prevents the UI from claiming multisampling that the renderer did not request.
    unsigned msaaSamples = 1;
    bool uiCompositing = true;
};

struct RenderQualityAudit {
    bool valid = true;
    float dpiScaleX = 1.0f;
    float dpiScaleY = 1.0f;
    std::vector<std::string> notices;
    std::vector<std::string> errors;
};

RenderQualityAudit AuditRenderQuality(const RenderQualityAuditInput&);

} // namespace PlagC
