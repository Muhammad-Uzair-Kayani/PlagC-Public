#include "pgcpch.h"
#include "EditorLayer.h"
#include <PlagC/Application.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cfloat>
#include <cstdio>

namespace PlagC::Editor {
namespace {
void LogEditorError(Engine& engine, const std::exception& error)
{
    engine.Logger().Write(Severity::Error, "EDITOR", error.what());
}
} // namespace

void EditorLayer::Initialize(Engine& engine, EditorSession&)
{
    seenDocumentGeneration_ = files_.DocumentGeneration();
    dataPanels_.Initialize(engine);
    engine.Logger().Write(Severity::Info, "EDITOR", "Docked editor workspace initialized.");
}

void EditorLayer::Frame(Engine& engine, EditorSession& session)
{
    const auto& io = ImGui::GetIO();
    HandleTransportShortcuts(engine, session);
    if (session.EditingEnabled() && !io.WantTextInput && !ImGui::IsAnyItemActive() &&
        !io.KeyAlt && !ImGui::IsPopupOpen(nullptr,
            ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
    {
        try
        {
            if (io.KeyCtrl && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z, false) && session.CanUndo())
                session.Undo();
            else if (io.KeyCtrl && ((ImGui::IsKeyPressed(ImGuiKey_Y, false) && !io.KeyShift) ||
                     (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z, false))) && session.CanRedo())
                session.Redo();
            else if (!session.Document().HasTransientEdit())
            {
                auto& document = session.EditDocument();
                const Entity selected = document.Selection();
                const bool locked = selected && hierarchy_.Locked(document.PersistentID(selected));
                if (selected && !locked && io.KeyCtrl && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_D, false))
                    document.Duplicate(selected);
                else if (selected && !locked && !io.KeyCtrl && !io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Delete, false))
                    document.Destroy(selected);
            }
        }
        catch (const std::exception& error) { LogEditorError(engine, error); }
    }

    DrawDockHost(engine, session);

    if (seenDocumentGeneration_ != files_.DocumentGeneration())
    {
        transformInspector_.Reset();
        hierarchy_.Reset();
        seenDocumentGeneration_ = files_.DocumentGeneration();
        inspectorPersistentID_ = 0;
        inspectorRevision_ = 0;
    }

    DrawViewport(session);
    if (showInspector_)
    {
        DrawInspector(engine, session);
        DrawMaterialInspector(engine, session);
    }
    if (showHierarchy_) DrawHierarchy(engine, session);
    if (showContentBrowser_) DrawContentBrowser(engine);
    if (showConsole_) DrawConsole(engine);
    if (showStatistics_) DrawStatistics(engine, session);
}

void EditorLayer::DrawModals(Engine& engine, EditorSession& session)
{
    files_.DrawUnsavedModal(engine, session);
}

void EditorLayer::Shutdown()
{
    transformInspector_.Reset();
    hierarchy_.Reset();
    inspectorPersistentID_ = 0;
    inspectorRevision_ = 0;
    viewport_ = {};
}

