#include "pgcpch.h"
#include "EditorLayer.h"
#include <PlagC/Application.h>
#include <imgui.h>
#include <algorithm>
#include <cfloat>

namespace PlagC::Editor {
namespace {

void LogComponentError(Engine& engine, const std::exception& error)
{
    engine.Logger().Write(Severity::Error, "EDITOR", error.what());
}

const char* MotionName(MotionType motion)
{
    switch (motion)
    {
    case MotionType::Static: return "Static";
    case MotionType::Dynamic: return "Dynamic";
    case MotionType::Kinematic: return "Kinematic";
    }
    return "Unknown";
}

const char* LightName(LightType type)
{
    switch (type)
    {
    case LightType::Directional: return "Directional";
    case LightType::Point: return "Point";
    case LightType::Spot: return "Spot";
    }
    return "Unknown";
}

const char* ColliderName(ColliderType type)
{
    switch (type)
    {
    case ColliderType::Box: return "Box";
    case ColliderType::Sphere: return "Sphere";
    case ColliderType::Capsule: return "Capsule";
    case ColliderType::Cylinder: return "Cylinder";
    default: return "Advanced";
    }
}

} // namespace

void EditorLayer::AppendComponentInspector(Engine& engine, EditorSession& session)
{
    if (!showInspector_ || !session.EditingEnabled())
        return;

    const auto entities = session.ViewDocument().Snapshot();
    const auto selected = std::find_if(entities.begin(), entities.end(),
        [](const AuthoringEntity& entity) { return entity.selected; });
    if (selected == entities.end())
        return;

    auto& document = session.EditDocument();
    if (componentPersistentID_ != selected->persistentID ||
        componentRevision_ != document.Revision())
    {
        componentPersistentID_ = selected->persistentID;
        componentRevision_ = document.Revision();
        componentCamera_ = selected->record.camera;
        componentLight_ = selected->record.light;
        componentPhysics_ = selected->record.physics;
        componentAnimator_ = selected->record.animator;
    }

    if (!ImGui::Begin("Inspector"))
    {
        ImGui::End();
        return;
    }

    ImGui::PushID("ComponentAuthoring");
    ImGui::SeparatorText("Component Authoring");

    auto resyncOnError = [&](const std::exception& error)
    {
        LogComponentError(engine, error);
        componentRevision_ = 0;
    };

    // Camera -----------------------------------------------------------------
    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (!componentCamera_)
        {
            if (ImGui::Button("Add Camera"))
            {
                try
                {
                    CameraComponent value{};
                    document.SetCamera(selected->entity, value);
                    componentCamera_ = value;
                    componentRevision_ = document.Revision();
                }
                catch (const std::exception& error) { resyncOnError(error); }
            }
        }
        else
        {
            auto commit = [&]
            {
                try
                {
                    document.SetCamera(selected->entity, *componentCamera_);
                    componentRevision_ = document.Revision();
                }
                catch (const std::exception& error) { resyncOnError(error); }
            };

            ImGui::DragFloat3("Target", &componentCamera_->target.x, 0.05f);
            if (ImGui::IsItemDeactivatedAfterEdit()) commit();
            ImGui::DragFloat("FOV", &componentCamera_->fov, 0.25f, 1.0f, 179.0f);
            if (ImGui::IsItemDeactivatedAfterEdit()) commit();
            ImGui::DragFloat("Near Clip", &componentCamera_->nearClip, 0.01f, 0.001f, FLT_MAX);
            if (ImGui::IsItemDeactivatedAfterEdit()) commit();
            ImGui::DragFloat("Far Clip", &componentCamera_->farClip, 1.0f, 0.01f, FLT_MAX);
            if (ImGui::IsItemDeactivatedAfterEdit()) commit();
            ImGui::DragFloat("Aspect Override", &componentCamera_->aspect, 0.01f, 0.0f, 10.0f);
            if (ImGui::IsItemDeactivatedAfterEdit()) commit();

            if (selected->activeCamera)
            {
                if (ImGui::Button("Clear Active Camera"))
                {
                    try
                    {
                        document.ClearActiveCamera();
                        componentRevision_ = document.Revision();
                    }
                    catch (const std::exception& error) { resyncOnError(error); }
                }
            }
            else if (ImGui::Button("Make Active Camera"))
            {
                try
                {
                    document.SetActiveCamera(selected->entity);
                    componentRevision_ = document.Revision();
                }
                catch (const std::exception& error) { resyncOnError(error); }
            }

            ImGui::SameLine();
            if (ImGui::Button("Remove Camera"))
            {
                try
                {
                    document.RemoveCamera(selected->entity);
                    componentCamera_.reset();
                    componentRevision_ = document.Revision();
                }
                catch (const std::exception& error) { resyncOnError(error); }
            }
        }
    }

