#include "pgcpch.h"
#include "EditorLayer.h"
#include "EditorPreferences.h"
#include "EditorVisualAssets.h"
#include <PlagC/Application.h>
#include <PlagC/Renderer/RenderQuality.h>
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <string>

namespace PlagC::Editor {
namespace {

void ToolbarSeparator()
{
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();
}

void DrawRenderingQualityPopup(Engine& engine, const ViewportState& viewport)
{
    if (!ImGui::BeginPopup("Rendering Quality##EditorToolbar"))
        return;

    auto settings = engine.Graphics().Settings();
    bool changed = false;
    changed |= ImGui::Checkbox("HDR + Tone Mapping", &settings.hdr);
    changed |= ImGui::Checkbox("Bloom", &settings.bloom);
    changed |= ImGui::Checkbox("Scene Shadows", &settings.shadows);
    ImGui::SetNextItemWidth(180.0f);
    changed |= ImGui::DragFloat("Exposure", &settings.exposure, 0.02f, 0.1f, 8.0f, "%.2f");
    ImGui::SetNextItemWidth(180.0f);
    changed |= ImGui::DragFloat("Bloom Strength", &settings.bloomStrength, 0.01f, 0.0f, 4.0f, "%.2f");
    ImGui::SetNextItemWidth(180.0f);
    changed |= ImGui::DragFloat("Bloom Threshold", &settings.bloomThreshold, 0.02f, 0.01f, 16.0f, "%.2f");
    if (changed)
    {
        settings.exposure = std::clamp(settings.exposure, 0.1f, 8.0f);
        settings.bloomStrength = std::clamp(settings.bloomStrength, 0.0f, 4.0f);
        settings.bloomThreshold = std::clamp(settings.bloomThreshold, 0.01f, 16.0f);
        engine.Graphics().SetSettings(settings);
    }

    ImGui::Separator();
    const auto environment = engine.Graphics().Environment();
    ImGui::Text("Backend: %s", BackendName(engine.Graphics().Backend()));
    ImGui::Text("Environment: %s", environment.cubemap.empty() ? "none" : environment.cubemap.c_str());
    ImGui::Text("Skybox: %s   IBL reflections: %s",
                environment.skybox ? "on" : "off",
                environment.reflections ? "on" : "off");

    const ImGuiIO& io = ImGui::GetIO();
    const unsigned logicalWidth = std::max(1u, viewport.width);
    const unsigned logicalHeight = std::max(1u, viewport.height);
    const unsigned framebufferWidth = std::max(
        1u, static_cast<unsigned>(std::lround(logicalWidth * io.DisplayFramebufferScale.x)));
    const unsigned framebufferHeight = std::max(
        1u, static_cast<unsigned>(std::lround(logicalHeight * io.DisplayFramebufferScale.y)));

    RenderQualityAuditInput input;
    input.settings = settings;
    input.environment = environment;
    input.backend = engine.Graphics().Backend();
    input.logicalWidth = logicalWidth;
    input.logicalHeight = logicalHeight;
    input.framebufferWidth = framebufferWidth;
    input.framebufferHeight = framebufferHeight;
    input.msaaSamples = 1u; // Current renderer-neutral Editor target baseline.
    input.uiCompositing = true;
    const RenderQualityAudit audit = AuditRenderQuality(input);

    ImGui::Text("Viewport: %ux%u -> %ux%u framebuffer", logicalWidth, logicalHeight,
                framebufferWidth, framebufferHeight);
    ImGui::Text("DPI scale: %.2f x %.2f", audit.dpiScaleX, audit.dpiScaleY);
    ImGui::Text("MSAA: 1x validated baseline");
    ImGui::TextDisabled("Base colour/emissive = sRGB; normal/metal/roughness/AO = linear.");

    if (!audit.errors.empty())
    {
        ImGui::SeparatorText("Quality Errors");
        for (const auto& error : audit.errors)
            ImGui::BulletText("%s", error.c_str());
    }
    if (!audit.notices.empty())
    {
        ImGui::SeparatorText("Validation Notes");
        for (const auto& notice : audit.notices)
            ImGui::BulletText("%s", notice.c_str());
    }

    ImGui::EndPopup();
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

    std::string undoTooltip = "Undo (Ctrl+Z)";
    std::string redoTooltip = "Redo (Ctrl+Y)";
    if (authoring)
    {
        if (!session.Document().UndoLabel().empty())
            undoTooltip = "Undo " + std::string(session.Document().UndoLabel()) + " (Ctrl+Z)";
        if (!session.Document().RedoLabel().empty())
            redoTooltip = "Redo " + std::string(session.Document().RedoLabel()) + " (Ctrl+Y)";
    }

    ImGui::SameLine();
    if (EditorVisualAssets::IconButton(EditorIcon::Undo, "ToolbarUndo",
                                       undoTooltip, session.CanUndo()))
    {
        session.Undo();
        inspectorRevision_ = componentRevision_ = 0;
    }
    ImGui::SameLine();
    if (EditorVisualAssets::IconButton(EditorIcon::Redo, "ToolbarRedo",
                                       redoTooltip, session.CanRedo()))
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

    const float rightControlsWidth = 76.0f;
    const float rightX = ImGui::GetWindowWidth() - rightControlsWidth;
    ImGui::SameLine();
    if (ImGui::GetCursorPosX() < rightX)
        ImGui::SetCursorPosX(rightX);
    if (ImGui::Button("Q##RenderingQuality", ImVec2(30.0f, 26.0f)))
        ImGui::OpenPopup("Rendering Quality##EditorToolbar");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("Rendering quality validation and live HDR/shadow controls");
    DrawRenderingQualityPopup(engine, viewport_);

    ImGui::SameLine();
    if (EditorVisualAssets::IconButton(EditorIcon::Settings, "ToolbarSettings",
                                       "Editor settings (Ctrl+,)"))
        EditorPreferences::ToggleActive();

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
}

} // namespace PlagC::Editor
