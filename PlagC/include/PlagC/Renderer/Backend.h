#pragma once
#include <stdexcept>
#include <string>
namespace PlagC
{
enum class RendererBackend
{
    Auto,
    OpenGL,
    Vulkan,
    Null
};
struct RendererBackendCapabilities
{
    bool openGL = false, vulkan = false, null = true;
    // Build/adapter support, not a guarantee that the host GPU/driver can initialize.
    bool Supports(RendererBackend backend) const
    {
        switch (backend)
        {
        case RendererBackend::Auto:
            return openGL || vulkan;
        case RendererBackend::OpenGL:
            return openGL;
        case RendererBackend::Vulkan:
            return vulkan;
        case RendererBackend::Null:
            return null;
        }
        return false;
    }
};
inline const char *BackendName(RendererBackend backend)
{
    switch (backend)
    {
    case RendererBackend::Auto:
        return "Auto";
    case RendererBackend::OpenGL:
        return "OpenGL";
    case RendererBackend::Vulkan:
        return "Vulkan";
    case RendererBackend::Null:
        return "Null";
    }
    throw std::invalid_argument("unknown renderer backend");
}
inline RendererBackend ParseRendererBackend(const std::string &name)
{
    if (name == "auto") return RendererBackend::Auto;
    if (name == "opengl") return RendererBackend::OpenGL;
    if (name == "vulkan") return RendererBackend::Vulkan;
    if (name == "null") return RendererBackend::Null;
    throw std::invalid_argument("renderer must be auto, opengl, vulkan or null");
}
// Pure selection policy shared by initialization, tests and a future settings UI.
// Explicit requests never silently fall back. Auto prefers the established OpenGL path.
inline RendererBackend ResolveRendererBackend(RendererBackend requested,
                                              const RendererBackendCapabilities &available,
                                              bool nullMode = false, bool offscreen = false)
{
    BackendName(requested); // Reject invalid enum values before making a selection.
    if (nullMode)
    {
        if (requested != RendererBackend::Auto && requested != RendererBackend::Null)
            throw std::invalid_argument(
                "headless/null mode conflicts with explicit graphics backend");
        requested = RendererBackend::Null;
    }
    if (requested == RendererBackend::Auto)
    {
        if (available.openGL)
            requested = RendererBackend::OpenGL;
        else if (available.vulkan)
            requested = RendererBackend::Vulkan;
        else
            throw std::runtime_error("no graphical renderer available; request Null explicitly");
    }
    if (!available.Supports(requested))
        throw std::runtime_error(std::string(BackendName(requested)) +
                                 " backend is not implemented in this build");
    if (offscreen && requested == RendererBackend::Null)
        throw std::invalid_argument("offscreen rendering requires a graphical backend");
    return requested;
}
} // namespace PlagC
