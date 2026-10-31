#include "pgcpch.h"
#include "EditorDragDrop.h"
#include "EditorLayer.h"
#include <PlagC/Application.h>
#include <PlagC/Assets/AssetManager.h>
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

namespace PlagC::Editor {
namespace {

std::string CookedAssetKey(std::string path)
{
    std::replace(path.begin(), path.end(), '\\', '/');
    constexpr std::string_view mountedPrefix = "Assets://Cooked/";
    constexpr std::string_view filesystemPrefix = "Assets/Cooked/";
    constexpr std::string_view cookedPrefix = "Cooked/";
    if (path.starts_with(mountedPrefix)) return path.substr(mountedPrefix.size());
    if (path.starts_with(filesystemPrefix)) return path.substr(filesystemPrefix.size());
    if (path.starts_with(cookedPrefix)) return path.substr(cookedPrefix.size());
    const auto found = path.find("/Assets/Cooked/");
    if (found != std::string::npos)
        return path.substr(found + std::string_view("/Assets/Cooked/").size());
    throw std::invalid_argument(
        "Animation source must be a cooked ModelAsset under Assets://Cooked");
}

AssetHandle<ModelAsset> LoadAnimationModel(Engine& engine, std::string path)
{
    if (path.empty()) throw std::invalid_argument("Animation source path is empty");
    auto asset = engine.Assets().Load<ModelAsset>(CookedAssetKey(std::move(path)));
    if (!asset) throw std::runtime_error("Animation source is not a loadable ModelAsset");
    return asset;
}

AssetHandle<ModelAsset> ResolvedAnimationAsset(const AuthoringEntity& entity)
{
    if (!entity.record.animator) return {};
    if (entity.record.animator->animationSource) return entity.record.animator->animationSource;
    if (entity.record.model) return entity.record.model->asset;
    return {};
}

bool SkeletonCompatible(const ModelAsset& model, const ModelAsset& source, std::string& reason)
{
    const auto& target = model.skeleton.joints;
    const auto& incoming = source.skeleton.joints;
    if (target.size() != incoming.size())
    {
        reason = "Skeleton joint count does not match the rendered model.";
        return false;
    }
    for (std::size_t i = 0; i < target.size(); ++i)
        if (target[i].name != incoming[i].name || target[i].parent != incoming[i].parent)
        {
            reason = "Skeleton names/parent layout do not match the rendered model.";
            return false;
        }
    return true;
}

const char* ParameterTypeName(AnimationParameterType type)
{
    switch (type)
    {
    case AnimationParameterType::Float: return "Float";
    case AnimationParameterType::Bool: return "Bool";
    case AnimationParameterType::Trigger: return "Trigger";
    }
    return "?";
}

std::string ClipLabel(const AnimationClip& clip, std::size_t index)
{
    char buffer[256]{};
    std::snprintf(buffer, sizeof(buffer), "%zu: %s (%.2f s)", index,
                  clip.name.empty() ? "Unnamed Clip" : clip.name.c_str(), clip.duration);
    return buffer;
}

unsigned JointDepth(const std::vector<AssetNode>& joints, std::size_t index)
{
    unsigned depth = 0;
    int parent = joints[index].parent;
    for (std::size_t guard = 0; parent >= 0 && guard < joints.size(); ++guard)
    {
        ++depth;
        parent = joints[static_cast<std::size_t>(parent)].parent;
    }
    return depth;
}

void LogAnimatorError(Engine& engine, const std::exception& error)
{
    engine.Logger().Write(Severity::Error, "EDITOR-ANIMATOR", error.what());
}

} // namespace

void EditorLayer::AppendAnimatorInspector(Engine& engine, EditorSession& session)
{
    if (!showInspector_)
        return;

    const auto entities = session.ViewDocument().Snapshot();
    const auto selectedIt = std::find_if(entities.begin(), entities.end(),
        [](const AuthoringEntity& entity) { return entity.selected; });
    if (selectedIt == entities.end() || !selectedIt->record.animator)
        return;
    const AuthoringEntity& selected = *selectedIt;
    const bool editable = session.EditingEnabled();
    const bool hierarchyLocked = hierarchy_.Locked(selected.persistentID);
    const bool readOnly = !editable || hierarchyLocked;

    if (animatorInspectorPersistentID_ != selected.persistentID)
    {
        animatorInspectorPersistentID_ = selected.persistentID;
        animatorInspectorStatus_.clear();
        animatorSourcePath_.fill('\0');
        if (selected.record.animator->animationSource)
            std::snprintf(animatorSourcePath_.data(), animatorSourcePath_.size(), "%s",
                          selected.record.animator->animationSource->metadata.path.c_str());
        if (animatorPreviewPersistentID_ != selected.persistentID)
        {
            animatorPreviewEnabled_ = false;
            animatorPreviewPersistentID_ = 0;
        }
    }

    if (!ImGui::Begin("Inspector"))
    {
        ImGui::End();
        return;
    }
    ImGui::PushID("Milestone11AnimatorInspector");
    ImGui::SeparatorText("Animator Authoring");

    auto commit = [&](AnimatorComponent value, const char* message) -> bool
    {
        if (!editable || hierarchyLocked) return false;
        try
        {
            session.EditDocument().SetAnimator(selected.entity, value);
            componentAnimator_ = value;
            componentRevision_ = session.EditDocument().Revision();
            animatorInspectorStatus_ = message ? message : "Animator updated.";
            return true;
        }
        catch (const std::exception& error)
        {
            animatorInspectorStatus_ = error.what();
            LogAnimatorError(engine, error);
            componentRevision_ = 0;
            return false;
        }
    };

    auto currentRecord = [&]() -> AnimatorComponent
    {
        const auto snapshot = session.ViewDocument().Snapshot();
        const auto it = std::find_if(snapshot.begin(), snapshot.end(), [&](const AuthoringEntity& entity) {
            return entity.persistentID == selected.persistentID && entity.record.animator.has_value();
        });
        return it == snapshot.end() ? *selected.record.animator : *it->record.animator;
    };

    AnimatorComponent animator = *selected.record.animator;
    const AssetHandle<ModelAsset> renderedModel = selected.record.model ? selected.record.model->asset : AssetHandle<ModelAsset>{};
    const AssetHandle<ModelAsset> animationAsset = ResolvedAnimationAsset(selected);
    const unsigned clipCount = animationAsset ? static_cast<unsigned>(animationAsset->animations.size()) : 0u;
    (void)clipCount;

    ImGui::TextDisabled("Animation Source / Skeleton");
    ImGui::BeginDisabled(readOnly);
    ImGui::SetNextItemWidth(-72.0f);
    ImGui::InputText("##AnimationSource", animatorSourcePath_.data(), animatorSourcePath_.size());
    ImGui::SameLine();
    if (ImGui::Button("Load"))
    {
        try
        {
            if (!renderedModel) throw std::invalid_argument("Animator requires a rendered ModelAsset");
            auto source = LoadAnimationModel(engine, animatorSourcePath_.data());
            if (source->animations.empty())
                throw std::invalid_argument("Animation source contains no clips");
            std::string reason;
            if (!SkeletonCompatible(*renderedModel, *source, reason))
                throw std::invalid_argument(reason);
            auto value = currentRecord();
            value.animationSource = source;
            value.state.clip = 0;
            value.state.time = 0;
            value.previous = {};
            value.fadeElapsed = value.fadeDuration = 0;
            value.controllerStarted = false;
            if (value.controller)
            {
                const auto graph = value.controller->Graph();
                graph.Validate(static_cast<unsigned>(source->animations.size()));
                value.controller.emplace(graph, static_cast<unsigned>(source->animations.size()));
            }
            if (commit(value, "Assigned compatible animation source."))
                std::snprintf(animatorSourcePath_.data(), animatorSourcePath_.size(), "%s",
                              source->metadata.path.c_str());
        }
        catch (const std::exception& error)
        {
            animatorInspectorStatus_ = error.what();
            LogAnimatorError(engine, error);
        }
    }
    ImGui::EndDisabled();

    ImGui::BeginDisabled(readOnly);
    ImGui::Button("Drop cooked animation ModelAsset here##AnimatorSourceDrop", ImVec2(-1.0f, 0.0f));
    ImGui::EndDisabled();
    const ImVec2 animationDropMin = ImGui::GetItemRectMin();
    const ImVec2 animationDropMax = ImGui::GetItemRectMax();
    if (!readOnly && ImGui::BeginDragDropTarget())
    {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(DragDrop::CookedModel))
        {
            try
            {
                const std::string path = DragDrop::TextPayload(*payload);
                if (!renderedModel) throw std::invalid_argument("Animator requires a rendered ModelAsset");
                auto source = LoadAnimationModel(engine, path);
                if (source->animations.empty()) throw std::invalid_argument("Animation source contains no clips");
                std::string reason;
                if (!SkeletonCompatible(*renderedModel, *source, reason)) throw std::invalid_argument(reason);
                auto value = currentRecord();
                value.animationSource = source;
                value.state.clip = 0;
                value.state.time = 0;
                value.controllerStarted = false;
                value.fadeElapsed = value.fadeDuration = 0;
                if (value.controller)
                {
                    const auto graph = value.controller->Graph();
                    graph.Validate(static_cast<unsigned>(source->animations.size()));
                    value.controller.emplace(graph, static_cast<unsigned>(source->animations.size()));
                }
                if (commit(value, "Assigned dropped animation source."))
                    std::snprintf(animatorSourcePath_.data(), animatorSourcePath_.size(), "%s",
                                  source->metadata.path.c_str());
            }
            catch (const std::exception& error)
            {
                animatorInspectorStatus_ = error.what();
                LogAnimatorError(engine, error);
            }
        }
        ImGui::EndDragDropTarget();
    }
    if (const ImGuiPayload* payload = ImGui::GetDragDropPayload())
    {
        const bool accepted = !readOnly && renderedModel && payload->IsDataType(DragDrop::CookedModel);
        const char* rejected = readOnly
            ? "Animator source is read-only"
            : !renderedModel
                ? "Assign a rendered ModelAsset before an animation source"
                : "Animator accepts cooked ModelAsset animation sources";
        DragDrop::DrawRectFeedback(animationDropMin, animationDropMax, accepted,
                                   "Assign compatible animation source", rejected);
    }

