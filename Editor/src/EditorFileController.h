#pragma once

#include <PlagC/Editor/EditorSession.h>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace PlagC {
class Engine;
}

namespace PlagC::Editor {

class EditorFileController {
public:
    void DrawMenu(Engine&, EditorSession&);
    void DrawUnsavedModal(Engine&, EditorSession&);
    bool Save(Engine&, EditorSession&);
    std::uint64_t DocumentGeneration() const noexcept { return documentGeneration_; }

private:
    enum class PendingAction {
        None,
        NewScene,
        OpenScene,
        OpenRecent,
        Exit
    };

    void Request(Engine&, EditorSession&, PendingAction);
    void RequestRecent(Engine&, EditorSession&, std::filesystem::path);
    void Execute(Engine&, EditorSession&, PendingAction, ReplacePolicy);
    bool SaveCurrent(Engine&, EditorSession&);
    bool SaveAs(Engine&, EditorSession&);
    bool OpenScene(Engine&, EditorSession&, ReplacePolicy);
    bool OpenScenePath(Engine&, EditorSession&, const std::filesystem::path&, ReplacePolicy);
    void EnsureRecentLoaded(Engine&);
    void RememberRecent(Engine&, const std::filesystem::path&);
    void RemoveRecent(Engine&, const std::filesystem::path&);
    void SaveRecent(Engine&);
    void ReportError(Engine&, std::string operation, const std::exception&);

    PendingAction pending_ = PendingAction::None;
    std::filesystem::path pendingPath_;
    std::vector<std::filesystem::path> recentScenes_;
    std::string lastError_;
    bool recentLoaded_ = false;
    bool openUnsavedPopup_ = false;
    std::uint64_t documentGeneration_ = 1;
};

} // namespace PlagC::Editor