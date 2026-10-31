#pragma once

#include "EditorDataPanels.h"
#include "EditorMenuController.h"
#include "EditorSceneHierarchy.h"
#include "EditorTransformInspector.h"
#include "EditorWorkspace.h"
#include <PlagC/Animation/AnimationControllerAsset.h>
#include <PlagC/Assets/AssetImport.h>
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
    EditorLayer() : files_(*this) {}
    void Initialize(Engine&, EditorSession&);
    void Frame(Engine&, EditorSession&);
    void DrawModals(Engine&, EditorSession&);
    void Shutdown();

    const ViewportState& Viewport() const noexcept { return viewport_; }
    bool InspectorVisible() const noexcept { return showInspector_; }
    bool HierarchyEntityLocked(std::uint64_t persistentID) const noexcept {
        return hierarchy_.Locked(persistentID);
    }
    Entity AnimatorPreviewEntity(const EditorSession& session) const {
        if (!animatorPreviewEnabled_ || session.Playing() || !animatorPreviewPersistentID_)
            return {};
        return session.Document().FindByPersistentID(animatorPreviewPersistentID_);
    }
    void AppendComponentInspector(Engine&, EditorSession&);
    void AppendAnimatorInspector(Engine&, EditorSession&);
    void AppendAssetInteractions(Engine&, EditorSession&);
    void AppendCameraInspector(Engine&, EditorSession&, CameraDesc& editorCamera);
    void AppendPhysicsInspector(Engine&, EditorSession&, const CameraDesc& editorCamera);
    void AppendAnimationStateMachine(Engine&, EditorSession&);
    void AppendAssetImport(Engine&, EditorSession&);
    void AppendViewportAssetDrop(Engine&, EditorSession&, const CameraDesc& editorCamera);
    void AppendViewportExperience(Engine&, EditorSession&, CameraDesc& editorCamera);
    // Draws and drives a world-space/local-space transform gizmo on the existing Viewport window.
    // Must be called after the viewport target image and scene-view overlays are appended.
    void AppendTransformGizmo(Engine&, EditorSession&, const CameraDesc& editorCamera);

private:
    friend class EditorMenuController;
    friend class EditorWorkspace;

    enum class TransportAction {
        PlayStop,
        PauseResume,
        Step
    };

    void DrawDockHost(Engine&, EditorSession&);
    void DrawEditorToolbar(Engine&, EditorSession&);
    void BuildDefaultLayout(unsigned width, unsigned height);
    void DrawHierarchy(Engine&, EditorSession&);
    void DrawViewport(EditorSession&);
    void DrawInspector(Engine&, EditorSession&);
    void DrawMaterialInspector(Engine&, EditorSession&);
    void DrawContentBrowser(Engine&);
    void DrawConsole(Engine&);
    void DrawStatistics(Engine&, EditorSession&);
    void DrawTransport(Engine&, EditorSession&);
    void HandleTransportShortcuts(Engine&, EditorSession&);
    void ExecuteTransport(Engine&, EditorSession&, TransportAction);
    void SyncInspector(EditorSession&, const AuthoringEntity&);
    void FinishGizmoEdit(Engine&, bool commit);
    void CreateCookedModelEntity(Engine&, EditorSession&, const char* assetPath, Vec3 position);

    EditorMenuController files_;
    EditorWorkspace workspace_;
    EditorPreviewDataPanels dataPanels_;
    EditorSceneHierarchy hierarchy_;
    EditorTransformInspector transformInspector_;
    std::uint64_t seenDocumentGeneration_ = 1;
    ViewportState viewport_{};
    bool showHierarchy_ = true;
    bool showInspector_ = true;
    bool showContentBrowser_ = true;
    bool showConsole_ = true;
    bool showStatistics_ = true;

    std::uint64_t inspectorPersistentID_ = 0;
    std::uint64_t inspectorRevision_ = 0;
    std::array<char, 256> renameBuffer_{};

    std::uint64_t componentPersistentID_ = 0;
    std::uint64_t componentRevision_ = 0;
    std::optional<CameraComponent> componentCamera_;
    std::optional<LightComponent> componentLight_;
    std::optional<PhysicsComponent> componentPhysics_;
    std::optional<AnimatorComponent> componentAnimator_;

    bool physicsDebugVisible_ = true;
    bool physicsDebugAll_ = false;

    std::uint64_t animatorInspectorPersistentID_ = 0;
    std::uint64_t animatorPreviewPersistentID_ = 0;
    bool animatorPreviewEnabled_ = false;
    std::array<char, 512> animatorSourcePath_{};
    std::string animatorInspectorStatus_;

    bool cameraPreview_ = false;
    std::uint64_t cameraPreviewPersistentID_ = 0;
    std::optional<CameraDesc> cameraPreviewRestore_;

    bool showAnimationStateMachine_ = true;
    std::uint64_t animationGraphPersistentID_ = 0;
    AnimationControllerAsset animationGraph_{};
    std::array<char, 260> animationControllerPath_{"Assets/Controllers/Controller.pgcontroller"};
    int animationSelectedState_ = -1;
    int animationSelectedTransition_ = -1;

    bool showAssetImport_ = true;
    AssetImportKind assetImportKind_ = AssetImportKind::Model;
    std::array<char, 512> assetImportSource_{};
    std::array<char, 512> assetImportCooked_{"Assets/Cooked/Imported.plasset"};
    ModelImportOptions modelImportOptions_{};
    TextureImportOptions textureImportOptions_{};
    std::string assetImportStatus_;
    std::string assetImportError_;

    std::unique_ptr<SceneDocument::TransformEdit> gizmoEdit_;
    std::uint64_t gizmoPersistentID_ = 0;
    int gizmoAxis_ = -1;
    Transform gizmoStart_{};
    float gizmoMouseStartX_ = 0.0f;
    float gizmoMouseStartY_ = 0.0f;
};

} // namespace PlagC::Editor
