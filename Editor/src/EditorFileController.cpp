#include "pgcpch.h"
#include "EditorFileController.h"
#include "EditorFileDialog.h"
#include <PlagC/Application.h>
#include <PlagC/Assets/Asset.h>
#include <imgui.h>
#include <algorithm>
#include <fstream>
#include <iomanip>

namespace PlagC::Editor {
namespace {

constexpr std::size_t MaxRecentScenes = 8;

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

std::filesystem::path RecentScenesPath()
{
    return std::filesystem::path("Saved") / "EditorRecentScenes.txt";
}

std::filesystem::path NormalizedScenePath(const std::filesystem::path& path)
{
    if (path.empty())
        return {};
    std::error_code error;
    auto absolute = std::filesystem::absolute(path, error);
    if (error)
        return path.lexically_normal();
    return absolute.lexically_normal();
}

std::string SceneDisplayName(const EditorSession& session)
{
    if (!session.HasPath())
        return "Untitled.plscene";
    const auto filename = session.Path().filename().string();
    return filename.empty() ? session.Path().string() : filename;
}

} // namespace

void EditorFileController::DrawMenu(Engine& engine, EditorSession& session)
{
    EnsureRecentLoaded(engine);

    const auto& io = ImGui::GetIO();
    if (session.EditingEnabled() && io.KeyCtrl && !io.KeyAlt && !io.WantTextInput &&
        !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel))
    {
        if (!io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_N, false))
            Request(engine, session, PendingAction::NewScene);
        else if (!io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_O, false))
            Request(engine, session, PendingAction::OpenScene);
        else if (ImGui::IsKeyPressed(ImGuiKey_S, false))
        {
            try
            {
                if (io.KeyShift) SaveAs(engine, session);
                else SaveCurrent(engine, session);
            }
            catch (const std::exception& error)
            {
                ReportError(engine, io.KeyShift ? "Save As failed" : "Save failed", error);
            }
        }
    }

    const char* menuLabel = session.Dirty() ? "File *" : "File";
    if (!ImGui::BeginMenu(menuLabel))
        return;

    const std::string display = SceneDisplayName(session) + (session.Dirty() ? "  *" : "");
    ImGui::TextDisabled("%s", display.c_str());
    if (session.HasPath() && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("%s", session.Path().string().c_str());
    ImGui::Separator();

    const bool stopped = session.EditingEnabled();
    if (ImGui::MenuItem("New", "Ctrl+N", false, stopped))
        Request(engine, session, PendingAction::NewScene);
    if (ImGui::MenuItem("Open...", "Ctrl+O", false, stopped))
        Request(engine, session, PendingAction::OpenScene);

    if (ImGui::BeginMenu("Open Recent", !recentScenes_.empty() && stopped))
    {
        for (std::size_t i = 0; i < recentScenes_.size(); ++i)
        {
            const auto& path = recentScenes_[i];
            std::error_code error;
            const bool exists = std::filesystem::is_regular_file(path, error);
            std::string label = path.filename().string();
            if (label.empty()) label = path.string();
            label += "##RecentScene" + std::to_string(i);
            if (ImGui::MenuItem(label.c_str(), nullptr, false, exists))
                RequestRecent(engine, session, path);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip("%s%s", path.string().c_str(), exists ? "" : "\nFile is missing");
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Clear Recent"))
        {
            recentScenes_.clear();
            SaveRecent(engine);
        }
        ImGui::EndMenu();
    }

    ImGui::Separator();
    if (ImGui::MenuItem("Save", "Ctrl+S", false, stopped))
    {
        try { SaveCurrent(engine, session); }
        catch (const std::exception& error) { ReportError(engine, "Save failed", error); }
    }
    if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S", false, stopped))
    {
        try { SaveAs(engine, session); }
        catch (const std::exception& error) { ReportError(engine, "Save As failed", error); }
    }

    if (!lastError_.empty())
    {
        ImGui::Separator();
        ImGui::TextDisabled("Last file error:");
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 360.0f);
        ImGui::TextWrapped("%s", lastError_.c_str());
        ImGui::PopTextWrapPos();
    }

    ImGui::Separator();
    if (ImGui::MenuItem("Exit"))
        Request(engine, session, PendingAction::Exit);

    ImGui::EndMenu();
}

bool EditorFileController::Save(Engine& engine, EditorSession& session)
{
    try
    {
        return SaveCurrent(engine, session);
    }
    catch (const std::exception& error)
    {
        ReportError(engine, "Save failed", error);
        return false;
    }
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
            ReportError(engine, "Save before scene replacement failed", error);
        }
    }

    ImGui::SameLine();
    if (ImGui::Button("Discard", ImVec2(100.0f, 0.0f)))
    {
        const PendingAction action = pending_;
        pending_ = PendingAction::None;
        ImGui::CloseCurrentPopup();
        try { Execute(engine, session, action, ReplacePolicy::DiscardChanges); }
        catch (const std::exception& error) { ReportError(engine, "Scene replacement failed", error); }
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(100.0f, 0.0f)))
    {
        pending_ = PendingAction::None;
        pendingPath_.clear();
        ImGui::CloseCurrentPopup();
    }

    if (!lastError_.empty())
    {
        ImGui::Separator();
        ImGui::TextWrapped("%s", lastError_.c_str());
    }

    ImGui::EndPopup();
}

