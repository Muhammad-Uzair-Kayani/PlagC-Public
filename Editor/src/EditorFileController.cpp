#include "pgcpch.h"
#include "EditorFileController.h"
#include "EditorFileDialog.h"
#include <PlagC/Application.h>
#include <PlagC/Assets/Asset.h>
#include <imgui.h>

namespace PlagC::Editor {
namespace {

void LogFileError(Engine& engine, const std::exception& error)
{
    engine.Logger().Write(Severity::Error, "EDITOR-FILE", error.what());
}

SceneDocument::ModelResolver Resolver(Engine& engine)
{
    return [&engine](const std::string& path) {
        return engine.Assets().Load<ModelAsset>(path);
    };
}

NativeWindow Owner(Engine& engine)
{
    return engine.MainWindow() ? engine.MainWindow()->NativeHandle() : NativeWindow{};
}

} // namespace

void EditorFileController::DrawMenu(Engine& engine, EditorSession& session)
{
    if (!ImGui::BeginMenu("File"))
        return;

    const bool stopped = session.EditingEnabled();
    if (ImGui::MenuItem("New", "Ctrl+N", false, stopped))
        Request(engine, session, PendingAction::NewScene);
    if (ImGui::MenuItem("Open...", "Ctrl+O", false, stopped))
        Request(engine, session, PendingAction::OpenScene);

    ImGui::Separator();
    if (ImGui::MenuItem("Save", "Ctrl+S", false, stopped))
    {
        try { SaveCurrent(engine, session); }
        catch (const std::exception& error) { LogFileError(engine, error); }
    }
    if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S", false, stopped))
    {
        try { SaveAs(engine, session); }
        catch (const std::exception& error) { LogFileError(engine, error); }
    }

    ImGui::Separator();
    if (ImGui::MenuItem("Exit"))
        Request(engine, session, PendingAction::Exit);

    ImGui::EndMenu();
}

void EditorFileController::DrawUnsavedModal(Engine& engine, EditorSession& session)
{
    if (openUnsavedPopup_)
    {
        ImGui::OpenPopup("Unsaved Changes");
        openUnsavedPopup_ = false;
    }

    if (!ImGui::BeginPopupModal("Unsaved Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    ImGui::TextUnformatted("The current scene has unsaved changes.");
    ImGui::TextUnformatted("Save them before continuing?");
    ImGui::Separator();

    if (ImGui::Button("Save", ImVec2(100.0f, 0.0f)))
    {
        try
        {
            if (SaveCurrent(engine, session))
            {
                const PendingAction action = pending_;
                pending_ = PendingAction::None;
                ImGui::CloseCurrentPopup();
                Execute(engine, session, action, ReplacePolicy::RequireClean);
            }
        }
        catch (const std::exception& error)
        {
            LogFileError(engine, error);
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Discard", ImVec2(100.0f, 0.0f)))
    {
        const PendingAction action = pending_;
        pending_ = PendingAction::None;
        ImGui::CloseCurrentPopup();
        try { Execute(engine, session, action, ReplacePolicy::DiscardChanges); }
        catch (const std::exception& error) { LogFileError(engine, error); }
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100.0f, 0.0f)))
    {
        pending_ = PendingAction::None;
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();
}

void EditorFileController::Request(Engine& engine,
                                   EditorSession& session,
                                   PendingAction action)
{
    if (action != PendingAction::Exit && !session.EditingEnabled())
        return;

    if (session.Dirty())
    {
        pending_ = action;
        openUnsavedPopup_ = true;
        return;
    }

    try { Execute(engine, session, action, ReplacePolicy::RequireClean); }
    catch (const std::exception& error) { LogFileError(engine, error); }
}

void EditorFileController::Execute(Engine& engine,
                                   EditorSession& session,
                                   PendingAction action,
                                   ReplacePolicy policy)
{
    switch (action)
    {
    case PendingAction::None:
        break;
    case PendingAction::NewScene:
        session.NewEmpty(policy);
        ++documentGeneration_;
        engine.Logger().Write(Severity::Info, "EDITOR-FILE", "Created a new empty scene.");
        break;
    case PendingAction::OpenScene:
        if (OpenScene(engine, session, policy))
            ++documentGeneration_;
        break;
    case PendingAction::Exit:
        if (auto* window = engine.MainWindow())
            window->RequestClose();
        break;
    }
}

bool EditorFileController::SaveCurrent(Engine& engine, EditorSession& session)
{
    if (!session.EditingEnabled())
        return false;

    if (!session.HasPath())
        return SaveAs(engine, session);

    session.Save();
    engine.Logger().Write(Severity::Info, "EDITOR-FILE",
                          "Saved scene: " + session.Path().string());
    return true;
}

bool EditorFileController::SaveAs(Engine& engine, EditorSession& session)
{
    if (!session.EditingEnabled())
        return false;

    const NativeWindow owner = Owner(engine);
    if (!NativeSceneFileDialogsAvailable(owner))
    {
        engine.Logger().Write(Severity::Warning, "EDITOR-FILE",
                              "Native scene file dialogs are unavailable on this platform/window.");
        return false;
    }

    const auto path = SaveSceneFileDialog(owner);
    if (!path)
        return false;

    session.SaveAs(*path);
    engine.Logger().Write(Severity::Info, "EDITOR-FILE",
                          "Saved scene: " + path->string());
    return true;
}

bool EditorFileController::OpenScene(Engine& engine,
                                     EditorSession& session,
                                     ReplacePolicy policy)
{
    const NativeWindow owner = Owner(engine);
    if (!NativeSceneFileDialogsAvailable(owner))
    {
        engine.Logger().Write(Severity::Warning, "EDITOR-FILE",
                              "Native scene file dialogs are unavailable on this platform/window.");
        return false;
    }

    const auto path = OpenSceneFileDialog(owner);
    if (!path)
        return false;

    session.Open(*path, Resolver(engine), policy);
    engine.Logger().Write(Severity::Info, "EDITOR-FILE",
                          "Opened scene: " + path->string());
    return true;
}

} // namespace PlagC::Editor
