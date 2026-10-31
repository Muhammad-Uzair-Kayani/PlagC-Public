#pragma once

#include <filesystem>
#include <string>

namespace PlagC::Editor {

class EditorLayer;

class EditorWorkspace final {
public:
    void Initialize(EditorLayer&);
    void Tick(EditorLayer&);
    void Save(EditorLayer&);
    void RequestRestore() noexcept { pending_ = Pending::Restore; }
    void RequestReset() noexcept { pending_ = Pending::Reset; }
    bool HasSavedWorkspace() const;
    const std::filesystem::path& LayoutPath() const noexcept { return layoutPath_; }

private:
    enum class Pending { None, Restore, Reset };

    bool ApplyPending(EditorLayer&);
    void SavePanelState(const EditorLayer&);
    void LoadPanelState(EditorLayer&);
    void SetDefaultPanels(EditorLayer&) const;

    std::filesystem::path directory_{"User/Editor"};
    std::filesystem::path layoutPath_{directory_ / "Workspace.ini"};
    std::filesystem::path statePath_{directory_ / "Workspace.state"};
    Pending pending_ = Pending::None;
    bool initialized_ = false;
    bool startupRestore_ = false;
};

} // namespace PlagC::Editor