void EditorFileController::Request(Engine& engine,
                                   EditorSession& session,
                                   PendingAction action)
{
    if (action != PendingAction::Exit && !session.EditingEnabled())
        return;

    if (action != PendingAction::OpenRecent)
        pendingPath_.clear();

    if (session.Dirty())
    {
        pending_ = action;
        openUnsavedPopup_ = true;
        return;
    }

    try { Execute(engine, session, action, ReplacePolicy::RequireClean); }
    catch (const std::exception& error) { ReportError(engine, "File operation failed", error); }
}

void EditorFileController::RequestRecent(Engine& engine,
                                         EditorSession& session,
                                         std::filesystem::path path)
{
    pendingPath_ = NormalizedScenePath(path);
    Request(engine, session, PendingAction::OpenRecent);
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
        pendingPath_.clear();
        lastError_.clear();
        ++documentGeneration_;
        engine.Logger().Write(Severity::Info, "EDITOR-FILE", "Created a new empty scene.");
        break;
    case PendingAction::OpenScene:
        if (OpenScene(engine, session, policy))
            ++documentGeneration_;
        pendingPath_.clear();
        break;
    case PendingAction::OpenRecent:
    {
        const auto path = pendingPath_;
        pendingPath_.clear();
        if (OpenScenePath(engine, session, path, policy))
            ++documentGeneration_;
        break;
    }
    case PendingAction::Exit:
        pendingPath_.clear();
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
    RememberRecent(engine, session.Path());
    lastError_.clear();
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
    RememberRecent(engine, *path);
    lastError_.clear();
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
    return OpenScenePath(engine, session, *path, policy);
}

bool EditorFileController::OpenScenePath(Engine& engine,
                                         EditorSession& session,
                                         const std::filesystem::path& path,
                                         ReplacePolicy policy)
{
    if (path.empty())
        throw std::invalid_argument("Scene path is empty");

    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error))
    {
        RemoveRecent(engine, path);
        throw std::runtime_error("Scene file does not exist: " + path.string());
    }

    session.Open(path, Resolver(engine), policy);
    RememberRecent(engine, path);
    lastError_.clear();
    engine.Logger().Write(Severity::Info, "EDITOR-FILE", "Opened scene: " + path.string());
    return true;
}

void EditorFileController::EnsureRecentLoaded(Engine& engine)
{
    if (recentLoaded_)
        return;
    recentLoaded_ = true;

    std::ifstream input(RecentScenesPath(), std::ios::binary);
    if (!input)
        return;

    std::filesystem::path path;
    std::string text;
    while (recentScenes_.size() < MaxRecentScenes && input >> std::quoted(text))
    {
        path = NormalizedScenePath(text);
        if (path.empty())
            continue;
        if (std::find(recentScenes_.begin(), recentScenes_.end(), path) == recentScenes_.end())
            recentScenes_.push_back(std::move(path));
    }
    if (!input.eof() && input.fail())
        engine.Logger().Write(Severity::Warning, "EDITOR-FILE",
                              "Recent scene list contains malformed entries; valid entries were retained.");
}

void EditorFileController::RememberRecent(Engine& engine, const std::filesystem::path& path)
{
    EnsureRecentLoaded(engine);
    const auto normalized = NormalizedScenePath(path);
    if (normalized.empty())
        return;
    std::erase(recentScenes_, normalized);
    recentScenes_.insert(recentScenes_.begin(), normalized);
    if (recentScenes_.size() > MaxRecentScenes)
        recentScenes_.resize(MaxRecentScenes);
    SaveRecent(engine);
}

void EditorFileController::RemoveRecent(Engine& engine, const std::filesystem::path& path)
{
    EnsureRecentLoaded(engine);
    const auto normalized = NormalizedScenePath(path);
    const auto before = recentScenes_.size();
    std::erase(recentScenes_, normalized);
    if (recentScenes_.size() != before)
        SaveRecent(engine);
}

void EditorFileController::SaveRecent(Engine& engine)
{
    try
    {
        const auto path = RecentScenesPath();
        if (const auto parent = path.parent_path(); !parent.empty())
            std::filesystem::create_directories(parent);
        auto temporary = path;
        temporary += ".tmp";
        {
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            if (!output)
                throw std::runtime_error("Cannot write recent scene list");
            for (const auto& scene : recentScenes_)
                output << std::quoted(scene.generic_string()) << '\n';
            output.flush();
            if (!output)
                throw std::runtime_error("Failed while writing recent scene list");
        }
        std::error_code error;
#ifdef _WIN32
        std::filesystem::remove(path, error);
        error.clear();
#endif
        std::filesystem::rename(temporary, path, error);
        if (error)
        {
            std::filesystem::remove(temporary);
            throw std::runtime_error("Cannot commit recent scene list: " + error.message());
        }
    }
    catch (const std::exception& error)
    {
        engine.Logger().Write(Severity::Warning, "EDITOR-FILE", error.what());
    }
}

void EditorFileController::ReportError(Engine& engine,
                                       std::string operation,
                                       const std::exception& error)
{
    lastError_ = std::move(operation) + ": " + error.what();
    engine.Logger().Write(Severity::Error, "EDITOR-FILE", lastError_);
}

} // namespace PlagC::Editor