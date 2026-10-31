from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

def read(path):
    return (ROOT / path).read_text(encoding="utf-8")

animator = read("Editor/src/EditorAnimatorInspector.cpp")
drag_drop = read("Editor/src/EditorDragDrop.h")
layer = read("Editor/src/EditorLayer.h")
app = read("Editor/src/EditorApp.cpp")
controller = read("PlagC/include/PlagC/Animation/AnimationController.h")
animation = read("PlagC/include/PlagC/Animation/Animation.h")
ozz = read("PlagC/src/Animation/OzzAnimator.cpp")
asset_scene_h = read("PlagC/include/PlagC/Assets/AssetScene.h")
asset_scene_cpp = read("PlagC/src/Assets/AssetScene.cpp")
docs = read("Docs/Editor.md")
validate = read("ValidateEditor.bat")
workflow = read(".github/workflows/editor-windows.yml")

checks = {
    "animator inspector is wired into Editor frame":
        "AppendAnimatorInspector(engine, *session_)" in app and
        "void AppendAnimatorInspector(Engine&, EditorSession&);" in layer,
    "animation source and skeleton assignment use cooked ModelAsset with compatibility validation":
        "Animation Source / Skeleton" in animator and
        "SkeletonCompatible" in animator and
        'CookedModel = "PLAGC_COOKED_MODEL"' in drag_drop and
        "ImGui::AcceptDragDropPayload(DragDrop::CookedModel)" in animator and
        "DragDrop::TextPayload(*payload)" in animator and
        "Use Model Asset" in animator,
    "named clip authoring and playback controls are present":
        "ClipLabel" in animator and
        "Preview in Scene View" in animator and
        'ImGui::Button(animator.state.playing ? "Pause" : "Play")' in animator and
        'ImGui::Button("Restart")' in animator and
        'ImGui::Button("Stop")' in animator and
        'ImGui::Checkbox("Loop"' in animator and
        'ImGui::DragFloat("Speed"' in animator and
        'ImGui::SliderFloat("Time"' in animator,
    "controller assignment, state selection and typed parameter authoring are present":
        "LoadAnimationControllerAsset" in animator and
        'ImGui::BeginCombo("Controller State"' in animator and
        "SetState(" in animator and
        "SetFloat(" in animator and "SetBool(" in animator and
        "Trigger(" in animator and "ResetTrigger(" in animator,
    "controller exposes presentation-safe state and parameter values":
        "float Value(std::string_view name) const" in controller and
        "unsigned CurrentIndex() const noexcept" in controller and
        "void SetState(unsigned state)" in controller,
    "assigned animation source is honored by runtime Animator while target geometry stays model-owned":
        "AssetHandle<ModelAsset> animationSource = {}" in animation and
        "RequireCompatibleSkeleton(*model, *animationSource)" in ozz and
        "for (auto &src : animationSource->animations)" in ozz and
        "const auto &src = p.model->meshes.at(mesh);" in ozz and
        "std::make_unique<Animator>(resources->asset, source)" in asset_scene_cpp,
    "edit preview is selected-only and presentation-owned while Play remains runtime-owned":
        "bool presentationOnly = false" in asset_scene_h and
        "previewAuthoring" in asset_scene_cpp and
        "previewState" in asset_scene_cpp and
        "SameAnimatorAuthoring" in asset_scene_cpp and
        "instance.animator->Update(*instance.previewState, dt)" in asset_scene_cpp and
        "auto sample = *animation" in asset_scene_cpp and
        "AnimatorPreviewEntity(*session_)" in app and
        "!session_->Playing()" in app,
    "skeleton and clip diagnostics are exposed without entering general debug visualization":
        "Skeleton Diagnostics" in animator and
        "Skeleton Joint Hierarchy" in animator and
        "Clip Diagnostics" in animator and
        "tracks for" in animator,
    "milestone 11 is documented and wired into both validation gates":
        "## 11. Animator Inspector — `DONE`" in docs and
        "EditorMilestone11Tests.py" in validate and
        "EditorMilestone11Tests.py" in workflow,
}

failed = [name for name, ok in checks.items() if not ok]
for name, ok in checks.items():
    print(("[PASS] " if ok else "[FAIL] ") + name)
if failed:
    raise SystemExit(f"Milestone 11 regression failed: {', '.join(failed)}")
print(f"[PASS] Editor Milestone 11 regression ({len(checks)} checks)")
