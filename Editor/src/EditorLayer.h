#pragma once

#include <PlagC/Editor/EditorModels.h>
#include <PlagC/Editor/EditorSession.h>

namespace PlagC {
class Engine;
}

namespace PlagC::Editor {

class EditorLayer {
public:
    void Initialize(Engine&, EditorSession&);
    void Frame(Engine&, EditorSession&);
    void Shutdown();

    const ViewportState& Viewport() const noexcept { return viewport_; }

private:
    void DrawDockHost(Engine&, EditorSession&);
    void BuildDefaultLayout(unsigned width, unsigned height);
    void DrawHierarchy(Engine&, EditorSession&);
    void DrawViewport(EditorSession&);
    void DrawInspector(EditorSession&);
    void DrawContentBrowser();
    void DrawConsole(Engine&);
    void DrawStatistics(Engine&, EditorSession&);
    void DrawTransport(Engine&, EditorSession&);

    ViewportState viewport_{};
    bool showHierarchy_ = true;
    bool showInspector_ = true;
    bool showContentBrowser_ = true;
    bool showConsole_ = true;
    bool showStatistics_ = true;
};

} // namespace PlagC::Editor
