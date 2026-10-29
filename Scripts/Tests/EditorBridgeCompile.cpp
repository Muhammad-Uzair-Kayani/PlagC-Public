#include <PlagC/Editor/EngineEditorBridge.h>

using namespace PlagC;

void CompileEditorBridge(Engine& engine, const Editor::SceneDocument& document)
{
    auto snapshot = Editor::CaptureEngineEditorSnapshot(engine, document);
    (void)snapshot;
}
