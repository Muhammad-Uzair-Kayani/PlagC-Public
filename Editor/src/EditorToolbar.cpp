#include "pgcpch.h"
#include "EditorLayer.h"
#include "EditorPreferences.h"
#include <PlagC/Application.h>
#include <imgui.h>
#include <algorithm>

namespace PlagC::Editor {
namespace {

bool ToolbarButton(const char* label,
                   const char* tooltip,
                   bool enabled = true,
                   bool active = false,
                   ImVec2 size = ImVec2(30.0f, 26.0f))
{
    ImGui::BeginDisabled(!enabled);
    if (active)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    }

    const bool pressed = ImGui::Button(label, size);

    if (active)
        ImGui::PopStyleColor(2);

    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("%s", tooltip);
    ImGui::EndDisabled();
    return pressed && enabled;
}

void ToolbarSeparator()
{
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
}

} // namespace

void EditorLayer::DrawEditorToolbar(Engine& engine, EditorSession& session)
{
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 4.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f));
    ImGui::BeginChild("##PlagC.Editor.Toolbar", ImVec2(0.0f, 38.0f), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    const bool authoring = session.EditingEnabled();

    // M21 uses compact text/symbol fallbacks so the toolbar is usable before the
    // dedicated embedded icon/font system lands in M24. Every control has a tooltip.
    if (ToolbarButton("S##ToolbarSave", "Save scene (Ctrl+S)", authoring))
        files_.Save(engine, session);
    if (session.Dirty())
    {
        ImGui::SameLine();
        ImGui::TextDisabled("*");
    }

    ImGui::SameLine();
    if (ToolbarButton("<##ToolbarUndo", "Undo (Ctrl+Z)", session.CanUndo()))
    {
        session.Undo();
        inspectorRevision_ = componentRevision_ = 0;
    }
    ImGui::SameLine();
    if (ToolbarButton(">##ToolbarRedo", "Redo (Ctrl+Y)", session.CanRedo()))
    {
        session.Redo();
        inspectorRevision_ = componentRevision_ = 0;
    }

    ToolbarSeparator();

    if (ToolbarButton("W##ToolbarTranslate", "Translate tool (W)", authoring,
                      viewport_.gizmo == GizmoMode::Translate))
        viewport_.gizmo = GizmoMode::Translate;
    ImGui::SameLine();
    if (ToolbarButton("E##ToolbarRotate", "Rotate tool (E)", authoring,
                      viewport_.gizmo == GizmoMode::Rotate))
        viewport_.gizmo = GizmoMode::Rotate;
    ImGui::SameLine();
    if (ToolbarButton("R##ToolbarScale", "Scale tool (R)", authoring,
                      viewport_.gizmo == GizmoMode::Scale))
        viewport_.gizmo = GizmoMode::Scale;

    ToolbarSeparator();

    const bool local = viewport_.transformSpace == TransformSpace::Local;
    if (ToolbarButton(local ? "L##ToolbarSpace" : "G##ToolbarSpace",
                      local ? "Local transform space" : "World/global transform space",
                      authoring, true))
        viewport_.transformSpace = local ? TransformSpace::World : TransformSpace::Local;

    ImGui::SameLine();
    const bool centered = viewport_.pivot == TransformPivot::Center;
    if (ToolbarButton(centered ? "C##ToolbarPivot" : "P##ToolbarPivot",
                      centered ? "Center transform handle" : "Pivot transform handle",
                      authoring, true))
        viewport_.pivot = centered ? TransformPivot::Pivot : TransformPivot::Center;

    ImGui::SameLine();
    if (ToolbarButton("N##ToolbarSnap", "Toggle transform snapping", authoring,
                      viewport_.snapEnabled))
        viewport_.snapEnabled = !viewport_.snapEnabled;

    // Keep the M17 state-driven transport centered in the global toolbar.
    const float transportStart = std::max(ImGui::GetCursorPosX() + 12.0f,
                                          (ImGui::GetWindowWidth() - 430.0f) * 0.5f);
    ImGui::SameLine();
    ImGui::SetCursorPosX(transportStart);
    DrawTransport(engine, session);

    // Preferences remains the existing persistent settings surface; M21 only exposes it.
    const float settingsX = ImGui::GetWindowWidth() - 40.0f;
    ImGui::SameLine();
    if (ImGui::GetCursorPosX() < settingsX)
        ImGui::SetCursorPosX(settingsX);
    if (ToolbarButton("...##ToolbarSettings", "Editor settings (Ctrl+,)", true,
                      false, ImVec2(34.0f, 26.0f)))
        EditorPreferences::ToggleActive();

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
}

} // namespace PlagC::Editor
