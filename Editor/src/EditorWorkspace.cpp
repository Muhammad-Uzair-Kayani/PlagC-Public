#include "pgcpch.h"
#include "EditorWorkspace.h"
#include "EditorLayer.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <fstream>
#include <stdexcept>
#include <string>

namespace PlagC::Editor {
namespace {

void RequireWrite(const std::ofstream& stream, const char* message)
{
    if (!stream)
        throw std::runtime_error(message);
}

bool ParseFlag(std::string_view line, std::string_view key, bool current)
{
    if (!line.starts_with(key) || line.size() != key.size() + 2 || line[key.size()] != '=')
        return current;
    const char value = line.back();
    return value == '1' ? true : value == '0' ? false : current;
}

} // namespace

void EditorWorkspace::Initialize(EditorLayer& owner)
{
    if (initialized_)
        return;

    std::error_code error;
    std::filesystem::create_directories(directory_, error);
    if (error)
        throw std::runtime_error("failed to create editor workspace directory: " + error.message());

    // PlagC owns when workspace settings are written so layout and panel visibility
    // are committed together. Dear ImGui still owns the docking/window serialization.
    // This initialization happens during the first menu pass, after ImGui::NewFrame may
    // already have consumed a legacy root imgui.ini. Disable future automatic I/O and
    // deterministically replace any first-frame legacy state before the DockSpace is drawn.
    const bool hasSavedWorkspace = HasSavedWorkspace();
    ImGui::GetIO().IniFilename = nullptr;
    if (hasSavedWorkspace)
    {
        startupRestore_ = true;
        if (std::filesystem::is_regular_file(statePath_))
            LoadPanelState(owner);
    }
    else
    {
        ImGui::ClearIniSettings();
        pending_ = Pending::Reset;
    }
    initialized_ = true;
}

void EditorWorkspace::Tick(EditorLayer& owner)
{
    if (!initialized_)
        Initialize(owner);

    ApplyPending(owner);

    auto& io = ImGui::GetIO();
    if (io.WantSaveIniSettings)
    {
        Save(owner);
        io.WantSaveIniSettings = false;
    }
}

bool EditorWorkspace::HasSavedWorkspace() const
{
    std::error_code error;
    return std::filesystem::is_regular_file(layoutPath_, error) && !error;
}

void EditorWorkspace::Save(EditorLayer& owner)
{
    if (!initialized_)
        Initialize(owner);

    ImGui::SaveIniSettingsToDisk(layoutPath_.string().c_str());
    SavePanelState(owner);
}

bool EditorWorkspace::ApplyPending(EditorLayer& owner)
{
    if (!startupRestore_ && pending_ == Pending::None)
        return false;

    const Pending action = startupRestore_ ? Pending::Restore : pending_;
    startupRestore_ = false;
    pending_ = Pending::None;

    const ImGuiID dockspaceID = ImGui::GetID("PlagC.Editor.Dockspace");
    ImGuiViewport* viewport = ImGui::GetMainViewport();

    if (action == Pending::Restore)
    {
        if (!HasSavedWorkspace())
            return false;

        ImGui::DockBuilderRemoveNode(dockspaceID);
        ImGui::ClearIniSettings();
        ImGui::LoadIniSettingsFromDisk(layoutPath_.string().c_str());
        LoadPanelState(owner);
        return true;
    }

    if (action == Pending::Reset)
    {
        ImGui::DockBuilderRemoveNode(dockspaceID);
        ImGui::ClearIniSettings();
        SetDefaultPanels(owner);
        owner.BuildDefaultLayout(
            static_cast<unsigned>(std::max(1.0f, viewport->WorkSize.x)),
            static_cast<unsigned>(std::max(1.0f, viewport->WorkSize.y)));
        Save(owner);
        return true;
    }

    return false;
}

void EditorWorkspace::SavePanelState(const EditorLayer& owner)
{
    const auto temporary = statePath_.string() + ".tmp";
    std::ofstream stream(temporary, std::ios::trunc);
    RequireWrite(stream, "failed to open editor workspace state for writing");

    stream << "Version=1\n"
           << "Hierarchy=" << int(owner.showHierarchy_) << '\n'
           << "Inspector=" << int(owner.showInspector_) << '\n'
           << "ContentBrowser=" << int(owner.showContentBrowser_) << '\n'
           << "Console=" << int(owner.showConsole_) << '\n'
           << "Statistics=" << int(owner.showStatistics_) << '\n'
           << "AnimationStateMachine=" << int(owner.showAnimationStateMachine_) << '\n'
           << "AssetImport=" << int(owner.showAssetImport_) << '\n';
    stream.flush();
    RequireWrite(stream, "failed to write editor workspace state");
    stream.close();

    std::error_code error;
    std::filesystem::remove(statePath_, error);
    error.clear();
    std::filesystem::rename(temporary, statePath_, error);
    if (error)
    {
        std::filesystem::remove(temporary, error);
        throw std::runtime_error("failed to replace editor workspace state: " + error.message());
    }
}

void EditorWorkspace::LoadPanelState(EditorLayer& owner)
{
    std::ifstream stream(statePath_);
    if (!stream)
        return;

    std::string line;
    bool versionValid = false;
    while (std::getline(stream, line))
    {
        if (line == "Version=1")
        {
            versionValid = true;
            continue;
        }
        if (!versionValid)
            continue;

        owner.showHierarchy_ = ParseFlag(line, "Hierarchy", owner.showHierarchy_);
        owner.showInspector_ = ParseFlag(line, "Inspector", owner.showInspector_);
        owner.showContentBrowser_ = ParseFlag(line, "ContentBrowser", owner.showContentBrowser_);
        owner.showConsole_ = ParseFlag(line, "Console", owner.showConsole_);
        owner.showStatistics_ = ParseFlag(line, "Statistics", owner.showStatistics_);
        owner.showAnimationStateMachine_ = ParseFlag(
            line, "AnimationStateMachine", owner.showAnimationStateMachine_);
        owner.showAssetImport_ = ParseFlag(line, "AssetImport", owner.showAssetImport_);
    }
}

void EditorWorkspace::SetDefaultPanels(EditorLayer& owner) const
{
    owner.showHierarchy_ = true;
    owner.showInspector_ = true;
    owner.showContentBrowser_ = true;
    owner.showConsole_ = true;
    owner.showStatistics_ = true;
    owner.showAnimationStateMachine_ = true;
    owner.showAssetImport_ = true;
}

} // namespace PlagC::Editor