void EditorLayer::DrawDockHost(Engine& engine, EditorSession& session)
{
    ImGuiViewport* mainViewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(mainViewport->WorkPos);
    ImGui::SetNextWindowSize(mainViewport->WorkSize);
    ImGui::SetNextWindowViewport(mainViewport->ID);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    constexpr ImGuiWindowFlags hostFlags = ImGuiWindowFlags_MenuBar |
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus;

    ImGui::Begin("PlagC Editor DockHost", nullptr, hostFlags);
    ImGui::PopStyleVar(3);
    const ImGuiID dockspaceID = ImGui::GetID("PlagC.Editor.Dockspace");
    if (ImGui::DockBuilderGetNode(dockspaceID) == nullptr)
        BuildDefaultLayout(static_cast<unsigned>(std::max(1.0f, mainViewport->WorkSize.x)),
                           static_cast<unsigned>(std::max(1.0f, mainViewport->WorkSize.y)));

    if (ImGui::BeginMenuBar())
    {
        files_.DrawMenu(engine, session);
        if (ImGui::BeginMenu("Edit"))
        {
            std::string undo = "Undo", redo = "Redo";
            if (session.EditingEnabled())
            {
                const auto& document = session.EditDocument();
                if (!document.UndoLabel().empty()) undo += " " + std::string(document.UndoLabel());
                if (!document.RedoLabel().empty()) redo += " " + std::string(document.RedoLabel());
            }
            if (ImGui::MenuItem(undo.c_str(), "Ctrl+Z", false, session.CanUndo()))
            {
                session.Undo(); inspectorRevision_ = 0;
            }
            if (ImGui::MenuItem(redo.c_str(), "Ctrl+Y", false, session.CanRedo()))
            {
                session.Redo(); inspectorRevision_ = 0;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Scene"))
        {
            if (ImGui::MenuItem("Create Empty", nullptr, false, session.EditingEnabled()))
            {
                try { session.EditDocument().Create(); inspectorRevision_ = 0; }
                catch (const std::exception& error) { LogEditorError(engine, error); }
            }
            Entity selected{};
            bool selectedLocked = false;
            if (session.EditingEnabled())
            {
                auto& document = session.EditDocument();
                selected = document.Selection();
                selectedLocked = selected && hierarchy_.Locked(document.PersistentID(selected));
            }
            if (ImGui::MenuItem("Duplicate Selected", "Ctrl+D", false, bool(selected) && !selectedLocked))
            {
                try { session.EditDocument().Duplicate(selected); inspectorRevision_ = 0; }
                catch (const std::exception& error) { LogEditorError(engine, error); }
            }
            if (ImGui::MenuItem("Delete Selected", "Delete", false, bool(selected) && !selectedLocked))
            {
                try
                {
                    session.EditDocument().Destroy(selected);
                    inspectorPersistentID_ = inspectorRevision_ = 0;
                }
                catch (const std::exception& error) { LogEditorError(engine, error); }
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("View"))
        {
            ImGui::MenuItem("Hierarchy", nullptr, &showHierarchy_);
            ImGui::MenuItem("Inspector", nullptr, &showInspector_);
            ImGui::MenuItem("Content Browser", nullptr, &showContentBrowser_);
            ImGui::MenuItem("Console", nullptr, &showConsole_);
            ImGui::MenuItem("Statistics", nullptr, &showStatistics_);
            ImGui::EndMenu();
        }
        DrawTransport(engine, session);
        ImGui::EndMenuBar();
    }
    ImGui::DockSpace(dockspaceID, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);
    ImGui::End();
}

void EditorLayer::BuildDefaultLayout(unsigned width, unsigned height)
{
    const ImGuiID dockspaceID = ImGui::GetID("PlagC.Editor.Dockspace");
    ImGui::DockBuilderRemoveNode(dockspaceID);
    ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceID, ImVec2(static_cast<float>(width), static_cast<float>(height)));
    ImGuiID main = dockspaceID, left = 0, right = 0, bottom = 0, bottomRight = 0;
    ImGui::DockBuilderSplitNode(main, ImGuiDir_Left, 0.18f, &left, &main);
    ImGui::DockBuilderSplitNode(main, ImGuiDir_Right, 0.24f, &right, &main);
    ImGui::DockBuilderSplitNode(main, ImGuiDir_Down, 0.28f, &bottom, &main);
    ImGui::DockBuilderSplitNode(bottom, ImGuiDir_Right, 0.50f, &bottomRight, &bottom);
    ImGui::DockBuilderDockWindow("Hierarchy", left);
    ImGui::DockBuilderDockWindow("Inspector", right);
    ImGui::DockBuilderDockWindow("Statistics", right);
    ImGui::DockBuilderDockWindow("Content Browser", bottom);
    ImGui::DockBuilderDockWindow("Console", bottomRight);
    ImGui::DockBuilderDockWindow("Viewport", main);
    ImGui::DockBuilderFinish(dockspaceID);
}

void EditorLayer::ExecuteTransport(Engine& engine, EditorSession& session, TransportAction action)
{
    try
    {
        switch (action)
        {
        case TransportAction::PlayStop:
            if (session.Playing())
            {
                session.StopPlay();
                engine.Logger().Write(Severity::Info, "EDITOR-PLAY", "Play mode stopped; authoring world restored.");
            }
            else
            {
                // Play always starts from a fully committed authoring snapshot.
                transformInspector_.Finish(engine, true);
                FinishGizmoEdit(engine, true);
                session.StartPlay();
                engine.Logger().Write(Severity::Info, "EDITOR-PLAY", "Play mode started from isolated authoring snapshot.");
            }
            inspectorPersistentID_ = inspectorRevision_ = 0;
            componentPersistentID_ = componentRevision_ = 0;
            break;

        case TransportAction::PauseResume:
            if (!session.Playing())
                return;
            session.TogglePause();
            engine.Logger().Write(Severity::Info, "EDITOR-PLAY",
                session.Paused() ? "Play mode paused." : "Play mode resumed.");
            break;

        case TransportAction::Step:
            if (!session.Paused())
                return;
            session.RequestStep();
            engine.Logger().Write(Severity::Trace, "EDITOR-PLAY", "Single simulation step requested.");
            break;
        }
    }
    catch (const std::exception& error)
    {
        LogEditorError(engine, error);
    }
}

void EditorLayer::HandleTransportShortcuts(Engine& engine, EditorSession& session)
{
    const auto& io = ImGui::GetIO();
    const bool blocked = io.WantTextInput || ImGui::IsAnyItemActive() ||
        io.KeyCtrl || io.KeyAlt || io.KeySuper ||
        ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
    if (blocked)
        return;

    // F5 = Play/Stop, F6 = Pause/Resume, F7 = single step while paused.
    if (ImGui::IsKeyPressed(ImGuiKey_F5, false))
        ExecuteTransport(engine, session, TransportAction::PlayStop);
    else if (session.Playing() && ImGui::IsKeyPressed(ImGuiKey_F6, false))
        ExecuteTransport(engine, session, TransportAction::PauseResume);
    else if (session.Paused() && ImGui::IsKeyPressed(ImGuiKey_F7, false))
        ExecuteTransport(engine, session, TransportAction::Step);
}

void EditorLayer::DrawTransport(Engine& engine, EditorSession& session)
{
    constexpr float approximateWidth = 430.0f;
    const float target = (ImGui::GetWindowSize().x - approximateWidth) * 0.5f;
    if (target > ImGui::GetCursorPosX())
        ImGui::SetCursorPosX(target);

    const PlaySession::State state = session.PlayState();
    const bool running = state != PlaySession::State::Stopped;
    const bool paused = state == PlaySession::State::Paused;

    const ImVec4 stateColour = state == PlaySession::State::Playing
        ? ImVec4(0.35f, 0.90f, 0.45f, 1.0f)
        : paused
            ? ImVec4(1.00f, 0.72f, 0.24f, 1.0f)
            : ImVec4(0.68f, 0.72f, 0.78f, 1.0f);
    const char* stateLabel = state == PlaySession::State::Playing
        ? "PLAYING"
        : paused ? "PAUSED" : "EDIT";

    ImGui::TextColored(stateColour, "%s", stateLabel);
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    ImGui::BeginDisabled(running);
    if (ImGui::Button("Play##Transport"))
        ExecuteTransport(engine, session, TransportAction::PlayStop);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("Start Play mode from an isolated scene snapshot. Shortcut: F5");
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(!running);
    if (ImGui::Button(paused ? "Resume##Transport" : "Pause##Transport"))
        ExecuteTransport(engine, session, TransportAction::PauseResume);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip(paused ? "Resume runtime simulation. Shortcut: F6"
                                 : "Pause runtime simulation. Shortcut: F6");
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(!paused);
    if (ImGui::Button("Step##Transport"))
        ExecuteTransport(engine, session, TransportAction::Step);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("Advance exactly one configured fixed simulation interval. Shortcut: F7");
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(!running);
    if (ImGui::Button("Stop##Transport"))
        ExecuteTransport(engine, session, TransportAction::PlayStop);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("Stop Play mode and return to the unchanged authoring world. Shortcut: F5");
    ImGui::EndDisabled();
}

void EditorLayer::DrawHierarchy(Engine& engine, EditorSession& session)
{
    hierarchy_.Draw(engine, session, showHierarchy_);
}

void EditorLayer::DrawViewport(EditorSession& session)
{
    ImGui::Begin("Viewport");
    const ImVec2 available = ImGui::GetContentRegionAvail();
    viewport_.Resize(static_cast<unsigned>(std::max(1.0f, available.x)),
                     static_cast<unsigned>(std::max(1.0f, available.y)));
    viewport_.input.viewportHovered = ImGui::IsWindowHovered();
    viewport_.input.viewportFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    const ImVec2 surfaceMin = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton("ViewportSurface", ImVec2(static_cast<float>(viewport_.width),
                                                      static_cast<float>(viewport_.height)));

    if (session.Playing())
    {
        const bool paused = session.Paused();
        const ImVec2 surfaceMax{surfaceMin.x + static_cast<float>(viewport_.width),
                                surfaceMin.y + static_cast<float>(viewport_.height)};
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImU32 accent = paused ? IM_COL32(255, 184, 61, 235) : IM_COL32(72, 220, 110, 235);
        const ImU32 background = IM_COL32(17, 21, 28, 220);
        const char* label = paused
            ? "PAUSED  |  Authoring locked  |  F6 Resume  F7 Step  F5 Stop"
            : "PLAYING  |  Authoring locked  |  F6 Pause  F5 Stop";
        const ImVec2 textSize = ImGui::CalcTextSize(label);
        const ImVec2 badgeMin{surfaceMin.x + 10.0f, surfaceMin.y + 10.0f};
        const ImVec2 badgeMax{badgeMin.x + textSize.x + 18.0f, badgeMin.y + textSize.y + 10.0f};

        draw->AddRect(surfaceMin, surfaceMax, accent, 0.0f, 0, 2.0f);
        draw->AddRectFilled(badgeMin, badgeMax, background, 4.0f);
        draw->AddRect(badgeMin, badgeMax, accent, 4.0f, 0, 1.0f);
        draw->AddText(ImVec2(badgeMin.x + 9.0f, badgeMin.y + 5.0f), accent, label);
    }

    ImGui::End();
}

void EditorLayer::SyncInspector(EditorSession& session, const AuthoringEntity& selected)
{
    if (!session.EditingEnabled()) return;
    auto& document = session.EditDocument();
    if (inspectorPersistentID_ == selected.persistentID && inspectorRevision_ == document.Revision()) return;
    inspectorPersistentID_ = selected.persistentID;
    inspectorRevision_ = document.Revision();
    std::snprintf(renameBuffer_.data(), renameBuffer_.size(), "%s", selected.record.name.c_str());
}

void EditorLayer::DrawInspector(Engine& engine, EditorSession& session)
{
    if (!ImGui::Begin("Inspector", &showInspector_)) { ImGui::End(); return; }

    const auto entities = session.ViewDocument().Snapshot();
    const auto selected = std::find_if(entities.begin(), entities.end(),
        [](const AuthoringEntity& entity) { return entity.selected; });

    if (selected == entities.end())
    {
        ImGui::Dummy(ImVec2(0.0f, 8.0f));
        ImGui::TextDisabled(session.EditingEnabled()
            ? "Select an entity in the Hierarchy or Scene View."
            : "Play mode is active. Select a runtime entity to inspect it.");
        ImGui::End();
        return;
    }

    const bool editing = session.EditingEnabled();
    const bool locked = hierarchy_.Locked(selected->persistentID);

    ImGui::PushID("InspectorEntityHeader");
    ImGui::TextUnformatted(selected->record.name.c_str());
    ImGui::SameLine();
    if (!editing)
        ImGui::TextDisabled("  PLAY / READ ONLY");
    else if (locked)
        ImGui::TextDisabled("  LOCKED");
    else
        ImGui::TextDisabled("  ENTITY");

    ImGui::TextDisabled("ID #%llu", static_cast<unsigned long long>(selected->persistentID));
    if (selected->parentPersistentID)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("  Parent #%llu",
            static_cast<unsigned long long>(selected->parentPersistentID));
    }

    if (editing)
    {
        SyncInspector(session, *selected);
        auto& document = session.EditDocument();
        ImGui::BeginDisabled(locked);
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputTextWithHint("##EntityName", "Entity name",
                                     renameBuffer_.data(), renameBuffer_.size(),
                                     ImGuiInputTextFlags_EnterReturnsTrue) ||
            ImGui::IsItemDeactivatedAfterEdit())
        {
            try
            {
                document.Rename(selected->entity, renameBuffer_.data());
                inspectorRevision_ = document.Revision();
            }
            catch (const std::exception& error)
            {
                LogEditorError(engine, error);
                inspectorRevision_ = 0;
            }
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("Rename the selected entity. Rename is command-backed and undoable.");
        ImGui::EndDisabled();

        if (locked)
            ImGui::TextDisabled("Hierarchy lock prevents Inspector mutations.");
    }
    ImGui::Separator();
    ImGui::PopID();

    if (editing)
    {
        transformInspector_.Draw(engine, session, *selected, locked);
    }
    else
    {
        const Transform& transform = selected->record.transform;
        ImGui::SeparatorText("Transform");
        if (ImGui::BeginTable("RuntimeTransform", 2,
            ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV))
        {
            ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 85.0f);
            ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
            auto row = [&](const char* label, Vec3 value)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextDisabled("%s", label);
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.3f   %.3f   %.3f", value.x, value.y, value.z);
            };
            row("Position", transform.position);
            row("Rotation", transform.rotationDegrees);
            row("Scale", transform.scale);
            ImGui::EndTable();
        }
        ImGui::TextDisabled("Runtime world is isolated from the authoring document.");
    }

    ImGui::End();
}

