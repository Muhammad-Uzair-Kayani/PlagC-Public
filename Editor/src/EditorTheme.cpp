#include "pgcpch.h"
#include "EditorTheme.h"
#include "EditorVisualAssets.h"
#include <imgui.h>

namespace PlagC::Editor {
namespace {

constexpr ImVec4 Colour(float r, float g, float b, float a = 1.0f)
{
    return ImVec4(r, g, b, a);
}

constexpr ImVec4 kWindow      = Colour(0.052f, 0.059f, 0.071f);
constexpr ImVec4 kPanel       = Colour(0.067f, 0.076f, 0.091f);
constexpr ImVec4 kRaised      = Colour(0.088f, 0.101f, 0.121f);
constexpr ImVec4 kRaisedHover = Colour(0.115f, 0.135f, 0.163f);
constexpr ImVec4 kBorder      = Colour(0.175f, 0.196f, 0.230f, 0.82f);
constexpr ImVec4 kText        = Colour(0.885f, 0.905f, 0.930f);
constexpr ImVec4 kMuted       = Colour(0.500f, 0.535f, 0.585f);
constexpr ImVec4 kAccent      = Colour(0.180f, 0.470f, 0.820f);
constexpr ImVec4 kAccentHover = Colour(0.235f, 0.555f, 0.940f);
constexpr ImVec4 kAccentDown  = Colour(0.145f, 0.385f, 0.690f);
constexpr ImVec4 kSelection   = Colour(0.175f, 0.390f, 0.670f, 0.82f);
constexpr ImVec4 kWarning     = Colour(0.955f, 0.650f, 0.240f);
constexpr ImVec4 kError       = Colour(0.920f, 0.315f, 0.315f);

} // namespace

ImVec4 EditorTheme::Accent() noexcept { return kAccent; }
ImVec4 EditorTheme::AccentHover() noexcept { return kAccentHover; }
ImVec4 EditorTheme::Panel() noexcept { return kPanel; }
ImVec4 EditorTheme::PanelRaised() noexcept { return kRaised; }
ImVec4 EditorTheme::MutedText() noexcept { return kMuted; }
ImVec4 EditorTheme::Warning() noexcept { return kWarning; }
ImVec4 EditorTheme::Error() noexcept { return kError; }

void EditorTheme::Apply()
{
    if (!ImGui::GetCurrentContext())
        return;

    ImGuiStyle& style = ImGui::GetStyle();
    ImGuiIO& io = ImGui::GetIO();

    // Typography stays aligned with M24: the embedded Dear ImGui font is the
    // canonical editor text face, while M23 defines its scale and surrounding rhythm.
    io.FontDefault = EditorVisualAssets::TextFont();
    io.FontGlobalScale = 1.0f;

    style.WindowPadding = ImVec2(10.0f, 9.0f);
    style.FramePadding = ImVec2(7.0f, 4.0f);
    style.CellPadding = ImVec2(7.0f, 5.0f);
    style.ItemSpacing = ImVec2(7.0f, 6.0f);
    style.ItemInnerSpacing = ImVec2(5.0f, 4.0f);
    style.TouchExtraPadding = ImVec2(0.0f, 0.0f);
    style.IndentSpacing = 18.0f;
    style.ScrollbarSize = 13.0f;
    style.GrabMinSize = 9.0f;

    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.TabBorderSize = 0.0f;

    style.WindowRounding = 4.0f;
    style.ChildRounding = 3.0f;
    style.FrameRounding = 3.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 8.0f;
    style.GrabRounding = 3.0f;
    style.TabRounding = 4.0f;

    style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
    style.ButtonTextAlign = ImVec2(0.5f, 0.5f);
    style.SelectableTextAlign = ImVec2(0.0f, 0.5f);
    style.SeparatorTextBorderSize = 1.0f;
    style.SeparatorTextAlign = ImVec2(0.0f, 0.5f);
    style.SeparatorTextPadding = ImVec2(8.0f, 4.0f);

    ImVec4* c = style.Colors;
    c[ImGuiCol_Text] = kText;
    c[ImGuiCol_TextDisabled] = kMuted;
    c[ImGuiCol_WindowBg] = kWindow;
    c[ImGuiCol_ChildBg] = kPanel;
    c[ImGuiCol_PopupBg] = Colour(0.062f, 0.071f, 0.086f, 0.985f);
    c[ImGuiCol_Border] = kBorder;
    c[ImGuiCol_BorderShadow] = Colour(0.0f, 0.0f, 0.0f, 0.0f);

    c[ImGuiCol_FrameBg] = kRaised;
    c[ImGuiCol_FrameBgHovered] = kRaisedHover;
    c[ImGuiCol_FrameBgActive] = Colour(0.130f, 0.155f, 0.190f);
    c[ImGuiCol_TitleBg] = Colour(0.045f, 0.051f, 0.061f);
    c[ImGuiCol_TitleBgActive] = Colour(0.065f, 0.075f, 0.092f);
    c[ImGuiCol_TitleBgCollapsed] = Colour(0.045f, 0.051f, 0.061f, 0.82f);
    c[ImGuiCol_MenuBarBg] = Colour(0.048f, 0.055f, 0.066f);

    c[ImGuiCol_ScrollbarBg] = Colour(0.040f, 0.046f, 0.056f, 0.78f);
    c[ImGuiCol_ScrollbarGrab] = Colour(0.210f, 0.235f, 0.275f);
    c[ImGuiCol_ScrollbarGrabHovered] = Colour(0.275f, 0.310f, 0.365f);
    c[ImGuiCol_ScrollbarGrabActive] = Colour(0.330f, 0.370f, 0.435f);

    c[ImGuiCol_CheckMark] = kAccentHover;
    c[ImGuiCol_SliderGrab] = kAccent;
    c[ImGuiCol_SliderGrabActive] = kAccentHover;
    c[ImGuiCol_Button] = kRaised;
    c[ImGuiCol_ButtonHovered] = Colour(0.155f, 0.205f, 0.275f);
    c[ImGuiCol_ButtonActive] = kAccentDown;

    // Collapsing headers are the Inspector's component-card headers. Keeping
    // their idle state raised and their interaction state accented gives cards
    // a clear hierarchy without introducing a second inspector rendering path.
    c[ImGuiCol_Header] = Colour(0.095f, 0.112f, 0.138f);
    c[ImGuiCol_HeaderHovered] = Colour(0.145f, 0.205f, 0.285f);
    c[ImGuiCol_HeaderActive] = kSelection;

    c[ImGuiCol_Separator] = Colour(0.165f, 0.185f, 0.215f);
    c[ImGuiCol_SeparatorHovered] = kAccent;
    c[ImGuiCol_SeparatorActive] = kAccentHover;
    c[ImGuiCol_ResizeGrip] = Colour(kAccent.x, kAccent.y, kAccent.z, 0.20f);
    c[ImGuiCol_ResizeGripHovered] = Colour(kAccent.x, kAccent.y, kAccent.z, 0.55f);
    c[ImGuiCol_ResizeGripActive] = kAccent;

    c[ImGuiCol_Tab] = Colour(0.070f, 0.080f, 0.096f);
    c[ImGuiCol_TabHovered] = Colour(kAccent.x, kAccent.y, kAccent.z, 0.68f);
    c[ImGuiCol_TabSelected] = Colour(0.120f, 0.250f, 0.410f);
    c[ImGuiCol_TabSelectedOverline] = kAccentHover;
    c[ImGuiCol_TabDimmed] = Colour(0.050f, 0.058f, 0.070f);
    c[ImGuiCol_TabDimmedSelected] = Colour(0.080f, 0.110f, 0.150f);
    c[ImGuiCol_TabDimmedSelectedOverline] = Colour(kAccent.x, kAccent.y, kAccent.z, 0.45f);

    c[ImGuiCol_DockingPreview] = Colour(kAccent.x, kAccent.y, kAccent.z, 0.62f);
    c[ImGuiCol_DockingEmptyBg] = Colour(0.035f, 0.040f, 0.049f);
    c[ImGuiCol_PlotLines] = kAccentHover;
    c[ImGuiCol_PlotLinesHovered] = Colour(0.380f, 0.700f, 1.0f);
    c[ImGuiCol_TableHeaderBg] = Colour(0.082f, 0.095f, 0.115f);
    c[ImGuiCol_TableBorderStrong] = kBorder;
    c[ImGuiCol_TableBorderLight] = Colour(0.135f, 0.150f, 0.177f, 0.70f);
    c[ImGuiCol_TableRowBg] = Colour(0.0f, 0.0f, 0.0f, 0.0f);
    c[ImGuiCol_TableRowBgAlt] = Colour(1.0f, 1.0f, 1.0f, 0.018f);
    c[ImGuiCol_TextSelectedBg] = Colour(kAccent.x, kAccent.y, kAccent.z, 0.34f);
    c[ImGuiCol_DragDropTarget] = kAccentHover;
    c[ImGuiCol_NavCursor] = kAccentHover;
    c[ImGuiCol_NavWindowingHighlight] = Colour(1.0f, 1.0f, 1.0f, 0.70f);
    c[ImGuiCol_NavWindowingDimBg] = Colour(0.0f, 0.0f, 0.0f, 0.28f);
    c[ImGuiCol_ModalWindowDimBg] = Colour(0.0f, 0.0f, 0.0f, 0.55f);
}

} // namespace PlagC::Editor
