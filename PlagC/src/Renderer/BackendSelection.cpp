#include "pgcpch.h"
#include <PlagC/Renderer/BackendSelection.h>
#include <array>
#include <charconv>
#include <cctype>
#include <fstream>
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

std::string Trim(std::string text)
{
    const auto notSpace = [](unsigned char value) { return !std::isspace(value); };
    const auto first = std::find_if(text.begin(), text.end(), notSpace);
    const auto last = std::find_if(text.rbegin(), text.rend(), notSpace).base();
    if (first >= last)
        return {};
    return std::string(first, last);
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

std::filesystem::path DefaultRendererPreferencePath()
{
    return std::filesystem::path("Saved") / "RendererBackend.txt";
}

RendererBackend LoadRendererPreference(const std::filesystem::path &path,
                                       RendererBackend fallback) noexcept
{
    try
    {
        std::ifstream input(path);
        if (!input)
            return fallback;

        std::string text;
        std::getline(input, text);
        text = Trim(std::move(text));
        if (text.empty())
            return fallback;

        const auto backend = ParseRendererBackend(text);
        return backend == RendererBackend::OpenGL || backend == RendererBackend::Vulkan
                   ? backend
                   : fallback;
    }
    catch (...)
    {
        return fallback;
    }
}

void SaveRendererPreference(const std::filesystem::path &path, RendererBackend backend)
{
    if (backend != RendererBackend::OpenGL && backend != RendererBackend::Vulkan)
        throw std::invalid_argument("renderer preference must be OpenGL or Vulkan");

    if (const auto parent = path.parent_path(); !parent.empty())
        std::filesystem::create_directories(parent);

    std::ofstream output(path, std::ios::trunc);
    if (!output)
        throw std::runtime_error("failed to open renderer preference for writing: " + path.string());

    output << (backend == RendererBackend::Vulkan ? "vulkan\n" : "opengl\n");
    if (!output)
        throw std::runtime_error("failed to write renderer preference: " + path.string());
}
} // namespace PlagC