void EditorLayer::DrawContentBrowser(Engine& engine)
{
    if (!ImGui::Begin("Content Browser", &showContentBrowser_)) { ImGui::End(); return; }
    dataPanels_.DrawContentBrowser(engine);
    ImGui::End();
}

void EditorLayer::DrawConsole(Engine& engine)
{
    if (!ImGui::Begin("Console", &showConsole_)) { ImGui::End(); return; }
    dataPanels_.DrawConsole(engine);
    ImGui::End();
}

void EditorLayer::DrawStatistics(Engine& engine, EditorSession& session)
{
    if (!ImGui::Begin("Statistics", &showStatistics_)) { ImGui::End(); return; }
    const FrameProfile& frame = engine.Profile().Latest();
    const JobStats jobs = engine.Jobs().Stats();
    const auto assets = engine.Assets().Registry();
    ImGui::Text("FPS: %.1f", frame.fps);
    ImGui::Text("Frame: %.3f ms", frame.frameMs);
    ImGui::Text("Render submit: %.3f ms", frame.renderSubmitMs);
    ImGui::Text("Renderer CPU: %.3f ms", frame.rendererCpuMs);
    ImGui::Separator();
    ImGui::Text("Entities: %zu", session.ViewWorld().Size());
    ImGui::Text("Assets: %zu", assets.size());
    ImGui::Text("Jobs submitted: %llu", static_cast<unsigned long long>(jobs.submitted));
    ImGui::Text("Jobs executed: %llu", static_cast<unsigned long long>(jobs.executed));
    ImGui::Text("Jobs stolen: %llu", static_cast<unsigned long long>(jobs.stolen));
    ImGui::Text("Job failures: %llu", static_cast<unsigned long long>(jobs.failed));
    ImGui::End();
}

} // namespace PlagC::Editor
