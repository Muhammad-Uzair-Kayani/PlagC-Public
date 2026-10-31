#pragma once

#include "EditorFileController.h"

namespace PlagC::Editor {

class EditorLayer;

// M22 composes the complete main menu without moving command ownership out of
// EditorFileController, EditorSession, SceneDocument, or the existing panels.
class EditorMenuController final : public EditorFileController {
public:
    explicit EditorMenuController(EditorLayer& owner) noexcept : owner_(owner) {}
    void DrawMenu(Engine&, EditorSession&);

private:
    EditorLayer& owner_;
};

} // namespace PlagC::Editor
