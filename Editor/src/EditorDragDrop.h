#pragma once

#include <imgui.h>
#include <string>

namespace PlagC::Editor::DragDrop {

inline constexpr const char* CookedModel = "PLAGC_COOKED_MODEL";
inline constexpr const char* MaterialAsset = "PLAGC_MATERIAL_ASSET";
inline constexpr const char* Entity = "PLAGC_EDITOR_ENTITY";

std::string TextPayload(const ImGuiPayload& payload);
bool Active(const char* acceptedType) noexcept;
void DrawInlineFeedback(const char* acceptedType,
                        const char* acceptedText,
                        const char* rejectedText);
void DrawRectFeedback(ImVec2 minimum, ImVec2 maximum,
                      const char* acceptedType,
                      const char* acceptedText,
                      const char* rejectedText);

} // namespace PlagC::Editor::DragDrop