    // Light ------------------------------------------------------------------
    if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (!componentLight_)
        {
            if (ImGui::Button("Add Light"))
            {
                try
                {
                    LightComponent value{};
                    document.SetLight(selected->entity, value);
                    componentLight_ = value;
                    componentRevision_ = document.Revision();
                }
                catch (const std::exception& error) { resyncOnError(error); }
            }
        }
        else
        {
            auto commit = [&]
            {
                try
                {
                    document.SetLight(selected->entity, *componentLight_);
                    componentRevision_ = document.Revision();
                }
                catch (const std::exception& error) { resyncOnError(error); }
            };

            if (ImGui::BeginCombo("Type", LightName(componentLight_->light.type)))
            {
                for (LightType type : {LightType::Directional, LightType::Point, LightType::Spot})
                {
                    const bool current = componentLight_->light.type == type;
                    if (ImGui::Selectable(LightName(type), current))
                    {
                        componentLight_->light.type = type;
                        commit();
                    }
                    if (current) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::DragFloat3("Direction", &componentLight_->light.direction.x, 0.01f);
            if (ImGui::IsItemDeactivatedAfterEdit()) commit();
            ImGui::ColorEdit3("Colour", &componentLight_->light.colour.x);
            if (ImGui::IsItemDeactivatedAfterEdit()) commit();
            ImGui::DragFloat("Power", &componentLight_->light.power, 0.05f, 0.0f, FLT_MAX);
            if (ImGui::IsItemDeactivatedAfterEdit()) commit();
            ImGui::DragFloat("Radius", &componentLight_->light.radius, 0.1f, 0.001f, FLT_MAX);
            if (ImGui::IsItemDeactivatedAfterEdit()) commit();
            if (componentLight_->light.type == LightType::Spot)
            {
                ImGui::DragFloat("Inner Angle", &componentLight_->light.innerAngle, 0.25f, 0.0f, 179.0f);
                if (ImGui::IsItemDeactivatedAfterEdit()) commit();
                ImGui::DragFloat("Outer Angle", &componentLight_->light.outerAngle, 0.25f, 0.0f, 179.0f);
                if (ImGui::IsItemDeactivatedAfterEdit()) commit();
            }
            if (ImGui::Button("Remove Light"))
            {
                try
                {
                    document.RemoveLight(selected->entity);
                    componentLight_.reset();
                    componentRevision_ = document.Revision();
                }
                catch (const std::exception& error) { resyncOnError(error); }
            }
        }
    }

    // Model ------------------------------------------------------------------
    if (ImGui::CollapsingHeader("Model", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (!selected->record.model)
        {
            ImGui::TextDisabled("No model assigned. Assign a ModelAsset from the Content Browser.");
        }
        else
        {
            bool visible = selected->record.model->visible;
            if (ImGui::Checkbox("Visible", &visible))
            {
                try
                {
                    document.SetModelVisible(selected->entity, visible);
                    componentRevision_ = document.Revision();
                }
                catch (const std::exception& error) { resyncOnError(error); }
            }
            if (ImGui::Button("Remove Model"))
            {
                try
                {
                    document.RemoveModel(selected->entity);
                    componentAnimator_.reset();
                    componentRevision_ = document.Revision();
                }
                catch (const std::exception& error) { resyncOnError(error); }
            }
        }
    }

    // Physics ----------------------------------------------------------------
    if (ImGui::CollapsingHeader("Physics", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (!componentPhysics_)
        {
            if (ImGui::Button("Add Physics"))
            {
                try
                {
                    PhysicsComponent value{};
                    document.SetPhysics(selected->entity, value);
                    componentPhysics_ = value;
                    componentRevision_ = document.Revision();
                }
                catch (const std::exception& error) { resyncOnError(error); }
            }
            ImGui::SameLine();
            ImGui::TextDisabled("Physics requires a hierarchy root with no children.");
        }
        else
        {
            auto commit = [&]
            {
                try
                {
                    document.SetPhysics(selected->entity, *componentPhysics_);
                    componentRevision_ = document.Revision();
                }
                catch (const std::exception& error) { resyncOnError(error); }
            };

            if (ImGui::BeginCombo("Motion", MotionName(componentPhysics_->body.motion)))
            {
                for (MotionType motion : {MotionType::Static, MotionType::Dynamic, MotionType::Kinematic})
                {
                    const bool current = componentPhysics_->body.motion == motion;
                    if (ImGui::Selectable(MotionName(motion), current))
                    {
                        componentPhysics_->body.motion = motion;
                        commit();
                    }
                    if (current) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::DragFloat("Mass", &componentPhysics_->body.mass, 0.05f, 0.001f, FLT_MAX);
            if (ImGui::IsItemDeactivatedAfterEdit()) commit();
            ImGui::DragFloat("Gravity Scale", &componentPhysics_->body.gravityScale, 0.01f, -10.0f, 10.0f);
            if (ImGui::IsItemDeactivatedAfterEdit()) commit();
            ImGui::DragFloat("Linear Damping", &componentPhysics_->body.linearDamping, 0.005f, 0.0f, FLT_MAX);
            if (ImGui::IsItemDeactivatedAfterEdit()) commit();
            ImGui::DragFloat("Angular Damping", &componentPhysics_->body.angularDamping, 0.005f, 0.0f, FLT_MAX);
            if (ImGui::IsItemDeactivatedAfterEdit()) commit();
            if (ImGui::Checkbox("Continuous Collision", &componentPhysics_->body.continuous)) commit();
            if (ImGui::Checkbox("Sensor", &componentPhysics_->body.sensor)) commit();

            if (ImGui::BeginCombo("Collider", ColliderName(componentPhysics_->collider.type)))
            {
                for (ColliderType type : {ColliderType::Box, ColliderType::Sphere,
                                          ColliderType::Capsule, ColliderType::Cylinder})
                {
                    const bool current = componentPhysics_->collider.type == type;
                    if (ImGui::Selectable(ColliderName(type), current))
                    {
                        componentPhysics_->collider.type = type;
                        commit();
                    }
                    if (current) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::DragFloat3("Collider Size", &componentPhysics_->collider.dimensions.x, 0.01f, 0.001f, FLT_MAX);
            if (ImGui::IsItemDeactivatedAfterEdit()) commit();
            ImGui::DragFloat("Friction", &componentPhysics_->collider.material.friction, 0.01f, 0.0f, FLT_MAX);
            if (ImGui::IsItemDeactivatedAfterEdit()) commit();
            ImGui::DragFloat("Restitution", &componentPhysics_->collider.material.restitution, 0.01f, 0.0f, 1.0f);
            if (ImGui::IsItemDeactivatedAfterEdit()) commit();

            if (ImGui::Button("Remove Physics"))
            {
                try
                {
                    document.RemovePhysics(selected->entity);
                    componentPhysics_.reset();
                    componentRevision_ = document.Revision();
                }
                catch (const std::exception& error) { resyncOnError(error); }
            }
        }
    }

    // Animator ---------------------------------------------------------------
    if (ImGui::CollapsingHeader("Animator", ImGuiTreeNodeFlags_DefaultOpen))
    {
        if (!componentAnimator_)
        {
            bool canAdd = false;
            if (selected->record.model && selected->record.model->asset)
                canAdd = !selected->record.model->asset->animations.empty();
            ImGui::BeginDisabled(!canAdd);
            if (ImGui::Button("Add Animator"))
            {
                try
                {
                    AnimatorComponent value{};
                    document.SetAnimator(selected->entity, value);
                    componentAnimator_ = value;
                    componentRevision_ = document.Revision();
                }
                catch (const std::exception& error) { resyncOnError(error); }
            }
            ImGui::EndDisabled();
            if (!canAdd)
                ImGui::TextDisabled("A model with animation clips is required.");
        }
        else
        {
            auto commit = [&]
            {
                try
                {
                    document.SetAnimator(selected->entity, *componentAnimator_);
                    componentRevision_ = document.Revision();
                }
                catch (const std::exception& error) { resyncOnError(error); }
            };

            unsigned clipCount = 0;
            if (componentAnimator_->animationSource)
                clipCount = static_cast<unsigned>(componentAnimator_->animationSource->animations.size());
            else if (selected->record.model && selected->record.model->asset)
                clipCount = static_cast<unsigned>(selected->record.model->asset->animations.size());

            int clip = static_cast<int>(componentAnimator_->state.clip);
            const int maxClip = clipCount ? static_cast<int>(clipCount - 1u) : 0;
            if (ImGui::DragInt("Clip", &clip, 1.0f, 0, maxClip))
                componentAnimator_->state.clip = static_cast<unsigned>(std::clamp(clip, 0, maxClip));
            if (ImGui::IsItemDeactivatedAfterEdit()) commit();
            ImGui::DragFloat("Speed", &componentAnimator_->state.speed, 0.01f, -10.0f, 10.0f);
            if (ImGui::IsItemDeactivatedAfterEdit()) commit();
            if (ImGui::Checkbox("Playing", &componentAnimator_->state.playing)) commit();
            if (ImGui::Checkbox("Loop", &componentAnimator_->state.loop)) commit();

            if (ImGui::Button("Remove Animator"))
            {
                try
                {
                    document.RemoveAnimator(selected->entity);
                    componentAnimator_.reset();
                    componentRevision_ = document.Revision();
                }
                catch (const std::exception& error) { resyncOnError(error); }
            }
        }
    }

    ImGui::PopID();
    ImGui::End();
}

} // namespace PlagC::Editor
