#include "pgcpch.h"
#include "EditorLayer.h"
#include <PlagC/Application.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cstdint>
#include <functional>
#include <unordered_map>

namespace PlagC::Editor {
namespace {

const char* SeverityName(Severity severity)
{
    switch (severity)
    {
    case Severity::Trace: return "TRACE";
    case Severity::Debug: return "DEBUG";
    case Severity::Info: return "INFO";
    case Severity::Warning: return "WARN";
    case Severity::Error: return "ERROR";
    case Severity::Critical: return "CRITICAL";
    }
    return "?";
}

void LogEditorError(Engine& engine, const std::exception& error)
{
    engine.Logger().Write(Severity::Error, "EDITOR", error.what());
}

} // namespace

void EditorLayer::Initialize(Engine& engine, EditorSession&)
{
    engine.Logger().Write(Severity::Info, "EDITOR", "Docked editor workspace initialized.");
}

void EditorLayer::Frame(Engine& engine, EditorSession& session)
{
    DrawDockHost(engine, session);

    if (showHierarchy_)
        DrawHierarchy(engine, session);
    DrawViewport(session);
    if (showInspector_)
        DrawInspector(session);
    if (showContentBrowser_)
        DrawContentBrowser();
    if (showConsole_)
        DrawConsole(engine);
    if (showStatistics_)
        DrawStatistics(engine, session);
}

void EditorLayer::Shutdown()
{
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

    constexpr ImGuiWindowFlags hostFlags =
        ImGuiWindowFlags_MenuBar |
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus;

    ImGui::Begin("PlagC Editor DockHost", nullptr, hostFlags);
    ImGui::PopStyleVar(3);

    const ImGuiID dockspaceID = ImGui::GetID("PlagC.Editor.Dockspace");
    if (ImGui::DockBuilderGetNode(dockspaceID) == nullptr)
        BuildDefaultLayout(static_cast<unsigned>(std::max(1.0f, mainViewport->WorkSize.x)),
                           static_cast<unsigned>(std::max(1.0f, mainViewport->WorkSize.y)));

    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New", "Ctrl+N", false, session.EditingEnabled()))
            {
                try { session.NewEmpty(); }
                catch (const std::exception& error) { LogEditorError(engine, error); }
            }
            ImGui::MenuItem("Open...", "Ctrl+O", false, false);
            if (ImGui::MenuItem("Save", "Ctrl+S", false,
                                session.EditingEnabled() && session.HasPath()))
            {
                try { session.Save(); }
                catch (const std::exception& error) { LogEditorError(engine, error); }
            }
            ImGui::MenuItem("Save As...", "Ctrl+Shift+S", false, false);
            ImGui::Separator();
            if (ImGui::MenuItem("Exit"))
                if (auto* window = engine.MainWindow())
                    window->RequestClose();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit"))
        {
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, session.CanUndo()))
                session.Undo();
            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, session.CanRedo()))
                session.Redo();
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Scene"))
        {
            if (ImGui::MenuItem("Create Empty", nullptr, false, session.EditingEnabled()))
            {
                try { session.EditDocument().Create(); }
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
    ImGui::DockBuilderSetNodeSize(dockspaceID,
                                  ImVec2(static_cast<float>(width), static_cast<float>(height)));

    ImGuiID main = dockspaceID;
    ImGuiID left = 0;
    ImGuiID right = 0;
    ImGuiID bottom = 0;
    ImGuiID bottomRight = 0;

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
    if (target > ImGui::GetCursorPosX())
        ImGui::SetCursorPosX(target);

    try
    {
        switch (session.PlayState())
        {
        case PlaySession::State::Stopped:
            if (ImGui::Button("Play"))
                session.StartPlay();
            break;
        case PlaySession::State::Playing:
            if (ImGui::Button("Pause"))
                session.PausePlay();
            ImGui::SameLine();
            if (ImGui::Button("Stop"))
                session.StopPlay();
            break;
        case PlaySession::State::Paused:
            if (ImGui::Button("Resume"))
                session.ResumePlay();
            ImGui::SameLine();
            if (ImGui::Button("Step"))
                session.RequestStep();
            ImGui::SameLine();
            if (ImGui::Button("Stop"))
                session.StopPlay();
            break;
        }
    }
    catch (const std::exception& error)
    {
        LogEditorError(engine, error);
    }
}

void EditorLayer::DrawHierarchy(Engine& engine, EditorSession& session)
{
    if (!ImGui::Begin("Hierarchy", &showHierarchy_))
    {
        ImGui::End();
        return;
    }

    const auto entities = session.ViewDocument().Snapshot();
    std::unordered_map<std::uint64_t, std::vector<const AuthoringEntity*>> children;
    children.reserve(entities.size() + 1);
    for (const auto& entity : entities)
        children[entity.parentPersistentID].push_back(&entity);

    std::function<void(std::uint64_t)> drawChildren = [&](std::uint64_t parentID)
    {
        const auto found = children.find(parentID);
        if (found == children.end())
            return;

        for (const AuthoringEntity* item : found->second)
        {
            const bool hasChildren = children.contains(item->persistentID);
            ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth |
                                       ImGuiTreeNodeFlags_OpenOnArrow |
                                       ImGuiTreeNodeFlags_OpenOnDoubleClick;
            if (item->selected)
                flags |= ImGuiTreeNodeFlags_Selected;
            if (!hasChildren)
                flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

            ImGui::PushID(static_cast<int>(item->persistentID & 0x7fffffff));
            const bool open = ImGui::TreeNodeEx("##entity", flags, "%s", item->record.name.c_str());
            if (ImGui::IsItemClicked() && session.EditingEnabled())
            {
                try { session.EditDocument().SelectPersistent(item->persistentID); }
                catch (const std::exception& error) { LogEditorError(engine, error); }
            }

            if (hasChildren && open)
            {
                drawChildren(item->persistentID);
                ImGui::TreePop();
            }
            ImGui::PopID();
        }
    };

    if (entities.empty())
        ImGui::TextDisabled("Scene is empty.");
    else
        drawChildren(0);

    if (!session.EditingEnabled())
    {
        ImGui::Separator();
        ImGui::TextDisabled("Runtime hierarchy is read-only while playing.");
    }

    ImGui::End();
}

void EditorLayer::DrawViewport(EditorSession& session)
{
    ImGui::Begin("Viewport");
    const ImVec2 available = ImGui::GetContentRegionAvail();
    viewport_.Resize(static_cast<unsigned>(std::max(1.0f, available.x)),
                     static_cast<unsigned>(std::max(1.0f, available.y)));

    const ImGuiChildFlags childFlags = ImGuiChildFlags_Borders;
    ImGui::BeginChild("ViewportSurface", available, childFlags,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    viewport_.input.viewportHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    viewport_.input.viewportFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

    ImGui::Spacing();
    ImGui::TextDisabled("Ogre editor render target is not bound yet.");
    ImGui::Text("%u x %u  |  %s",
                viewport_.width, viewport_.height,
                session.Playing() ? "Runtime" : "Authoring");
    ImGui::EndChild();
    ImGui::End();
}

void EditorLayer::DrawInspector(EditorSession& session)
{
    if (!ImGui::Begin("Inspector", &showInspector_))
    {
        ImGui::End();
        return;
    }

    const auto entities = session.ViewDocument().Snapshot();
    const auto selected = std::find_if(entities.begin(), entities.end(),
        [](const AuthoringEntity& entity) { return entity.selected; });

    if (selected == entities.end())
    {
        ImGui::TextDisabled(session.EditingEnabled()
                                ? "Select an entity to inspect it."
                                : "Runtime document is read-only.");
        ImGui::End();
        return;
    }

    ImGui::TextUnformatted(selected->record.name.c_str());
    ImGui::Separator();
    ImGui::Text("Persistent ID: %llu",
                static_cast<unsigned long long>(selected->persistentID));
    if (selected->parentPersistentID)
        ImGui::Text("Parent ID: %llu",
                    static_cast<unsigned long long>(selected->parentPersistentID));
    else
        ImGui::TextDisabled("Root entity");

    const Transform transform = session.EditingEnabled()
                                    ? session.EditDocument().LocalTransform(selected->entity)
                                    : selected->record.transform;
    ImGui::SeparatorText(session.EditingEnabled() ? "Local Transform" : "World Transform");
    ImGui::Text("Position  %.3f  %.3f  %.3f",
                transform.position.x, transform.position.y, transform.position.z);
    ImGui::Text("Rotation  %.3f  %.3f  %.3f",
                transform.rotationDegrees.x, transform.rotationDegrees.y,
                transform.rotationDegrees.z);
    ImGui::Text("Scale     %.3f  %.3f  %.3f",
                transform.scale.x, transform.scale.y, transform.scale.z);

    ImGui::SeparatorText("Components");
    ImGui::Text("Camera: %s", selected->record.camera ? "yes" : "no");
    ImGui::Text("Light: %s", selected->record.light ? "yes" : "no");
    ImGui::Text("Model: %s", selected->record.model ? "yes" : "no");
    ImGui::Text("Physics: %s", selected->record.physics ? "yes" : "no");
    ImGui::Text("Animator: %s", selected->record.animator ? "yes" : "no");
    ImGui::End();
}

void EditorLayer::DrawContentBrowser()
{
    if (!ImGui::Begin("Content Browser", &showContentBrowser_))
    {
        ImGui::End();
        return;
    }
    ImGui::TextDisabled("Asset discovery will populate this panel in the next editor checkpoint.");
    ImGui::End();
}

void EditorLayer::DrawConsole(Engine& engine)
{
    if (!ImGui::Begin("Console", &showConsole_))
    {
        ImGui::End();
        return;
    }

    if (ImGui::Button("Clear"))
        engine.Logger().Clear();
    ImGui::Separator();

    const auto records = engine.Logger().Snapshot();
    ImGui::BeginChild("ConsoleRecords", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None,
                      ImGuiWindowFlags_HorizontalScrollbar);
    for (const auto& record : records)
        ImGui::TextUnformatted((record.timestamp + " [" + SeverityName(record.severity) + "] [" +
                                record.category + "] " + record.message).c_str());
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f)
        ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();
    ImGui::End();
}

void EditorLayer::DrawStatistics(Engine& engine, EditorSession& session)
{
    if (!ImGui::Begin("Statistics", &showStatistics_))
    {
        ImGui::End();
        return;
    }

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