    const bool canUseModelAnimations = renderedModel && !renderedModel->animations.empty();
    ImGui::BeginDisabled(readOnly || !canUseModelAnimations);
    if (ImGui::Button("Use Model Asset"))
    {
        auto value = currentRecord();
        value.animationSource = {};
        value.state.clip = std::min<unsigned>(value.state.clip,
            static_cast<unsigned>(renderedModel->animations.size() - 1));
        value.state.time = 0;
        value.controllerStarted = false;
        value.fadeElapsed = value.fadeDuration = 0;
        if (value.controller)
        {
            const auto graph = value.controller->Graph();
            graph.Validate(static_cast<unsigned>(renderedModel->animations.size()));
            value.controller.emplace(graph, static_cast<unsigned>(renderedModel->animations.size()));
        }
        if (commit(value, "Using the rendered model's skeleton and clips."))
            animatorSourcePath_.fill('\0');
    }
    ImGui::EndDisabled();

    const AssetHandle<ModelAsset> resolved = [&]() {
        const auto snapshot = session.ViewDocument().Snapshot();
        const auto it = std::find_if(snapshot.begin(), snapshot.end(), [&](const AuthoringEntity& entity) {
            return entity.persistentID == selected.persistentID;
        });
        return it == snapshot.end() ? animationAsset : ResolvedAnimationAsset(*it);
    }();
    const unsigned resolvedClipCount = resolved ? static_cast<unsigned>(resolved->animations.size()) : 0u;

