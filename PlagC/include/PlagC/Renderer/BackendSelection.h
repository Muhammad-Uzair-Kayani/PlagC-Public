#pragma once

#include <PlagC/Renderer/Backend.h>
#include <filesystem>
#include <iosfwd>
#include <span>
#include <string_view>
#include <vector>

namespace PlagC
{
// Shared metadata for every concrete renderer that may appear in a startup menu,
// launcher or settings UI. Auto is a selection policy, not a concrete adapter.
struct RendererBackendOption
{
    RendererBackend backend;
    std::string_view id;
    std::string_view displayName;
};

std::span<const RendererBackendOption> RendererBackendCatalog();
std::vector<RendererBackendOption>
AvailableRendererBackends(const RendererBackendCapabilities &available, bool includeNull = false);

// Legacy console surface retained for explicit debug/launcher use. Normal startup is noninteractive.
RendererBackend PromptRendererBackend(const RendererBackendCapabilities &available,
                                      std::istream &input, std::ostream &output);

// Persistent graphical preference shared by startup and the editor settings UI. Missing, malformed,
// Auto and Null values fall back instead of making the editor unbootable.
std::filesystem::path DefaultRendererPreferencePath();
RendererBackend LoadRendererPreference(const std::filesystem::path &path,
                                       RendererBackend fallback = RendererBackend::Auto) noexcept;
void SaveRendererPreference(const std::filesystem::path &path, RendererBackend backend);
} // namespace PlagC
