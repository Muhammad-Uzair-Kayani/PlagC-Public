#include "pgcpch.h"
#include "EditorTransformInspector.h"
#include <PlagC/Application.h>
#include <PlagC/World/TransformGraph.h>
#include <imgui.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace PlagC::Editor {
namespace {

float& Axis(Vec3& value, int axis)
{
    if (axis == 0) return value.x;
    if (axis == 1) return value.y;
    return value.z;
}

const char* AxisName(int axis)
{
    static constexpr const char* names[] = {"X", "Y", "Z"};
    return names[axis];
}

ImVec4 AxisColour(int axis)
{
    static const ImVec4 colours[] = {
        {0.95f, 0.30f, 0.30f, 1.0f},
        {0.35f, 0.90f, 0.40f, 1.0f},
        {0.35f, 0.55f, 1.00f, 1.0f}
    };
    return colours[axis];
}

bool PhysicsOwnsTransform(const AuthoringEntity& selected)
{
    return selected.record.physics &&
           selected.record.physics->body.motion == MotionType::Dynamic;
}

void LogTransformError(Engine& engine, const std::exception& error)
{
    engine.Logger().Write(Severity::Error, "EDITOR-TRANSFORM", error.what());
}

} // namespace

void EditorTransformInspector::Reset() noexcept
{
    edit_.reset();
    editPersistentID_ = 0;
    persistentID_ = 0;
    revision_ = 0;
    value_ = {};
    worldSpace_ = false;
    uniformScale_ = false;
}

void EditorTransformInspector::Finish(Engine& engine, bool commit)
{
    if (!edit_)
        return;

    try
    {
        if (commit)
            edit_->Commit();
        else
            edit_->Cancel();
    }
    catch (const std::exception& error)
    {
        LogTransformError(engine, error);
    }

    edit_.reset();
    editPersistentID_ = 0;
    revision_ = 0;
}

void EditorTransformInspector::Sync(EditorSession& session, const AuthoringEntity& selected)
{
    if (!session.EditingEnabled() || edit_)
        return;

    auto& document = session.EditDocument();
    if (persistentID_ == selected.persistentID && revision_ == document.Revision())
        return;

    persistentID_ = selected.persistentID;
    revision_ = document.Revision();
    value_ = worldSpace_ ? selected.record.transform : document.LocalTransform(selected.entity);
}

void EditorTransformInspector::BeginGesture(Engine& engine,
                                             EditorSession& session,
                                             const AuthoringEntity& selected)
{
    if (edit_)
        return;

    try
    {
        auto edit = session.EditDocument().BeginTransformEdit(selected.entity);
        edit_ = std::make_unique<SceneDocument::TransformEdit>(std::move(edit));
        editPersistentID_ = selected.persistentID;
    }
    catch (const std::exception& error)
    {
        LogTransformError(engine, error);
        revision_ = 0;
    }
}

void EditorTransformInspector::Preview(Engine& engine,
                                        EditorSession& session,
                                        const AuthoringEntity& selected)
{
    if (!edit_ || editPersistentID_ != selected.persistentID)
        return;

    try
    {
        auto& document = session.EditDocument();
        Transform world = value_;
        if (!worldSpace_ && selected.parentPersistentID)
        {
            const Entity parent = document.FindByPersistentID(selected.parentPersistentID);
            if (!parent)
                throw std::logic_error("Transform Inspector parent no longer exists");
            world = ComposeTransform(document.Scene().ReadTransform(parent), value_);
        }

        edit_->Preview(world);
        revision_ = document.Revision();
    }
    catch (const std::exception& error)
    {
        LogTransformError(engine, error);
        Finish(engine, false);
    }
}

void EditorTransformInspector::ApplyImmediate(Engine& engine,
                                               EditorSession& session,
                                               const AuthoringEntity& selected,
                                               const Transform& next)
{
    try
    {
        auto& document = session.EditDocument();
        if (worldSpace_)
            document.SetTransform(selected.entity, next);
        else
            document.SetLocalTransform(selected.entity, next);

        value_ = next;
        revision_ = document.Revision();
    }
    catch (const std::exception& error)
    {
        LogTransformError(engine, error);
        revision_ = 0;
    }
}

