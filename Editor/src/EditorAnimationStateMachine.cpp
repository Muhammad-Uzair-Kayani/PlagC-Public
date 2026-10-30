#include "pgcpch.h"
#include "EditorLayer.h"
#include <PlagC/Application.h>
#include <imgui.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>

namespace PlagC::Editor {
namespace {

const AuthoringEntity* SelectedAnimated(const std::vector<AuthoringEntity>& entities)
{
    const auto it = std::find_if(entities.begin(), entities.end(), [](const AuthoringEntity& entity) {
        return entity.selected && entity.record.animator.has_value();
    });
    return it == entities.end() ? nullptr : &*it;
}

AssetHandle<ModelAsset> AnimationAsset(const AuthoringEntity& entity)
{
    if (!entity.record.animator)
        return {};
    if (entity.record.animator->animationSource)
        return entity.record.animator->animationSource;
    if (entity.record.model)
        return entity.record.model->asset;
    return {};
}

unsigned ClipCount(const AuthoringEntity& entity)
{
    const auto asset = AnimationAsset(entity);
    return asset ? static_cast<unsigned>(asset->animations.size()) : 0u;
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

const char* CompareName(AnimationCompare compare)
{
    switch (compare)
    {
    case AnimationCompare::Greater: return ">";
    case AnimationCompare::Less: return "<";
    case AnimationCompare::Equal: return "==";
    case AnimationCompare::NotEqual: return "!=";
    }
    return "?";
}

void LogGraphError(Engine& engine, const std::exception& error)
{
    engine.Logger().Write(Severity::Error, "EDITOR-ANIMATION", error.what());
}

AnimationControllerAsset GraphFromAnimator(const AuthoringEntity& entity)
{
    AnimationControllerAsset asset;
    if (entity.record.animator && entity.record.animator->controller)
        asset.graph = entity.record.animator->controller->Graph();
    else
    {
        AnimationNode node;
        node.name = "Entry";
        node.clip = entity.record.animator ? entity.record.animator->state.clip : 0u;
        node.loop = entity.record.animator ? entity.record.animator->state.loop : true;
        node.speed = entity.record.animator ? entity.record.animator->state.speed : 1.0f;
        asset.graph.states.push_back(std::move(node));
        asset.graph.entry = 0;
    }
    asset.layout.reserve(asset.graph.states.size());
    for (std::size_t i = 0; i < asset.graph.states.size(); ++i)
        asset.layout.push_back({40.0f + float(i % 4) * 190.0f, 50.0f + float(i / 4) * 120.0f});
    return asset;
}

void ApplyGraph(Engine& engine, EditorSession& session, const AuthoringEntity& entity,
                const AnimationControllerAsset& asset)
{
    const unsigned clips = ClipCount(entity);
    if (!clips)
        throw std::invalid_argument("Animation State Machine requires an animated ModelAsset");
    asset.Validate(clips);
    AnimatorComponent animator = *entity.record.animator;
    animator.controller.emplace(asset.graph, clips);
    animator.controllerStarted = false;
    animator.fadeElapsed = 0;
    animator.fadeDuration = 0;
    session.EditDocument().SetAnimator(entity.entity, animator);
    engine.Logger().Write(Severity::Debug, "EDITOR-ANIMATION", "Animation controller graph applied.");
}

void RemoveState(AnimationControllerAsset& asset, unsigned index)
{
    if (asset.graph.states.size() <= 1)
        throw std::invalid_argument("Animation controller must contain at least one state");
    if (index >= asset.graph.states.size())
        return;
    asset.graph.states.erase(asset.graph.states.begin() + index);
    asset.layout.erase(asset.layout.begin() + index);
    std::erase_if(asset.graph.transitions, [index](const AnimationTransition& transition) {
        return transition.from == index || transition.to == index;
    });
    for (auto& transition : asset.graph.transitions)
    {
        if (transition.from > index) --transition.from;
        if (transition.to > index) --transition.to;
    }
    if (asset.graph.entry == index)
        asset.graph.entry = 0;
    else if (asset.graph.entry > index)
        --asset.graph.entry;
}

ImVec2 NodeCenter(ImVec2 origin, const AnimationNodeLayout& layout)
{
    return {origin.x + layout.x + 72.0f, origin.y + layout.y + 31.0f};
}

void DrawGraphCanvas(AnimationControllerAsset& asset, const char* activeState,
                     int& selectedState, int& selectedTransition, bool editable)
{
    constexpr ImVec2 nodeSize{144.0f, 62.0f};
    ImGui::BeginChild("AnimationGraphCanvas", ImVec2(0, 330), true,
                      ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoScrollbar);
    const ImVec2 origin = ImGui::GetCursorScreenPos();
    const ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(origin, {origin.x + canvasSize.x, origin.y + canvasSize.y},
                        IM_COL32(25, 27, 31, 255));

    for (std::size_t i = 0; i < asset.graph.transitions.size(); ++i)
    {
        const auto& transition = asset.graph.transitions[i];
        if (transition.from >= asset.layout.size() || transition.to >= asset.layout.size())
            continue;
        const ImVec2 a = NodeCenter(origin, asset.layout[transition.from]);
        const ImVec2 b = NodeCenter(origin, asset.layout[transition.to]);
        const bool chosen = selectedTransition == static_cast<int>(i);
        draw->AddLine(a, b, chosen ? IM_COL32(255, 190, 70, 255) : IM_COL32(130, 145, 165, 210),
                      chosen ? 3.0f : 2.0f);
        const ImVec2 mid{(a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f};
        draw->AddCircleFilled(mid, chosen ? 5.0f : 3.0f,
                              chosen ? IM_COL32(255, 190, 70, 255) : IM_COL32(150, 165, 185, 255));
    }

    for (std::size_t i = 0; i < asset.graph.states.size(); ++i)
    {
        auto& layout = asset.layout[i];
        const auto& state = asset.graph.states[i];
        const ImVec2 p{origin.x + layout.x, origin.y + layout.y};
        ImGui::SetCursorScreenPos(p);
        ImGui::PushID(static_cast<int>(i));
        ImGui::InvisibleButton("node", nodeSize,
            editable ? ImGuiButtonFlags_MouseButtonLeft : ImGuiButtonFlags_None);
        if (ImGui::IsItemClicked())
        {
            selectedState = static_cast<int>(i);
            selectedTransition = -1;
        }
        if (editable && ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        {
            layout.x = std::max(0.0f, layout.x + ImGui::GetIO().MouseDelta.x);
            layout.y = std::max(0.0f, layout.y + ImGui::GetIO().MouseDelta.y);
        }

        const bool selected = selectedState == static_cast<int>(i);
        const bool active = activeState && state.name == activeState;
        ImU32 fill = IM_COL32(52, 58, 68, 255);
        if (selected) fill = IM_COL32(72, 82, 98, 255);
        if (active) fill = IM_COL32(45, 102, 67, 255);
        draw->AddRectFilled(p, {p.x + nodeSize.x, p.y + nodeSize.y}, fill, 6.0f);
        draw->AddRect(p, {p.x + nodeSize.x, p.y + nodeSize.y},
                      i == asset.graph.entry ? IM_COL32(80, 220, 120, 255) : IM_COL32(105, 115, 130, 255),
                      6.0f, 0, i == asset.graph.entry ? 2.5f : 1.5f);
        draw->AddText({p.x + 10, p.y + 9}, IM_COL32(240, 242, 246, 255), state.name.c_str());
        char detail[96]{};
        std::snprintf(detail, sizeof(detail), "Clip %u  %.2fx%s", state.clip, state.speed,
                      state.loop ? "  loop" : "");
        draw->AddText({p.x + 10, p.y + 34}, IM_COL32(175, 185, 200, 255), detail);
        if (active)
            draw->AddCircleFilled({p.x + nodeSize.x - 12, p.y + 12}, 5.0f, IM_COL32(80, 235, 125, 255));
        ImGui::PopID();
    }
    ImGui::EndChild();
}

} // namespace

void EditorLayer::AppendAnimationStateMachine(Engine& engine, EditorSession& session)
{
    if (!showAnimationStateMachine_)
        return;

    const auto entities = session.ViewDocument().Snapshot();
    const AuthoringEntity* selected = SelectedAnimated(entities);
    const bool editable = session.EditingEnabled();

    if (!ImGui::Begin("Animation State Machine", &showAnimationStateMachine_))
    {
        ImGui::End();
        return;
    }
    if (!selected)
    {
        ImGui::TextDisabled("Select an entity with an Animator component to edit its controller.");
        ImGui::End();
        return;
    }

    const unsigned clips = ClipCount(*selected);
    if (!clips)
    {
        ImGui::TextDisabled("The selected Animator has no animation clips.");
        ImGui::End();
        return;
    }

    if (animationGraphPersistentID_ != selected->persistentID)
    {
        animationGraphPersistentID_ = selected->persistentID;
        animationGraph_ = GraphFromAnimator(*selected);
        animationSelectedState_ = animationGraph_.graph.states.empty() ? -1 : 0;
        animationSelectedTransition_ = -1;
    }

    const char* activeState = nullptr;
    if (selected->record.animator && selected->record.animator->controller)
        activeState = selected->record.animator->controller->Current().name.c_str();

    ImGui::Text("Entity: %s", selected->record.name.c_str());
    ImGui::SameLine();
    if (activeState)
        ImGui::Text(" | Runtime state: %s", activeState);
    else
        ImGui::TextDisabled(" | Controller not applied");

    ImGui::SetNextItemWidth(360.0f);
    ImGui::InputText("Controller Asset", animationControllerPath_.data(), animationControllerPath_.size());
    ImGui::BeginDisabled(!editable);
    if (ImGui::Button("New"))
    {
        animationGraph_ = GraphFromAnimator(*selected);
        animationGraph_.graph.parameters.clear();
        animationGraph_.graph.transitions.clear();
        animationSelectedState_ = 0;
        animationSelectedTransition_ = -1;
    }
    ImGui::SameLine();
    if (ImGui::Button("Load"))
    {
        try
        {
            animationGraph_ = LoadAnimationControllerAsset(animationControllerPath_.data(), clips);
            animationSelectedState_ = animationGraph_.graph.states.empty() ? -1 : 0;
            animationSelectedTransition_ = -1;
            ApplyGraph(engine, session, *selected, animationGraph_);
        }
        catch (const std::exception& error) { LogGraphError(engine, error); }
    }
    ImGui::SameLine();
    if (ImGui::Button("Save"))
    {
        try
        {
            SaveAnimationControllerAsset(animationControllerPath_.data(), animationGraph_, clips);
            engine.Logger().Write(Severity::Info, "EDITOR-ANIMATION",
                                  std::string("Saved animation controller: ") + animationControllerPath_.data());
        }
        catch (const std::exception& error) { LogGraphError(engine, error); }
    }
    ImGui::SameLine();
    if (ImGui::Button("Apply to Animator"))
    {
        try { ApplyGraph(engine, session, *selected, animationGraph_); }
        catch (const std::exception& error) { LogGraphError(engine, error); }
    }
    ImGui::EndDisabled();

    DrawGraphCanvas(animationGraph_, activeState, animationSelectedState_,
                    animationSelectedTransition_, editable);

    if (ImGui::BeginTabBar("AnimationGraphTabs"))
    {
        if (ImGui::BeginTabItem("States"))
        {
            ImGui::BeginDisabled(!editable);
            if (ImGui::Button("Add State"))
            {
                AnimationNode node;
                node.name = "State " + std::to_string(animationGraph_.graph.states.size());
                node.clip = 0;
                animationGraph_.graph.states.push_back(std::move(node));
                animationGraph_.layout.push_back({40.0f + float(animationGraph_.graph.states.size() % 4) * 180.0f,
                                                  50.0f + float(animationGraph_.graph.states.size() / 4) * 110.0f});
                animationSelectedState_ = static_cast<int>(animationGraph_.graph.states.size() - 1);
            }
            if (animationSelectedState_ >= 0 &&
                animationSelectedState_ < static_cast<int>(animationGraph_.graph.states.size()))
            {
                auto& state = animationGraph_.graph.states[static_cast<std::size_t>(animationSelectedState_)];
                ImGui::SeparatorText("State Inspector");
                char name[128]{};
                std::snprintf(name, sizeof(name), "%s", state.name.c_str());
                if (ImGui::InputText("Name", name, sizeof(name))) state.name = name;
                int clip = static_cast<int>(state.clip);
                if (ImGui::DragInt("Clip", &clip, 1.0f, 0, static_cast<int>(clips - 1)))
                    state.clip = static_cast<unsigned>(std::clamp(clip, 0, static_cast<int>(clips - 1)));
                if (const auto asset = AnimationAsset(*selected); asset && state.clip < asset->animations.size())
                {
                    ImGui::SameLine(); ImGui::TextDisabled("%s", asset->animations[state.clip].name.c_str());
                }
                ImGui::Checkbox("Loop", &state.loop);
                ImGui::DragFloat("Speed", &state.speed, 0.01f, -10.0f, 10.0f);
                bool entry = animationGraph_.graph.entry == static_cast<unsigned>(animationSelectedState_);
                if (ImGui::Checkbox("Entry State", &entry) && entry)
                    animationGraph_.graph.entry = static_cast<unsigned>(animationSelectedState_);
                if (ImGui::Button("Remove State"))
                {
                    try
                    {
                        RemoveState(animationGraph_, static_cast<unsigned>(animationSelectedState_));
                        animationSelectedState_ = animationGraph_.graph.states.empty() ? -1 : 0;
                        animationSelectedTransition_ = -1;
                    }
                    catch (const std::exception& error) { LogGraphError(engine, error); }
                }
            }
            ImGui::EndDisabled();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Parameters"))
        {
            ImGui::BeginDisabled(!editable);
            if (ImGui::Button("Add Parameter"))
            {
                AnimationParameter parameter;
                parameter.name = "Parameter" + std::to_string(animationGraph_.graph.parameters.size());
                parameter.type = AnimationParameterType::Float;
                animationGraph_.graph.parameters.push_back(std::move(parameter));
            }
            for (std::size_t i = 0; i < animationGraph_.graph.parameters.size(); ++i)
            {
                auto& parameter = animationGraph_.graph.parameters[i];
                ImGui::PushID(static_cast<int>(i));
                char name[96]{};
                std::snprintf(name, sizeof(name), "%s", parameter.name.c_str());
                ImGui::SetNextItemWidth(150.0f);
                if (ImGui::InputText("##Name", name, sizeof(name))) parameter.name = name;
                ImGui::SameLine();
                ImGui::SetNextItemWidth(100.0f);
                if (ImGui::BeginCombo("##Type", ParameterTypeName(parameter.type)))
                {
                    for (auto type : {AnimationParameterType::Float, AnimationParameterType::Bool, AnimationParameterType::Trigger})
                        if (ImGui::Selectable(ParameterTypeName(type), parameter.type == type)) parameter.type = type;
                    ImGui::EndCombo();
                }
                ImGui::SameLine(); ImGui::SetNextItemWidth(90.0f);
                if (parameter.type == AnimationParameterType::Float)
                    ImGui::DragFloat("##Initial", &parameter.initial, 0.05f);
                else
                {
                    bool value = parameter.initial != 0.0f;
                    if (ImGui::Checkbox("Initial", &value)) parameter.initial = value ? 1.0f : 0.0f;
                }
                ImGui::SameLine();
                if (ImGui::SmallButton("Remove"))
                {
                    const std::string removed = parameter.name;
                    animationGraph_.graph.parameters.erase(animationGraph_.graph.parameters.begin() + i);
                    for (auto& transition : animationGraph_.graph.transitions)
                        std::erase_if(transition.conditions, [&](const AnimationCondition& c){ return c.parameter == removed; });
                    ImGui::PopID();
                    break;
                }
                ImGui::PopID();
            }
            ImGui::EndDisabled();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Transitions"))
        {
            ImGui::BeginDisabled(!editable);
            if (animationGraph_.graph.states.size() >= 2 && ImGui::Button("Add Transition"))
            {
                AnimationTransition transition;
                transition.from = animationSelectedState_ >= 0 ? static_cast<unsigned>(animationSelectedState_) : 0u;
                transition.to = transition.from == 0 ? 1u : 0u;
                animationGraph_.graph.transitions.push_back(std::move(transition));
                animationSelectedTransition_ = static_cast<int>(animationGraph_.graph.transitions.size() - 1);
            }
            for (std::size_t i = 0; i < animationGraph_.graph.transitions.size(); ++i)
            {
                const auto& transition = animationGraph_.graph.transitions[i];
                const std::string label = animationGraph_.graph.states[transition.from].name + " -> " +
                                          animationGraph_.graph.states[transition.to].name + "##Transition" + std::to_string(i);
                if (ImGui::Selectable(label.c_str(), animationSelectedTransition_ == static_cast<int>(i)))
                {
                    animationSelectedTransition_ = static_cast<int>(i);
                    animationSelectedState_ = -1;
                }
            }
            if (animationSelectedTransition_ >= 0 &&
                animationSelectedTransition_ < static_cast<int>(animationGraph_.graph.transitions.size()))
            {
                auto& transition = animationGraph_.graph.transitions[static_cast<std::size_t>(animationSelectedTransition_)];
                ImGui::SeparatorText("Transition Inspector");
                auto stateCombo = [&](const char* label, unsigned& value)
                {
                    if (ImGui::BeginCombo(label, animationGraph_.graph.states[value].name.c_str()))
                    {
                        for (unsigned i = 0; i < animationGraph_.graph.states.size(); ++i)
                            if (ImGui::Selectable(animationGraph_.graph.states[i].name.c_str(), value == i)) value = i;
                        ImGui::EndCombo();
                    }
                };
                stateCombo("From", transition.from);
                stateCombo("To", transition.to);
                ImGui::DragFloat("Transition Duration", &transition.crossfade, 0.01f, 0.0f, 10.0f, "%.2f s");
                if (ImGui::Button("Add Condition") && !animationGraph_.graph.parameters.empty())
                    transition.conditions.push_back({animationGraph_.graph.parameters.front().name, AnimationCompare::Equal, 1.0f});
                for (std::size_t c = 0; c < transition.conditions.size(); ++c)
                {
                    auto& condition = transition.conditions[c];
                    ImGui::PushID(static_cast<int>(c));
                    ImGui::SetNextItemWidth(140.0f);
                    if (ImGui::BeginCombo("##Parameter", condition.parameter.c_str()))
                    {
                        for (const auto& parameter : animationGraph_.graph.parameters)
                            if (ImGui::Selectable(parameter.name.c_str(), condition.parameter == parameter.name)) condition.parameter = parameter.name;
                        ImGui::EndCombo();
                    }
                    ImGui::SameLine(); ImGui::SetNextItemWidth(70.0f);
                    if (ImGui::BeginCombo("##Compare", CompareName(condition.compare)))
                    {
                        for (auto compare : {AnimationCompare::Greater, AnimationCompare::Less,
                                             AnimationCompare::Equal, AnimationCompare::NotEqual})
                            if (ImGui::Selectable(CompareName(compare), condition.compare == compare)) condition.compare = compare;
                        ImGui::EndCombo();
                    }
                    ImGui::SameLine(); ImGui::SetNextItemWidth(90.0f);
                    ImGui::DragFloat("##Value", &condition.value, 0.05f);
                    ImGui::SameLine();
                    if (ImGui::SmallButton("X"))
                    {
                        transition.conditions.erase(transition.conditions.begin() + c);
                        ImGui::PopID();
                        break;
                    }
                    ImGui::PopID();
                }
                if (ImGui::Button("Remove Transition"))
                {
                    animationGraph_.graph.transitions.erase(animationGraph_.graph.transitions.begin() + animationSelectedTransition_);
                    animationSelectedTransition_ = -1;
                }
            }
            ImGui::EndDisabled();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    if (!editable)
        ImGui::TextDisabled("Play mode: graph authoring is locked; active runtime state remains visible.");
    ImGui::End();
}

} // namespace PlagC::Editor
