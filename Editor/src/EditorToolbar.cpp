#include "pgcpch.h"
#include "EditorLayer.h"
#include "EditorPreferences.h"
#include "EditorVisualAssets.h"
#include <PlagC/Application.h>
#include <imgui.h>
#include <algorithm>

namespace PlagC::Editor {
namespace {

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

    if (EditorVisualAssets::IconButton(EditorIcon::Save, "ToolbarSave",
                                       "Save scene (Ctrl+S)", authoring))
        files_.Save(engine, session);
    if (session.Dirty())
    {
        ImGui::SameLine();
        ImGui::TextDisabled("*");
    }

    ImGui::SameLine();
    if (EditorVisualAssets::IconButton(EditorIcon::Undo, "ToolbarUndo",
                                       "Undo (Ctrl+Z)", session.CanUndo()))
    {
        session.Undo();
        inspectorRevision_ = componentRevision_ = 0;
    }
    ImGui::SameLine();
    if (EditorVisualAssets::IconButton(EditorIcon::Redo, "ToolbarRedo",
                                       "Redo (Ctrl+Y)", session.CanRedo()))
    {
        session.Redo();
        inspectorRevision_ = componentRevision_ = 0;
    }

    ToolbarSeparator();

    if (EditorVisualAssets::IconButton(EditorIcon::Translate, "ToolbarTranslate",
                                       "Translate tool (W)", authoring,
                                       viewport_.gizmo == GizmoMode::Translate))
        viewport_.gizmo = GizmoMode::Translate;
    ImGui::SameLine();
    if (EditorVisualAssets::IconButton(EditorIcon::Rotate, "ToolbarRotate",
                                       "Rotate tool (E)", authoring,
                                       viewport_.gizmo == GizmoMode::Rotate))
        viewport_.gizmo = GizmoMode::Rotate;
    ImGui::SameLine();
    if (EditorVisualAssets::IconButton(EditorIcon::Scale, "ToolbarScale",
                                       "Scale tool (R)", authoring,
                                       viewport_.gizmo == GizmoMode::Scale))
        viewport_.gizmo = GizmoMode::Scale;

    ToolbarSeparator();

    const bool local = viewport_.transformSpace == TransformSpace::Local;
    if (EditorVisualAssets::IconButton(local ? EditorIcon::Local : EditorIcon::World,
                                       "ToolbarSpace",
                                       local ? "Local transform space" : "World/global transform space",
                                       authoring, true))
        viewport_.transformSpace = local ? TransformSpace::World : TransformSpace::Local;

    ImGui::SameLine();
    const bool centered = viewport_.pivot == TransformPivot::Center;
    if (EditorVisualAssets::IconButton(centered ? EditorIcon::Center : EditorIcon::Pivot,
                                       "ToolbarPivot",
                                       centered ? "Center transform handle" : "Pivot transform handle",
                                       authoring, true))
        viewport_.pivot = centered ? TransformPivot::Pivot : TransformPivot::Center;

    ImGui::SameLine();
    if (EditorVisualAssets::IconButton(EditorIcon::Snap, "ToolbarSnap",
                                       "Toggle transform snapping", authoring,
                                       viewport_.snapEnabled))
        viewport_.snapEnabled = !viewport_.snapEnabled;

    const PlaySession::State playState = session.PlayState();
    const bool running = playState != PlaySession::State::Stopped;
    const bool paused = playState == PlaySession::State::Paused;
    const float transportStart = std::max(ImGui::GetCursorPosX() + 12.0f,
                                          (ImGui::GetWindowWidth() - 330.0f) * 0.5f);
    ImGui::SameLine();
    ImGui::SetCursorPosX(transportStart);
    const ImVec4 stateColour = playState == PlaySession::State::Playing
        ? ImVec4(0.35f,0.90f,0.45f,1.0f)
        : paused ? ImVec4(1.00f,0.72f,0.24f,1.0f)
                 : ImVec4(0.68f,0.72f,0.78f,1.0f);
    ImGui::TextColored(stateColour, "%s", running ? (paused ? "PAUSED" : "PLAYING") : "EDIT");
    ImGui::SameLine();

    if (EditorVisualAssets::IconButton(EditorIcon::Play, "ToolbarPlay",
                                       "Start Play mode (F5)", !running))
        ExecuteTransport(engine, session, TransportAction::PlayStop);
    ImGui::SameLine();
    if (EditorVisualAssets::IconButton(paused ? EditorIcon::Resume : EditorIcon::Pause,
                                       "ToolbarPauseResume",
                                       paused ? "Resume simulation (F6)" : "Pause simulation (F6)",
                                       running))
        ExecuteTransport(engine, session, TransportAction::PauseResume);
    ImGui::SameLine();
    if (EditorVisualAssets::IconButton(EditorIcon::Step, "ToolbarStep",
                                       "Advance one fixed simulation step (F7)", paused))
        ExecuteTransport(engine, session, TransportAction::Step);
    ImGui::SameLine();
    if (EditorVisualAssets::IconButton(EditorIcon::Stop, "ToolbarStop",
                                       "Stop Play mode (F5)", running))
        ExecuteTransport(engine, session, TransportAction::PlayStop);

    const float settingsX = ImGui::GetWindowWidth() - 40.0f;
    ImGui::SameLine();
    if (ImGui::GetCursorPosX() < settingsX)
        ImGui::SetCursorPosX(settingsX);
    if (EditorVisualAssets::IconButton(EditorIcon::Settings, "ToolbarSettings",
                                       "Editor settings (Ctrl+,)"))
        EditorPreferences::ToggleActive();

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
}

} // namespace PlagC::Editor
