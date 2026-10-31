#include "pgcpch.h"
#include "EditorMenuController.h"
#include "EditorLayer.h"
#include <PlagC/Application.h>
#include <imgui.h>

namespace PlagC::Editor {
namespace {

void LogMenuError(Engine& engine, const std::exception& error)
{
    engine.Logger().Write(Severity::Error, "EDITOR-MENU", error.what());
}

} // namespace

void EditorMenuController::DrawMenu(Engine& engine, EditorSession& session)
{
    try { owner_.workspace_.Tick(owner_); }
    catch (const std::exception& error) { LogMenuError(engine, error); }

    // Preserve File ownership and its existing context-safe Ctrl+N/O/S routing.
    EditorFileController::DrawMenu(engine, session);

    const bool authoring = session.EditingEnabled();
    Entity selected{};
    bool selectedLocked = false;
    bool transient = false;
    if (authoring)
    {
        auto& document = session.EditDocument();
        selected = document.Selection();
        transient = document.HasTransientEdit();
        selectedLocked = selected && owner_.hierarchy_.Locked(document.PersistentID(selected));
    }

    if (ImGui::BeginMenu("Entity"))
    {
        if (ImGui::MenuItem("Create Empty", nullptr, false, authoring && !transient))
        {
            try
            {
                session.EditDocument().Create();
                owner_.inspectorRevision_ = 0;
            }
            catch (const std::exception& error) { LogMenuError(engine, error); }
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Duplicate Selected", "Ctrl+D", false,
                            bool(selected) && !selectedLocked && !transient))
        {
            try
            {
                session.EditDocument().Duplicate(selected);
                owner_.inspectorRevision_ = 0;
            }
            catch (const std::exception& error) { LogMenuError(engine, error); }
        }
        if (ImGui::MenuItem("Delete Selected", "Delete", false,
                            bool(selected) && !selectedLocked && !transient))
        {
            try
            {
                session.EditDocument().Destroy(selected);
                owner_.inspectorPersistentID_ = owner_.inspectorRevision_ = 0;
            }
            catch (const std::exception& error) { LogMenuError(engine, error); }
        }
        if (ImGui::MenuItem("Clear Selection", "Esc", false, bool(selected)))
        {
            if (authoring)
                session.EditDocument().Select({});
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Assets"))
    {
        if (ImGui::MenuItem("Content Browser", nullptr, owner_.showContentBrowser_))
            owner_.showContentBrowser_ = true;
        if (ImGui::MenuItem("Refresh Asset Browser"))
            owner_.dataPanels_.RefreshAssets(engine);

        ImGui::Separator();
        if (ImGui::MenuItem("Import / Reimport...", nullptr,
                            owner_.showAssetImport_, authoring))
            owner_.showAssetImport_ = !owner_.showAssetImport_;
        if (ImGui::MenuItem("Animation State Machine", nullptr,
                            owner_.showAnimationStateMachine_))
            owner_.showAnimationStateMachine_ = !owner_.showAnimationStateMachine_;
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Window"))
    {
        ImGui::MenuItem("Hierarchy", nullptr, &owner_.showHierarchy_);
        ImGui::MenuItem("Inspector", nullptr, &owner_.showInspector_);
        ImGui::MenuItem("Content Browser", nullptr, &owner_.showContentBrowser_);
        ImGui::MenuItem("Console", nullptr, &owner_.showConsole_);
        ImGui::MenuItem("Statistics", nullptr, &owner_.showStatistics_);
        ImGui::Separator();
        ImGui::MenuItem("Animation State Machine", nullptr,
                        &owner_.showAnimationStateMachine_);
        ImGui::MenuItem("Asset Import", nullptr, &owner_.showAssetImport_);
        ImGui::Separator();

        if (ImGui::BeginMenu("Workspace"))
        {
            if (ImGui::MenuItem("Save Workspace"))
            {
                try
                {
                    owner_.workspace_.Save(owner_);
                    engine.Logger().Write(Severity::Info, "EDITOR-WORKSPACE",
                                          "Editor workspace saved.");
                }
                catch (const std::exception& error) { LogMenuError(engine, error); }
            }

            if (ImGui::MenuItem("Restore Saved Workspace", nullptr, false,
                                owner_.workspace_.HasSavedWorkspace()))
            {
                owner_.workspace_.RequestRestore();
                engine.Logger().Write(Severity::Info, "EDITOR-WORKSPACE",
                                      "Workspace restore requested.");
            }
            if (ImGui::MenuItem("Reset to Default Layout"))
            {
                owner_.workspace_.RequestReset();
                engine.Logger().Write(Severity::Info, "EDITOR-WORKSPACE",
                                      "Default workspace reset requested.");
            }

            ImGui::Separator();
            const std::string path = owner_.workspace_.LayoutPath().generic_string();
            ImGui::TextDisabled("Saved locally: %s", path.c_str());
            ImGui::EndMenu();
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Debug"))
    {
        ImGui::MenuItem("Grid", nullptr, &owner_.viewport_.gridVisible);
        ImGui::MenuItem("Gizmos", nullptr, &owner_.viewport_.gizmosVisible);
        ImGui::MenuItem("Preview Lighting", nullptr, &owner_.viewport_.previewLighting);
        ImGui::Separator();
        ImGui::MenuItem("Physics Colliders", nullptr, &owner_.physicsDebugVisible_);
        ImGui::BeginDisabled(!owner_.physicsDebugVisible_);
        ImGui::MenuItem("All Physics Colliders", nullptr, &owner_.physicsDebugAll_);
        ImGui::EndDisabled();
        ImGui::Separator();
        if (ImGui::MenuItem("Open Statistics"))
            owner_.showStatistics_ = true;
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Help"))
    {
        ImGui::MenuItem("PlagC Editor", nullptr, false, false);
        ImGui::TextDisabled("Prototype authoring workspace");
        ImGui::Separator();
        ImGui::TextDisabled("File: Ctrl+N / Ctrl+O / Ctrl+S");
        ImGui::TextDisabled("Edit: Ctrl+Z / Ctrl+Y / Ctrl+D / Delete");
        ImGui::TextDisabled("Play: F5 / F6 / F7");
        ImGui::Separator();
        if (ImGui::MenuItem("Copy Editor Documentation Path"))
            ImGui::SetClipboardText("Docs/Editor.md");
        ImGui::EndMenu();
    }
}

} // namespace PlagC::Editor
