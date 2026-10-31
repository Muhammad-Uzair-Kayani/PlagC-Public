#pragma once

#include <imgui.h>

namespace PlagC::Editor {

// Milestone 23 visual language. This is presentation-only state applied to
// Dear ImGui; it owns no renderer resources and does not alter authoring data.
class EditorTheme final
{
public:
    static void Apply();

    static ImVec4 Accent() noexcept;
    static ImVec4 AccentHover() noexcept;
    static ImVec4 Panel() noexcept;
    static ImVec4 PanelRaised() noexcept;
    static ImVec4 MutedText() noexcept;
    static ImVec4 Warning() noexcept;
    static ImVec4 Error() noexcept;
};

} // namespace PlagC::Editor
