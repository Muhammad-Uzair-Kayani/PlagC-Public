#include "pgcpch.h"
#include <PlagC/Renderer/BackendSelection.h>
#include <array>
#include <charconv>
#include <cctype>
#include <istream>
#include <ostream>
#include <string>

namespace PlagC
{
namespace
{
constexpr std::array<RendererBackendOption, 3> BackendCatalog{{
    {RendererBackend::OpenGL, "opengl", "OpenGL"},
    {RendererBackend::Vulkan, "vulkan", "Vulkan"},
    {RendererBackend::Null, "null", "Null"},
}};

bool SameAscii(std::string_view lhs, std::string_view rhs)
{
    if (lhs.size() != rhs.size())
        return false;
    for (std::size_t i = 0; i < lhs.size(); ++i)
    {
        const auto a = static_cast<unsigned char>(lhs[i]);
        const auto b = static_cast<unsigned char>(rhs[i]);
        if (std::tolower(a) != std::tolower(b))
            return false;
    }
    return true;
}
} // namespace

std::span<const RendererBackendOption> RendererBackendCatalog()
{
    return BackendCatalog;
}

std::vector<RendererBackendOption>
AvailableRendererBackends(const RendererBackendCapabilities &available, bool includeNull)
{
    std::vector<RendererBackendOption> result;
    for (const auto &option : BackendCatalog)
    {
        if (option.backend == RendererBackend::Null && !includeNull)
            continue;
        if (available.Supports(option.backend))
            result.push_back(option);
    }
    return result;
}

RendererBackend PromptRendererBackend(const RendererBackendCapabilities &available,
                                      std::istream &input, std::ostream &output)
{
    const auto options = AvailableRendererBackends(available);
    const auto fallback = ResolveRendererBackend(RendererBackend::Auto, available);

    // Do not add a meaningless prompt to builds that contain only one graphics adapter.
    if (options.size() <= 1u)
        return fallback;

    output << "\n=== PlagC Renderer ===\n";
    for (std::size_t i = 0; i < options.size(); ++i)
        output << '[' << (i + 1u) << "] " << options[i].displayName << " (" << options[i].id
               << ")\n";
    output << "Select renderer [Enter = " << BackendName(fallback) << "]: " << std::flush;

    std::string selection;
    if (!std::getline(input, selection) || selection.empty())
        return fallback;

    std::size_t index = 0u;
    const auto numeric =
        std::from_chars(selection.data(), selection.data() + selection.size(), index);
    if (numeric.ec == std::errc{} && numeric.ptr == selection.data() + selection.size() &&
        index >= 1u && index <= options.size())
        return options[index - 1u].backend;

    for (const auto &option : options)
    {
        if (SameAscii(selection, option.id) || SameAscii(selection, option.displayName))
            return option.backend;
    }

    throw std::invalid_argument("invalid renderer selection: " + selection);
}
} // namespace PlagC
