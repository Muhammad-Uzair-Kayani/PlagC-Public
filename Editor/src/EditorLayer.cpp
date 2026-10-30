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
    if (showInspector_) DrawInspector(engine, session);
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

void EditorLayer::DrawTransport(Engine& engine, EditorSession& session)
{
    constexpr float approximateWidth = 220.0f;
    const float target = (ImGui::GetWindowSize().x - approximateWidth) * 0.5f;
    if (target > ImGui::GetCursorPosX()) ImGui::SetCursorPosX(target);
    try
    {
        switch (session.PlayState())
        {
        case PlaySession::State::Stopped:
            if (ImGui::Button("Play"))
            {
                transformInspector_.Finish(engine, true); session.StartPlay();
                inspectorPersistentID_ = inspectorRevision_ = 0;
            }
            break;
        case PlaySession::State::Playing:
            if (ImGui::Button("Pause")) session.PausePlay();
            ImGui::SameLine();
            if (ImGui::Button("Stop"))
            {
                session.StopPlay(); inspectorPersistentID_ = inspectorRevision_ = 0;
            }
            break;
        case PlaySession::State::Paused:
            if (ImGui::Button("Resume")) session.ResumePlay();
            ImGui::SameLine();
            if (ImGui::Button("Step")) session.RequestStep();
            ImGui::SameLine();
            if (ImGui::Button("Stop"))
            {
                session.StopPlay(); inspectorPersistentID_ = inspectorRevision_ = 0;
            }
            break;
        }
    }
    catch (const std::exception& error) { LogEditorError(engine, error); }
}

void EditorLayer::DrawHierarchy(Engine& engine, EditorSession& session)
{
    hierarchy_.Draw(engine, session, showHierarchy_);
}

void EditorLayer::DrawViewport(EditorSession&)
{
    ImGui::Begin("Viewport");
    const ImVec2 available = ImGui::GetContentRegionAvail();
    viewport_.Resize(static_cast<unsigned>(std::max(1.0f, available.x)),
                     static_cast<unsigned>(std::max(1.0f, available.y)));
    viewport_.input.viewportHovered = ImGui::IsWindowHovered();
    viewport_.input.viewportFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    ImGui::InvisibleButton("ViewportSurface", ImVec2(static_cast<float>(viewport_.width),
                                                      static_cast<float>(viewport_.height)));
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

    // Entity header -----------------------------------------------------------
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
        // Runtime uses the same Inspector surface, but authoring controls are deliberately read-only.
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

    // Component cards are appended by AppendComponentInspector in the same logical
    // Inspector window later in the frame. Keeping the entity header/Transform here
    // preserves the Transform Inspector milestone while eliminating the old yes/no
    // component debug summary.
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
