#include "pgcpch.h"
#include "EditorLayer.h"
#include <PlagC/Application.h>
#include <imgui.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <string>

namespace PlagC::Editor {
namespace {

void LogComponentError(Engine& engine, const std::exception& error)
{
    engine.Logger().Write(Severity::Error, "EDITOR-INSPECTOR", error.what());
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

void HelpMarker(const char* text)
{
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("%s", text);
}

struct CardActions
{
    bool open = false;
    bool reset = false;
    bool remove = false;
};

CardActions BeginComponentCard(const char* title, const char* id, bool readOnly,
                               const char* help, bool removable = true)
{
    ImGui::PushID(id);
    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    CardActions result;
    result.open = ImGui::CollapsingHeader(
        title, ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);

    if (help)
        HelpMarker(help);

    const float buttonWidth = ImGui::CalcTextSize("...").x +
        ImGui::GetStyle().FramePadding.x * 2.0f;
    const float right = ImGui::GetWindowContentRegionMax().x;
    ImGui::SameLine();
    ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), right - buttonWidth));
    ImGui::BeginDisabled(readOnly);
    if (ImGui::SmallButton("..."))
        ImGui::OpenPopup("ComponentActions");
    ImGui::EndDisabled();

    if (ImGui::BeginPopup("ComponentActions"))
    {
        ImGui::BeginDisabled(readOnly);
        result.reset = ImGui::MenuItem("Reset");
        if (removable)
            result.remove = ImGui::MenuItem("Remove Component");
        ImGui::EndDisabled();
        ImGui::EndPopup();
    }

    if (readOnly && result.open)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("Read only");
    }
    return result;
}

void EndComponentCard()
{
    ImGui::Spacing();
    ImGui::PopID();
}

bool BeginProperties(const char* id)
{
    constexpr ImGuiTableFlags flags =
        ImGuiTableFlags_SizingStretchProp |
        ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_PadOuterX;
    if (!ImGui::BeginTable(id, 2, flags))
        return false;
    ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 105.0f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
    return true;
}

void Property(const char* label, const char* help = nullptr)
{
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%s", label);
    if (help && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("%s", help);
    ImGui::TableSetColumnIndex(1);
    ImGui::SetNextItemWidth(-FLT_MIN);
}

void ValidationMessage(const std::string& message)
{
    if (message.empty())
        return;
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.53f, 0.30f, 1.0f));
    ImGui::TextWrapped("%s", message.c_str());
    ImGui::PopStyleColor();
}

std::string ValidateCamera(const CameraComponent& camera)
{
    if (!std::isfinite(camera.fov) || camera.fov < 1.0f || camera.fov > 179.0f)
        return "FOV must be between 1 and 179 degrees.";
    if (!std::isfinite(camera.nearClip) || camera.nearClip <= 0.0f)
        return "Near clip must be greater than zero.";
    if (!std::isfinite(camera.farClip) || camera.farClip <= camera.nearClip)
        return "Far clip must be greater than the near clip.";
    if (!std::isfinite(camera.aspect) || camera.aspect < 0.0f)
        return "Aspect override cannot be negative. Use 0 for viewport aspect.";
    return {};
}

std::string ValidateLight(const LightComponent& light)
{
    if (!std::isfinite(light.light.power) || light.light.power < 0.0f)
        return "Light intensity cannot be negative.";
    if (light.light.type != LightType::Directional &&
        (!std::isfinite(light.light.radius) || light.light.radius <= 0.0f))
        return "Point and spot lights require a positive range.";
    if (light.light.type == LightType::Spot)
    {
        if (light.light.innerAngle < 0.0f || light.light.outerAngle <= light.light.innerAngle ||
            light.light.outerAngle >= 180.0f)
            return "Spot outer cone must be greater than inner cone and below 180 degrees.";
    }
    return {};
}

