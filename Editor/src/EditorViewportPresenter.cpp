#include "pgcpch.h"
#include "EditorViewportPresenter.h"
#include "ImGuiRendererBridge.h"
#include <imgui.h>
#include <imgui_internal.h>

namespace PlagC::Editor {

void EditorViewportPresenter::Draw(TargetHandle target)
{
    if (!target || !ImGui::GetCurrentContext())
        return;

    ImGuiWindow* window = ImGui::FindWindowByName("Viewport");
    if (!window || !window->WasActive || window->Collapsed)
        return;

    const ImRect rect = window->WorkRect;
    if (rect.GetWidth() <= 1.0f || rect.GetHeight() <= 1.0f)
        return;

    ImDrawList* drawList = window->DrawList;
    drawList->PushClipRect(rect.Min, rect.Max, true);
    drawList->AddImage(
        static_cast<ImTextureID>(ImGuiRendererBridge::TargetID(target)),
        rect.Min, rect.Max,
        ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f));
    drawList->PopClipRect();
}

} // namespace PlagC::Editor
