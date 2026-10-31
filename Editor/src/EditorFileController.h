#pragma once

#include <PlagC/Editor/EditorSession.h>
#include <cstdint>

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
        Exit
    };

    void Request(Engine&, EditorSession&, PendingAction);
    void Execute(Engine&, EditorSession&, PendingAction, ReplacePolicy);
    bool SaveCurrent(Engine&, EditorSession&);
    bool SaveAs(Engine&, EditorSession&);
    bool OpenScene(Engine&, EditorSession&, ReplacePolicy);

    PendingAction pending_ = PendingAction::None;
    bool openUnsavedPopup_ = false;
    std::uint64_t documentGeneration_ = 1;
};

} // namespace PlagC::Editor
