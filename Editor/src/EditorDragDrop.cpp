#include "pgcpch.h"
#include "EditorDragDrop.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace PlagC::Editor::DragDrop {

std::string TextPayload(const ImGuiPayload& payload)
{
    if (!payload.Data || payload.DataSize <= 1)
        throw std::invalid_argument("drag payload is empty");

    const auto* data = static_cast<const char*>(payload.Data);
    const std::size_t size = static_cast<std::size_t>(payload.DataSize);
    if (data[size - 1] != '\0')
        throw std::invalid_argument("drag payload text is not null terminated");

    const auto firstNull = std::find(data, data + size - 1, '\0');
    if (firstNull != data + size - 1)
        throw std::invalid_argument("drag payload contains embedded null bytes");

    return std::string(data, size - 1);
}

std::uint64_t EntityIDPayload(const ImGuiPayload& payload)
{
    if (!payload.Data)
        throw std::invalid_argument("entity drag payload is empty");
    if (payload.DataSize != static_cast<int>(sizeof(std::uint64_t)))
        throw std::invalid_argument("entity drag payload has an invalid size");

    std::uint64_t id = 0;
    std::memcpy(&id, payload.Data, sizeof(id));
    if (!id)
        throw std::invalid_argument("entity drag payload contains an invalid persistent id");
    return id;
}

bool Active(const char* acceptedType) noexcept
{
    const ImGuiPayload* payload = ImGui::GetDragDropPayload();
    return payload && acceptedType && payload->IsDataType(acceptedType);
}

void DrawInlineFeedback(bool accepted,
                        const char* acceptedText,
                        const char* rejectedText)
{
    if (!ImGui::GetDragDropPayload())
        return;

    const ImVec4 colour = accepted
        ? ImVec4{0.34f, 0.86f, 0.48f, 1.0f}
        : ImVec4{0.95f, 0.42f, 0.38f, 1.0f};
    ImGui::TextColored(colour, "%s %s", accepted ? "[DROP]" : "[NO]",
                       accepted ? acceptedText : rejectedText);
}

void DrawInlineFeedback(const char* acceptedType,
                        const char* acceptedText,
                        const char* rejectedText)
{
    const ImGuiPayload* payload = ImGui::GetDragDropPayload();
    if (!payload)
        return;
    DrawInlineFeedback(acceptedType && payload->IsDataType(acceptedType),
                       acceptedText, rejectedText);
}

void DrawRectFeedback(ImVec2 minimum, ImVec2 maximum,
                      bool accepted,
                      const char* acceptedText,
                      const char* rejectedText)
{
    if (!ImGui::GetDragDropPayload())
        return;

    const ImU32 colour = accepted
        ? IM_COL32(72, 220, 122, 230)
        : IM_COL32(235, 82, 76, 230);
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    draw->AddRect(minimum, maximum, colour, 4.0f, 0, 2.5f);

    const char* text = accepted ? acceptedText : rejectedText;
    const ImVec2 textSize = ImGui::CalcTextSize(text);
    const ImVec2 padding{8.0f, 5.0f};
    const ImVec2 boxMin{minimum.x + 10.0f, minimum.y + 10.0f};
    const ImVec2 boxMax{boxMin.x + textSize.x + padding.x * 2.0f,
                        boxMin.y + textSize.y + padding.y * 2.0f};
    draw->AddRectFilled(boxMin, boxMax, IM_COL32(20, 24, 31, 235), 4.0f);
    draw->AddRect(boxMin, boxMax, colour, 4.0f);
    draw->AddText({boxMin.x + padding.x, boxMin.y + padding.y}, colour, text);
}

void DrawRectFeedback(ImVec2 minimum, ImVec2 maximum,
                      const char* acceptedType,
                      const char* acceptedText,
                      const char* rejectedText)
{
    const ImGuiPayload* payload = ImGui::GetDragDropPayload();
    if (!payload)
        return;
    DrawRectFeedback(minimum, maximum,
                     acceptedType && payload->IsDataType(acceptedType),
                     acceptedText, rejectedText);
}

} // namespace PlagC::Editor::DragDrop
