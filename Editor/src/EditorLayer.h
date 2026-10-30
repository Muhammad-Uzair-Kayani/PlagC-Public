#pragma once

#include "EditorDataPanels.h"
#include "EditorFileController.h"
#include "EditorSceneHierarchy.h"
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
    void DrawModals(Engine&, EditorSession&);
    void Shutdown();

    const ViewportState& Viewport() const noexcept { return viewport_; }
    bool InspectorVisible() const noexcept { return showInspector_; }
    bool HierarchyEntityLocked(std::uint64_t persistentID) const noexcept {
        return hierarchy_.Locked(persistentID);
    }
    void AppendComponentInspector(Engine&, EditorSession&);
    void AppendAssetInteractions(Engine&, EditorSession&);
    void AppendViewportAssetDrop(Engine&, EditorSession&, const CameraDesc& editorCamera);
    void AppendViewportExperience(Engine&, EditorSession&, CameraDesc& editorCamera);
    // Draws and drives a world-space/local-space transform gizmo on the existing Viewport window.
    // Must be called after the viewport target image and scene-view overlays are appended.
    void AppendTransformGizmo(Engine&, EditorSession&, const CameraDesc& editorCamera);

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
    void FinishGizmoEdit(Engine&, bool commit);
    void CreateCookedModelEntity(Engine&, EditorSession&, const char* assetPath, Vec3 position);

    EditorFileController files_;
    EditorDataPanels dataPanels_;
    EditorSceneHierarchy hierarchy_;
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

    std::unique_ptr<SceneDocument::TransformEdit> gizmoEdit_;
    std::uint64_t gizmoPersistentID_ = 0;
    int gizmoAxis_ = -1;
    Transform gizmoStart_{};
    float gizmoMouseStartX_ = 0.0f;
    float gizmoMouseStartY_ = 0.0f;
};

} // namespace PlagC::Editor
