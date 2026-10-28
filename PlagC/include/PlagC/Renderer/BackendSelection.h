#pragma once

#include <PlagC/Renderer/Backend.h>
#include <iosfwd>
#include <span>
#include <string_view>
#include <vector>

namespace PlagC
{
// Shared metadata for every concrete renderer that may appear in a startup menu,
// launcher or future settings UI. Auto is a selection policy, not a concrete adapter.
struct RendererBackendOption
{
    RendererBackend backend;
    std::string_view id;
    std::string_view displayName;
};

std::span<const RendererBackendOption> RendererBackendCatalog();
std::vector<RendererBackendOption>
AvailableRendererBackends(const RendererBackendCapabilities &available, bool includeNull = false);

// Console surface used only before Engine creates renderer-owned resources. Keeping the
// selection policy separate from the console lets a future launcher/settings UI consume
// RendererBackendCatalog/AvailableRendererBackends without duplicating backend knowledge.
RendererBackend PromptRendererBackend(const RendererBackendCapabilities &available,
                                      std::istream &input, std::ostream &output);
} // namespace PlagC
