#pragma once

#include "EditorDataPanels.h"
#include "EditorFileController.h"
#include <PlagC/Editor/EditorModels.h>
#include <PlagC/Editor/EditorSession.h>
#include <array>
#include <memory>
#include <optional>

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
    bool InspectorVisible() const noexcept { return showInspector_; }
    // Appends full component authoring controls to the Inspector after the core
    // transform/name section has been emitted by Frame().
    void AppendComponentInspector(Engine&, EditorSession&);

private:
    void DrawDockHost(Engine&, EditorSession&);
    void BuildDefaultLayout(unsigned width, unsigned height);
    void DrawHierarchy(Engine&, EditorSession&);
    void DrawViewport(EditorSession&);
    void DrawInspector(Engine&, EditorSession&);
    void DrawContentBrowser(Engine&);
    void DrawConsole(Engine&);
    void DrawStatistics(Engine&, EditorSession&);
    void DrawTransport(Engine&, EditorSession&);
    void SyncInspector(EditorSession&, const AuthoringEntity&);
    void PreviewInspectorTransform(Engine&, EditorSession&, const AuthoringEntity&);
    void FinishTransformEdit(Engine&, bool commit);

    EditorFileController files_;
    EditorDataPanels dataPanels_;
    std::uint64_t seenDocumentGeneration_ = 1;
    ViewportState viewport_{};
    bool showHierarchy_ = true;
    bool showInspector_ = true;
    bool showContentBrowser_ = true;
    bool showConsole_ = true;
    bool showStatistics_ = true;

    std::uint64_t inspectorPersistentID_ = 0;
    std::uint64_t inspectorRevision_ = 0;
    Transform inspectorLocal_{};
    std::array<char, 256> renameBuffer_{};
    std::unique_ptr<SceneDocument::TransformEdit> transformEdit_;
    std::uint64_t transformEditPersistentID_ = 0;

    std::uint64_t componentPersistentID_ = 0;
    std::uint64_t componentRevision_ = 0;
    std::optional<CameraComponent> componentCamera_;
    std::optional<LightComponent> componentLight_;
    std::optional<PhysicsComponent> componentPhysics_;
    std::optional<AnimatorComponent> componentAnimator_;
};

} // namespace PlagC::Editor