void EditorTransformInspector::Draw(Engine& engine,
                                     EditorSession& session,
                                     const AuthoringEntity& selected,
                                     bool hierarchyLocked)
{
    Sync(session, selected);

    const bool physicsOwned = PhysicsOwnsTransform(selected);
    const bool readOnly = hierarchyLocked || physicsOwned;

    ImGui::SeparatorText("Transform");

    ImGui::BeginDisabled(readOnly || edit_ != nullptr);
    if (ImGui::RadioButton("Local", !worldSpace_))
    {
        worldSpace_ = false;
        revision_ = 0;
        Sync(session, selected);
    }
    ImGui::SameLine();
    if (ImGui::RadioButton("World", worldSpace_))
    {
        worldSpace_ = true;
        revision_ = 0;
        Sync(session, selected);
    }
    ImGui::SameLine();
    ImGui::Checkbox("Uniform Scale", &uniformScale_);
    ImGui::EndDisabled();

    if (selected.parentPersistentID)
    {
        if (worldSpace_)
            ImGui::TextDisabled("World space: resolved scene transform.");
        else
            ImGui::TextDisabled("Local space: relative to parent #%llu.",
                static_cast<unsigned long long>(selected.parentPersistentID));
    }
    else
    {
        ImGui::TextDisabled("Root entity: Local and World values are identical.");
    }

    if (hierarchyLocked)
        ImGui::TextDisabled("Transform editing is disabled because this entity is locked in Hierarchy.");
    else if (physicsOwned)
        ImGui::TextDisabled("Dynamic transform is physics-owned. Change the body motion type before authoring its transform.");

    auto applyReset = [&](Vec3 Transform::*member, Vec3 reset)
    {
        Transform next = value_;
        next.*member = reset;
        ApplyImmediate(engine, session, selected, next);
    };

    ImGui::BeginDisabled(readOnly);

    if (ImGui::Button("Reset Transform"))
        ApplyImmediate(engine, session, selected, Transform{});

    constexpr ImGuiTableFlags tableFlags =
        ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV;
    if (ImGui::BeginTable("TransformInspectorAxes", 5, tableFlags))
    {
        ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 64.0f);
        ImGui::TableSetupColumn("X");
        ImGui::TableSetupColumn("Y");
        ImGui::TableSetupColumn("Z");
        ImGui::TableSetupColumn("Reset", ImGuiTableColumnFlags_WidthFixed, 48.0f);

        auto drawRow = [&](const char* label,
                           Vec3 Transform::*member,
                           float speed,
                           bool scale)
        {
            ImGui::PushID(label);
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(label);

            Vec3& vector = value_.*member;
            for (int axis = 0; axis < 3; ++axis)
            {
                ImGui::TableSetColumnIndex(axis + 1);
                ImGui::PushID(axis);
                ImGui::TextColored(AxisColour(axis), "%s", AxisName(axis));
                ImGui::SameLine(0.0f, 4.0f);
                ImGui::SetNextItemWidth(-FLT_MIN);

                float& component = Axis(vector, axis);
                const float beforeAxis = component;
                const bool changed = scale
                    ? ImGui::DragFloat("##Value", &component, speed, 0.001f, FLT_MAX, "%.3f")
                    : ImGui::DragFloat("##Value", &component, speed, 0.0f, 0.0f, "%.3f");

                if (ImGui::IsItemActivated())
                    BeginGesture(engine, session, selected);

                if (changed)
                {
                    if (scale)
                    {
                        component = std::max(component, 0.001f);
                        if (uniformScale_ && beforeAxis > 1e-6f)
                        {
                            const float ratio = component / beforeAxis;
                            for (int other = 0; other < 3; ++other)
                            {
                                if (other == axis)
                                    continue;
                                Axis(vector, other) = std::max(0.001f, Axis(vector, other) * ratio);
                            }
                        }
                    }
                    if (!edit_)
                        BeginGesture(engine, session, selected);
                    Preview(engine, session, selected);
                }

                if (ImGui::IsItemDeactivated() && edit_)
                    Finish(engine, true);
                ImGui::PopID();
            }

            ImGui::TableSetColumnIndex(4);
            if (ImGui::SmallButton("Reset"))
                applyReset(member, scale ? Vec3{1.0f, 1.0f, 1.0f} : Vec3{});
            ImGui::PopID();
        };

        drawRow("Position", &Transform::position, 0.05f, false);
        drawRow("Rotation", &Transform::rotationDegrees, 0.25f, false);
        drawRow("Scale", &Transform::scale, 0.01f, true);
        ImGui::EndTable();
    }

    ImGui::EndDisabled();
}

} // namespace PlagC::Editor