std::string ValidatePhysics(const PhysicsComponent& physics)
{
    const auto& d = physics.collider.dimensions;
    if (d.x <= 0.0f || d.y <= 0.0f || d.z <= 0.0f)
        return "Collider dimensions must all be greater than zero.";
    if (physics.body.motion == MotionType::Dynamic &&
        (!std::isfinite(physics.body.mass) || physics.body.mass <= 0.0f))
        return "Dynamic bodies require a positive mass.";
    if (physics.collider.material.friction < 0.0f)
        return "Friction cannot be negative.";
    if (physics.collider.material.restitution < 0.0f ||
        physics.collider.material.restitution > 1.0f)
        return "Restitution must be between 0 and 1.";
    return {};
}

std::string ValidateAnimator(const AnimatorComponent& animator, unsigned clipCount)
{
    if (!std::isfinite(animator.state.speed))
        return "Animation speed must be finite.";
    if (clipCount && animator.state.clip >= clipCount)
        return "Selected clip index is outside the available animation range.";
    return {};
}

} // namespace

void EditorLayer::AppendComponentInspector(Engine& engine, EditorSession& session)
{
    if (!showInspector_)
        return;

    const auto entities = session.ViewDocument().Snapshot();
    const auto selected = std::find_if(entities.begin(), entities.end(),
        [](const AuthoringEntity& entity) { return entity.selected; });
    if (selected == entities.end())
        return;

    const bool editable = session.EditingEnabled();
    const bool hierarchyLocked = hierarchy_.Locked(selected->persistentID);
    const bool readOnly = !editable || hierarchyLocked;

    static std::uint64_t statusEntity = 0;
    static std::string status;
    if (statusEntity != selected->persistentID)
    {
        statusEntity = selected->persistentID;
        status.clear();
    }

    if (editable)
    {
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
    }
    else
    {
        componentPersistentID_ = selected->persistentID;
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

    ImGui::PushID("InspectorComponentCards");

    auto mutationFailed = [&](const std::exception& error)
    {
        status = error.what();
        LogComponentError(engine, error);
        componentRevision_ = 0;
    };
    auto mutationSucceeded = [&]()
    {
        status.clear();
        if (editable)
            componentRevision_ = session.EditDocument().Revision();
    };

    if (componentCamera_)
    {
        const auto card = BeginComponentCard(
            "Camera", "CameraCard", readOnly,
            "Controls the authored game camera. Camera-specific preview/frustum tools remain in the camera milestone.");
        if (card.reset && editable)
        {
            try
            {
                session.EditDocument().SetCamera(selected->entity, CameraComponent{});
                componentCamera_ = CameraComponent{};
                mutationSucceeded();
            }
            catch (const std::exception& error) { mutationFailed(error); }
        }
        if (card.remove && editable)
        {
            try
            {
                session.EditDocument().RemoveCamera(selected->entity);
                componentCamera_.reset();
                mutationSucceeded();
            }
            catch (const std::exception& error) { mutationFailed(error); }
        }
        if (card.open && componentCamera_)
        {
            ImGui::BeginDisabled(readOnly);
            bool commitRequested = false;
            if (BeginProperties("CameraProperties"))
            {
                Property("Target", "World-space point the camera looks at.");
                ImGui::DragFloat3("##Target", &componentCamera_->target.x, 0.05f);
                commitRequested |= ImGui::IsItemDeactivatedAfterEdit();

                Property("FOV", "Vertical field of view in degrees.");
                ImGui::DragFloat("##FOV", &componentCamera_->fov, 0.25f, 1.0f, 179.0f, "%.1f deg");
                commitRequested |= ImGui::IsItemDeactivatedAfterEdit();

                Property("Near Clip", "Nearest renderable camera distance.");
                ImGui::DragFloat("##Near", &componentCamera_->nearClip, 0.01f, 0.001f, FLT_MAX, "%.3f");
                commitRequested |= ImGui::IsItemDeactivatedAfterEdit();

                Property("Far Clip", "Farthest renderable camera distance.");
                ImGui::DragFloat("##Far", &componentCamera_->farClip, 1.0f, 0.01f, FLT_MAX, "%.2f");
                commitRequested |= ImGui::IsItemDeactivatedAfterEdit();

                Property("Aspect", "0 uses the active viewport aspect ratio.");
                ImGui::DragFloat("##Aspect", &componentCamera_->aspect, 0.01f, 0.0f, 10.0f, "%.3f");
                commitRequested |= ImGui::IsItemDeactivatedAfterEdit();

                ImGui::EndTable();
            }
            ImGui::EndDisabled();

            const std::string validation = ValidateCamera(*componentCamera_);
            ValidationMessage(validation);
            if (editable && !readOnly && commitRequested && validation.empty())
            {
                try
                {
                    session.EditDocument().SetCamera(selected->entity, *componentCamera_);
                    mutationSucceeded();
                }
                catch (const std::exception& error) { mutationFailed(error); }
            }

            if (selected->activeCamera)
                ImGui::TextDisabled("Active game camera");
        }
        EndComponentCard();
    }

    if (componentLight_)
    {
        const auto card = BeginComponentCard(
            "Light", "LightCard", readOnly,
            "Directional, point and spot light authoring with immediate Scene View feedback.");
        if (card.reset && editable)
        {
            try
            {
                LightComponent value{};
                session.EditDocument().SetLight(selected->entity, value);
                componentLight_ = value;
                mutationSucceeded();
            }
            catch (const std::exception& error) { mutationFailed(error); }
        }
        if (card.remove && editable)
        {
            try
            {
                session.EditDocument().RemoveLight(selected->entity);
                componentLight_.reset();
                mutationSucceeded();
            }
            catch (const std::exception& error) { mutationFailed(error); }
        }
        if (card.open && componentLight_)
        {
            auto commitLight = [&]()
            {
                const std::string validation = ValidateLight(*componentLight_);
                if (!editable || readOnly || !validation.empty())
                    return;
                try
                {
                    session.EditDocument().SetLight(selected->entity, *componentLight_);
                    mutationSucceeded();
                }
                catch (const std::exception& error) { mutationFailed(error); }
            };

            ImGui::BeginDisabled(readOnly);
            if (BeginProperties("LightProperties"))
            {
                Property("Type", "Choose Directional, Point or Spot behavior.");
                if (ImGui::BeginCombo("##LightType", LightName(componentLight_->light.type)))
                {
                    for (LightType type : {LightType::Directional, LightType::Point, LightType::Spot})
                    {
                        const bool current = componentLight_->light.type == type;
                        if (ImGui::Selectable(LightName(type), current))
                        {
                            componentLight_->light.type = type;
                            if (type == LightType::Spot &&
                                componentLight_->light.outerAngle <= componentLight_->light.innerAngle)
                                componentLight_->light.outerAngle =
                                    std::min(179.0f, componentLight_->light.innerAngle + 0.1f);
                            commitLight();
                        }
                        if (current) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                if (componentLight_->light.type != LightType::Point)
                {
                    Property("Direction", "World-space direction used by Directional and Spot lights.");
                    if (ImGui::DragFloat3("##Direction", &componentLight_->light.direction.x,
                                          0.01f, -1.0f, 1.0f, "%.3f"))
                        commitLight();
                }

                Property("Colour", "Linear light colour sent to the renderer.");
                if (ImGui::ColorEdit3("##Colour", &componentLight_->light.colour.x))
                    commitLight();

                Property("Intensity", "Renderer-neutral light power/intensity.");
                if (ImGui::DragFloat("##Intensity", &componentLight_->light.power,
                                     0.05f, 0.0f, FLT_MAX, "%.3f"))
                    commitLight();

                if (componentLight_->light.type != LightType::Directional)
                {
                    Property("Range", "Maximum influence distance for Point and Spot lights.");
                    if (ImGui::DragFloat("##Range", &componentLight_->light.radius,
                                         0.1f, 0.01f, 100000.0f, "%.2f"))
                        commitLight();
                }
                else
                {
                    Property("Range");
                    ImGui::TextDisabled("Infinite");
                }

                if (componentLight_->light.type == LightType::Spot)
                {
                    Property("Inner Cone", "Fully-lit inner cone angle.");
                    float inner = componentLight_->light.innerAngle;
                    if (ImGui::DragFloat("##InnerCone", &inner, 0.25f, 0.1f, 178.8f, "%.1f deg"))
                    {
                        componentLight_->light.innerAngle =
                            std::clamp(inner, 0.1f,
                                       std::max(0.1f, componentLight_->light.outerAngle - 0.1f));
                        commitLight();
                    }

                    Property("Outer Cone", "Outer falloff cone angle.");
                    float outer = componentLight_->light.outerAngle;
                    if (ImGui::DragFloat("##OuterCone", &outer, 0.25f, 0.2f, 179.0f, "%.1f deg"))
                    {
                        componentLight_->light.outerAngle =
                            std::clamp(outer, componentLight_->light.innerAngle + 0.1f, 179.0f);
                        commitLight();
                    }
                }

                Property("Shadows", "PlagC currently owns shadow-map execution as a renderer-wide pass.");
                auto renderSettings = engine.Graphics().Settings();
                bool sceneShadows = renderSettings.shadows;
                if (ImGui::Checkbox("##SceneShadows", &sceneShadows))
                {
                    renderSettings.shadows = sceneShadows;
                    engine.Graphics().SetSettings(renderSettings);
                }

                ImGui::EndTable();
            }
            ImGui::EndDisabled();

            ValidationMessage(ValidateLight(*componentLight_));
            ImGui::TextDisabled("Scene View light guides follow the viewport Gizmos toggle and never render during Play.");
        }
        EndComponentCard();
    }

    if (selected->record.model)
    {
        const auto card = BeginComponentCard(
            "Model", "ModelCard", readOnly,
            "Model asset assignment and mesh/material details are provided by the Model / Mesh Inspector milestone.");
        if (card.reset && editable)
        {
            try
            {
                ModelComponent value = *selected->record.model;
                value.visible = true;
                value.castShadows = true;
                session.EditDocument().SetModel(selected->entity, value);
                mutationSucceeded();
            }
            catch (const std::exception& error) { mutationFailed(error); }
        }
        if (card.remove && editable)
        {
            try
            {
                session.EditDocument().RemoveModel(selected->entity);
                componentAnimator_.reset();
                mutationSucceeded();
            }
            catch (const std::exception& error) { mutationFailed(error); }
        }
        if (card.open)
        {
            bool visible = selected->record.model->visible;
            bool castShadows = selected->record.model->castShadows;
            ImGui::BeginDisabled(readOnly);
            if (BeginProperties("ModelProperties"))
            {
                Property("Visible", "Controls whether the model is submitted to the renderer.");
                if (ImGui::Checkbox("##Visible", &visible) && editable)
                {
                    try { session.EditDocument().SetModelVisible(selected->entity, visible); mutationSucceeded(); }
                    catch (const std::exception& error) { mutationFailed(error); }
                }

                Property("Cast Shadows", "Controls renderer shadow casting for this model instance.");
                if (ImGui::Checkbox("##CastShadows", &castShadows) && editable)
                {
                    try { session.EditDocument().SetModelCastShadows(selected->entity, castShadows); mutationSucceeded(); }
                    catch (const std::exception& error) { mutationFailed(error); }
                }
                ImGui::EndTable();
            }
            ImGui::EndDisabled();
            if (!selected->record.model->asset)
                ValidationMessage("Model component has no resolved ModelAsset.");
        }
        EndComponentCard();
    }

    if (componentPhysics_)
    {
        const auto card = BeginComponentCard(
            "Physics", "PhysicsCard", readOnly,
            "Baseline body/collider properties. Milestone 10 owns the complete physics-authoring experience.");
        if (card.reset && editable)
        {
            try
            {
                PhysicsComponent value{};
                session.EditDocument().SetPhysics(selected->entity, value);
                componentPhysics_ = value;
                mutationSucceeded();
            }
            catch (const std::exception& error) { mutationFailed(error); }
        }
        if (card.remove && editable)
        {
            try
            {
                session.EditDocument().RemovePhysics(selected->entity);
                componentPhysics_.reset();
                mutationSucceeded();
            }
            catch (const std::exception& error) { mutationFailed(error); }
        }
        if (card.open && componentPhysics_)
        {
            ImGui::BeginDisabled(readOnly);
            bool commitRequested = false;
            if (BeginProperties("PhysicsProperties"))
            {
                Property("Motion", "Static, dynamic or kinematic body ownership.");
                if (ImGui::BeginCombo("##Motion", MotionName(componentPhysics_->body.motion)))
                {
                    for (MotionType motion : {MotionType::Static, MotionType::Dynamic, MotionType::Kinematic})
                    {
                        const bool current = componentPhysics_->body.motion == motion;
                        if (ImGui::Selectable(MotionName(motion), current))
                        {
                            componentPhysics_->body.motion = motion;
                            commitRequested = true;
                        }
                        if (current) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                Property("Mass", "Used by dynamic bodies.");
                ImGui::DragFloat("##Mass", &componentPhysics_->body.mass, 0.05f, 0.001f, FLT_MAX);
                commitRequested |= ImGui::IsItemDeactivatedAfterEdit();

                Property("Gravity", "Per-body gravity scale.");
                ImGui::DragFloat("##Gravity", &componentPhysics_->body.gravityScale, 0.01f, -10.0f, 10.0f);
                commitRequested |= ImGui::IsItemDeactivatedAfterEdit();

                Property("Collider", "Basic collider primitive.");
                if (ImGui::BeginCombo("##Collider", ColliderName(componentPhysics_->collider.type)))
                {
                    for (ColliderType type : {ColliderType::Box, ColliderType::Sphere,
                                              ColliderType::Capsule, ColliderType::Cylinder})
                    {
                        const bool current = componentPhysics_->collider.type == type;
                        if (ImGui::Selectable(ColliderName(type), current))
                        {
                            componentPhysics_->collider.type = type;
                            commitRequested = true;
                        }
                        if (current) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }

                Property("Size", "Collider dimensions; all axes must be positive.");
                ImGui::DragFloat3("##ColliderSize", &componentPhysics_->collider.dimensions.x,
                                  0.01f, 0.001f, FLT_MAX);
                commitRequested |= ImGui::IsItemDeactivatedAfterEdit();

                Property("Friction", "Surface friction coefficient.");
                ImGui::DragFloat("##Friction", &componentPhysics_->collider.material.friction,
                                 0.01f, 0.0f, FLT_MAX);
                commitRequested |= ImGui::IsItemDeactivatedAfterEdit();

                Property("Restitution", "Bounce coefficient from 0 to 1.");
                ImGui::DragFloat("##Restitution", &componentPhysics_->collider.material.restitution,
                                 0.01f, 0.0f, 1.0f);
                commitRequested |= ImGui::IsItemDeactivatedAfterEdit();
                ImGui::EndTable();
            }
            ImGui::EndDisabled();

            const std::string validation = ValidatePhysics(*componentPhysics_);
            ValidationMessage(validation);
            if (editable && !readOnly && validation.empty() && commitRequested)
            {
                try
                {
                    session.EditDocument().SetPhysics(selected->entity, *componentPhysics_);
                    mutationSucceeded();
                }
                catch (const std::exception& error) { mutationFailed(error); }
            }
        }
        EndComponentCard();
    }

    if (componentAnimator_)
    {
        const auto card = BeginComponentCard(
            "Animator", "AnimatorCard", readOnly,
            "Animation playback state. Milestone 11 owns controller/skeleton authoring depth.");
        if (card.reset && editable)
        {
            try
            {
                AnimatorComponent value = *componentAnimator_;
                value.state = {};
                session.EditDocument().SetAnimator(selected->entity, value);
                componentAnimator_ = value;
                mutationSucceeded();
            }
            catch (const std::exception& error) { mutationFailed(error); }
        }
        if (card.remove && editable)
        {
            try
            {
                session.EditDocument().RemoveAnimator(selected->entity);
                componentAnimator_.reset();
                mutationSucceeded();
            }
            catch (const std::exception& error) { mutationFailed(error); }
        }
        if (card.open && componentAnimator_)
        {
            unsigned clipCount = 0;
            if (componentAnimator_->animationSource)
                clipCount = static_cast<unsigned>(componentAnimator_->animationSource->animations.size());
            else if (selected->record.model && selected->record.model->asset)
                clipCount = static_cast<unsigned>(selected->record.model->asset->animations.size());

            ImGui::BeginDisabled(readOnly);
            bool commitRequested = false;
            if (BeginProperties("AnimatorProperties"))
            {
                int clip = static_cast<int>(componentAnimator_->state.clip);
                const int maxClip = clipCount ? static_cast<int>(clipCount - 1u) : 0;
                Property("Clip", "Index of the animation clip in the resolved source.");
                if (ImGui::DragInt("##Clip", &clip, 1.0f, 0, maxClip))
                {
                    componentAnimator_->state.clip = static_cast<unsigned>(std::clamp(clip, 0, maxClip));
                    commitRequested = true;
                }

                Property("Speed", "Animation playback-rate multiplier.");
                ImGui::DragFloat("##AnimSpeed", &componentAnimator_->state.speed, 0.01f, -10.0f, 10.0f);
                commitRequested |= ImGui::IsItemDeactivatedAfterEdit();

                Property("Playing", "Whether the animator advances this clip.");
                commitRequested |= ImGui::Checkbox("##Playing", &componentAnimator_->state.playing);

                Property("Loop", "Restart the clip when it reaches the end.");
                commitRequested |= ImGui::Checkbox("##Loop", &componentAnimator_->state.loop);
                ImGui::EndTable();
            }
            ImGui::EndDisabled();

            const std::string validation = ValidateAnimator(*componentAnimator_, clipCount);
            ValidationMessage(validation);
            if (editable && !readOnly && validation.empty() && commitRequested)
            {
                try
                {
                    session.EditDocument().SetAnimator(selected->entity, *componentAnimator_);
                    mutationSucceeded();
                }
                catch (const std::exception& error) { mutationFailed(error); }
            }
        }
        EndComponentCard();
    }

    if (editable)
    {
        ImGui::BeginDisabled(hierarchyLocked);
        if (ImGui::Button("+ Add Component", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f)))
            ImGui::OpenPopup("AddComponentPopup");
        ImGui::EndDisabled();

        if (ImGui::BeginPopup("AddComponentPopup"))
        {
            auto& document = session.EditDocument();

            ImGui::BeginDisabled(selected->record.camera.has_value());
            if (ImGui::MenuItem("Camera"))
            {
                try
                {
                    CameraComponent value{};
                    document.SetCamera(selected->entity, value);
                    componentCamera_ = value;
                    mutationSucceeded();
                }
                catch (const std::exception& error) { mutationFailed(error); }
            }
            ImGui::EndDisabled();

            ImGui::BeginDisabled(selected->record.light.has_value());
            if (ImGui::MenuItem("Light"))
            {
                try
                {
                    LightComponent value{};
                    document.SetLight(selected->entity, value);
                    componentLight_ = value;
                    mutationSucceeded();
                }
                catch (const std::exception& error) { mutationFailed(error); }
            }
            ImGui::EndDisabled();

            ImGui::BeginDisabled(selected->record.physics.has_value());
            if (ImGui::MenuItem("Physics"))
            {
                try
                {
                    PhysicsComponent value{};
                    document.SetPhysics(selected->entity, value);
                    componentPhysics_ = value;
                    mutationSucceeded();
                }
                catch (const std::exception& error) { mutationFailed(error); }
            }
            ImGui::EndDisabled();

            const bool canAddAnimator = !selected->record.animator.has_value() &&
                                        selected->record.model && selected->record.model->asset &&
                                        !selected->record.model->asset->animations.empty();
            ImGui::BeginDisabled(!canAddAnimator);
            if (ImGui::MenuItem("Animator"))
            {
                try
                {
                    AnimatorComponent value{};
                    document.SetAnimator(selected->entity, value);
                    componentAnimator_ = value;
                    mutationSucceeded();
                }
                catch (const std::exception& error) { mutationFailed(error); }
            }
            ImGui::EndDisabled();

            ImGui::BeginDisabled(true);
            ImGui::MenuItem("Model (assign asset in Model Inspector)");
            ImGui::EndDisabled();

            ImGui::EndPopup();
        }
    }
    else
    {
        ImGui::TextDisabled("Play mode: component authoring is read-only.");
    }

    if (hierarchyLocked)
        ValidationMessage("This entity is locked in the Hierarchy. Inspector mutations are disabled.");
    ValidationMessage(status);

    ImGui::PopID();
    ImGui::End();
}

} // namespace PlagC::Editor