    if (!resolved)
        ImGui::TextDisabled("No animation source is resolved.");
    else
    {
        ImGui::TextDisabled("Resolved: %s", resolved->metadata.path.c_str());
        ImGui::TextDisabled("Skeleton: %zu joints | Clips: %zu",
                            resolved->skeleton.joints.size(), resolved->animations.size());
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Playback / Preview");
    animator = currentRecord();
    ImGui::BeginDisabled(readOnly || !resolvedClipCount);
    bool preview = animatorPreviewEnabled_ && animatorPreviewPersistentID_ == selected.persistentID;
    if (ImGui::Checkbox("Preview in Scene View", &preview))
    {
        animatorPreviewEnabled_ = preview;
        animatorPreviewPersistentID_ = preview ? selected.persistentID : 0;
    }
    if (readOnly && session.Playing())
        ImGui::TextDisabled("Runtime Play owns animation preview while the game is running.");

    if (!animator.controller && resolvedClipCount)
    {
        const unsigned current = std::min<unsigned>(animator.state.clip, resolvedClipCount - 1);
        const std::string currentLabel = ClipLabel(resolved->animations[current], current);
        if (ImGui::BeginCombo("Clip", currentLabel.c_str()))
        {
            for (unsigned i = 0; i < resolvedClipCount; ++i)
            {
                const std::string label = ClipLabel(resolved->animations[i], i);
                if (ImGui::Selectable(label.c_str(), animator.state.clip == i))
                {
                    auto value = currentRecord();
                    value.state.clip = i;
                    value.state.time = 0;
                    commit(value, "Animation clip changed.");
                }
            }
            ImGui::EndCombo();
        }
    }
    else if (animator.controller)
        ImGui::TextDisabled("Clip is driven by the assigned controller state.");

    if (ImGui::Button(animator.state.playing ? "Pause" : "Play"))
    {
        auto value = currentRecord();
        value.state.playing = !value.state.playing;
        commit(value, value.state.playing ? "Animation playback resumed." : "Animation playback paused.");
    }
    ImGui::SameLine();
    if (ImGui::Button("Restart"))
    {
        auto value = currentRecord();
        value.state.time = 0;
        value.state.playing = true;
        value.fadeElapsed = value.fadeDuration = 0;
        commit(value, "Animation restarted.");
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop"))
    {
        auto value = currentRecord();
        value.state.time = 0;
        value.state.playing = false;
        value.fadeElapsed = value.fadeDuration = 0;
        commit(value, "Animation stopped at the first frame.");
    }

    animator = currentRecord();
    if (!animator.controller)
    {
        bool loop = animator.state.loop;
        if (ImGui::Checkbox("Loop", &loop))
        {
            auto value = currentRecord();
            value.state.loop = loop;
            commit(value, "Animation loop mode changed.");
        }
        float speed = animator.state.speed;
        if (ImGui::DragFloat("Speed", &speed, 0.02f, -10.0f, 10.0f, "%.2fx"))
        {
            auto value = currentRecord();
            value.state.speed = speed;
            commit(value, "Animation speed changed.");
        }
    }
    animator = currentRecord();
    if (resolvedClipCount && animator.state.clip < resolvedClipCount)
    {
        const float duration = std::max(0.0001f, resolved->animations[animator.state.clip].duration);
        float time = static_cast<float>(std::clamp(animator.state.time, 0.0, static_cast<double>(duration)));
        if (ImGui::SliderFloat("Time", &time, 0.0f, duration, "%.3f s"))
        {
            auto value = currentRecord();
            value.state.time = time;
            commit(value, "Animation preview time changed.");
        }
    }
    ImGui::EndDisabled();

    ImGui::Spacing();
    ImGui::TextDisabled("Controller");
    ImGui::BeginDisabled(readOnly || !resolvedClipCount);
    ImGui::SetNextItemWidth(-80.0f);
    ImGui::InputText("##ControllerAsset", animationControllerPath_.data(), animationControllerPath_.size());
    ImGui::SameLine();
    if (ImGui::Button("Assign"))
    {
        try
        {
            auto asset = LoadAnimationControllerAsset(animationControllerPath_.data(), resolvedClipCount);
            auto value = currentRecord();
            value.controller.emplace(asset.graph, resolvedClipCount);
            value.controllerStarted = false;
            value.fadeElapsed = value.fadeDuration = 0;
            const auto& entry = asset.graph.states.at(asset.graph.entry);
            value.state = {entry.clip, 0, entry.speed, true, entry.loop};
            if (commit(value, "Animation controller assigned."))
            {
                animationGraph_ = std::move(asset);
                animationGraphPersistentID_ = selected.persistentID;
                animationSelectedState_ = static_cast<int>(animationGraph_.graph.entry);
                animationSelectedTransition_ = -1;
            }
        }
        catch (const std::exception& error)
        {
            animatorInspectorStatus_ = error.what();
            LogAnimatorError(engine, error);
        }
    }
    animator = currentRecord();
    if (animator.controller)
    {
        ImGui::SameLine();
        if (ImGui::Button("Clear"))
        {
            auto value = currentRecord();
            value.controller.reset();
            value.controllerStarted = false;
            value.fadeElapsed = value.fadeDuration = 0;
            commit(value, "Animation controller cleared.");
        }
    }
    ImGui::EndDisabled();

    animator = currentRecord();
    if (animator.controller)
    {
        auto value = animator;
        const auto& graph = value.controller->Graph();
        const unsigned currentState = value.controller->CurrentIndex();
        const char* stateName = currentState < graph.states.size() ? graph.states[currentState].name.c_str() : "Invalid";
        ImGui::BeginDisabled(readOnly);
        if (ImGui::BeginCombo("Controller State", stateName))
        {
            for (unsigned i = 0; i < graph.states.size(); ++i)
                if (ImGui::Selectable(graph.states[i].name.c_str(), i == currentState))
                {
                    auto changed = currentRecord();
                    changed.controller->SetState(i);
                    changed.controllerStarted = false;
                    changed.fadeElapsed = changed.fadeDuration = 0;
                    commit(changed, "Controller preview state changed.");
                }
            ImGui::EndCombo();
        }

        if (!graph.parameters.empty())
        {
            ImGui::SeparatorText("Controller Parameters");
            for (const auto& parameter : graph.parameters)
            {
                ImGui::PushID(parameter.name.c_str());
                auto changed = currentRecord();
                float currentValue = changed.controller->Value(parameter.name);
                bool parameterChanged = false;
                switch (parameter.type)
                {
                case AnimationParameterType::Float:
                    parameterChanged = ImGui::DragFloat(parameter.name.c_str(), &currentValue, 0.02f);
                    if (parameterChanged) changed.controller->SetFloat(parameter.name, currentValue);
                    break;
                case AnimationParameterType::Bool:
                {
                    bool boolean = currentValue != 0.0f;
                    parameterChanged = ImGui::Checkbox(parameter.name.c_str(), &boolean);
                    if (parameterChanged) changed.controller->SetBool(parameter.name, boolean);
                    break;
                }
                case AnimationParameterType::Trigger:
                {
                    ImGui::Text("%s", parameter.name.c_str());
                    ImGui::SameLine();
                    const bool armed = currentValue != 0.0f;
                    if (ImGui::SmallButton(armed ? "Reset Trigger" : "Fire Trigger"))
                    {
                        if (armed) changed.controller->ResetTrigger(parameter.name);
                        else changed.controller->Trigger(parameter.name);
                        parameterChanged = true;
                    }
                    break;
                }
                }
                if (parameterChanged)
                    commit(changed, "Controller parameter changed.");
                ImGui::SameLine();
                ImGui::TextDisabled("%s", ParameterTypeName(parameter.type));
                ImGui::PopID();
            }
        }
        ImGui::EndDisabled();
    }

    ImGui::Spacing();
    ImGui::SeparatorText("Skeleton Diagnostics");
    if (!resolved)
        ImGui::TextDisabled("No skeleton is resolved.");
    else
    {
        const auto& joints = resolved->skeleton.joints;
        const std::size_t roots = static_cast<std::size_t>(std::count_if(joints.begin(), joints.end(),
            [](const AssetNode& joint) { return joint.parent < 0; }));
        ImGui::Text("Joints: %zu", joints.size());
        ImGui::Text("Roots: %zu", roots);
        ImGui::Text("Clips: %zu", resolved->animations.size());
        if (joints.empty())
            ImGui::TextDisabled("[!] Source has no skeleton joints.");
        if (roots != 1 && !joints.empty())
            ImGui::TextDisabled("[!] Skeleton has %zu roots; verify the imported hierarchy.", roots);
        for (const auto& clip : resolved->animations)
            if (clip.tracks.size() != joints.size())
                ImGui::TextDisabled("[!] Clip '%s' has %zu tracks for %zu joints.",
                                    clip.name.c_str(), clip.tracks.size(), joints.size());

        if (ImGui::TreeNode("Clip Diagnostics"))
        {
            for (std::size_t i = 0; i < resolved->animations.size(); ++i)
            {
                const auto& clip = resolved->animations[i];
                ImGui::BulletText("%zu: %s | %.3f s | %zu tracks", i,
                                  clip.name.empty() ? "Unnamed Clip" : clip.name.c_str(),
                                  clip.duration, clip.tracks.size());
            }
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Skeleton Joint Hierarchy"))
        {
            for (std::size_t i = 0; i < joints.size(); ++i)
            {
                const unsigned depth = JointDepth(joints, i);
                ImGui::Indent(static_cast<float>(depth) * 12.0f);
                ImGui::BulletText("%zu  %s", i, joints[i].name.c_str());
                ImGui::Unindent(static_cast<float>(depth) * 12.0f);
            }
            ImGui::TreePop();
        }
        for (const auto& warning : resolved->warnings)
            ImGui::TextDisabled("Import: %s", warning.c_str());
    }

    if (!animatorInspectorStatus_.empty())
    {
        ImGui::Spacing();
        ImGui::TextWrapped("%s", animatorInspectorStatus_.c_str());
    }
    if (hierarchyLocked)
        ImGui::TextDisabled("Hierarchy lock keeps Animator authoring read-only.");
    else if (!editable)
        ImGui::TextDisabled("Play mode: Animator authoring is read-only; runtime state remains visible.");

    ImGui::PopID();
    ImGui::End();
}

} // namespace PlagC::Editor
