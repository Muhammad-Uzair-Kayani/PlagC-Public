#pragma once

#include <imgui.h>
#include <string_view>

namespace PlagC::Editor {

enum class EditorIcon {
    Entity,
    Folder,
    Asset,
    Model,
    Texture,
    Material,
    Camera,
    Light,
    Physics,
    Save,
    Undo,
    Redo,
    Translate,
    Rotate,
    Scale,
    World,
    Local,
    Pivot,
    Center,
    Snap,
    Play,
    Pause,
    Resume,
    Step,
    Stop,
    Settings
};

// Renderer-neutral editor visual assets. Icons are drawn from ImGui primitives,
// and text uses Dear ImGui's built-in embedded default font. No raw TTF/OTF
// files or renderer-specific icon textures are required at runtime.
class EditorVisualAssets final {
public:
    static ImFont* TextFont() noexcept;
    static const char* AccessibleName(EditorIcon) noexcept;

    static void DrawIcon(ImDrawList*, EditorIcon, ImVec2 center, float size, ImU32 colour);
    static void DrawInlineIcon(EditorIcon, float size = 14.0f,
                               ImU32 colour = IM_COL32(220, 224, 232, 255));

    static bool IconButton(EditorIcon,
                           std::string_view id,
                           std::string_view tooltip,
                           bool enabled = true,
                           bool active = false,
                           ImVec2 size = ImVec2(30.0f, 26.0f));
};

} // namespace PlagC::Editor
