#include "pgcpch.h"
#include "EditorLayer.h"
#include <PlagC/Application.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>

namespace PlagC::Editor {
namespace {

constexpr const char* CookedModelPayload = "PLAGC_COOKED_MODEL";

Vec3 Add(Vec3 a, Vec3 b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vec3 Sub(Vec3 a, Vec3 b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3 Mul(Vec3 value, float scalar)
{
    return {value.x * scalar, value.y * scalar, value.z * scalar};
}

float Dot(Vec3 a, Vec3 b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 Cross(Vec3 a, Vec3 b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

float Length(Vec3 value)
{
    return std::sqrt(Dot(value, value));
}

Vec3 Normalize(Vec3 value)
{
    const float length = Length(value);
    return length > 1e-6f ? Mul(value, 1.0f / length) : Vec3{};
}

Vec3 DropPosition(const CameraDesc& camera, const ImRect& rect, ImVec2 mouse)
{
    const float width = std::max(1.0f, rect.GetWidth());
    const float height = std::max(1.0f, rect.GetHeight());
    const float u = std::clamp((mouse.x - rect.Min.x) / width, 0.0f, 1.0f);
    const float v = std::clamp((mouse.y - rect.Min.y) / height, 0.0f, 1.0f);
    const float ndcX = u * 2.0f - 1.0f;
    const float ndcY = 1.0f - v * 2.0f;

    Vec3 forward = Normalize(Sub(camera.target, camera.position));
    if (Length(forward) < 1e-5f)
        forward = {0.0f, 0.0f, -1.0f};

    Vec3 right = Normalize(Cross(forward, {0.0f, 1.0f, 0.0f}));
    if (Length(right) < 1e-5f)
        right = {1.0f, 0.0f, 0.0f};
    const Vec3 up = Normalize(Cross(right, forward));

    constexpr float pi = 3.14159265358979323846f;
    const float tanHalfFov = std::tan(
        std::clamp(camera.fov, 1.0f, 179.0f) * 0.5f * pi / 180.0f);
    const float aspect = camera.aspect > 0.0f ? camera.aspect : width / height;

    Vec3 direction = forward;
    direction = Add(direction, Mul(right, ndcX * aspect * tanHalfFov));
    direction = Add(direction, Mul(up, ndcY * tanHalfFov));
    direction = Normalize(direction);

    // Authoring placement uses the conventional world ground plane (Y=0).
    // If the cursor ray points away from that plane, preserve the mouse direction
    // and place the model at roughly the camera target distance instead.
    if (std::abs(direction.y) > 1e-5f)
    {
        const float t = -camera.position.y / direction.y;
        if (t > std::max(0.01f, camera.nearClip))
            return Add(camera.position, Mul(direction, t));
    }

    float fallbackDistance = Length(Sub(camera.target, camera.position));
    if (fallbackDistance < std::max(1.0f, camera.nearClip))
        fallbackDistance = 10.0f;
    return Add(camera.position, Mul(direction, fallbackDistance));
}

} // namespace

void EditorLayer::AppendViewportAssetDrop(Engine& engine, EditorSession& session,
                                          const CameraDesc& editorCamera)
{
    if (!session.EditingEnabled() || !ImGui::GetCurrentContext())
        return;

    ImGuiWindow* window = ImGui::FindWindowByName("Viewport");
    if (!window || !window->WasActive || window->Collapsed)
        return;

    const ImRect rect = window->WorkRect;
    if (rect.GetWidth() <= 1.0f || rect.GetHeight() <= 1.0f)
        return;

    // DrawViewport() created this exact item ID. Use a custom target so the drop
    // maps to the real rendered surface even though other editor windows were drawn
    // between the InvisibleButton and this late viewport presentation pass.
    const ImGuiID surface = window->GetID("ViewportSurface");
    if (!ImGui::BeginDragDropTargetCustom(rect, surface))
        return;

    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(CookedModelPayload))
    {
        try
        {
            const auto position = DropPosition(editorCamera, rect, ImGui::GetIO().MousePos);
            CreateCookedModelEntity(engine, session,
                                    static_cast<const char*>(payload->Data), position);
        }
        catch (const std::exception& error)
        {
            engine.Logger().Write(Severity::Error, "EDITOR-ASSETS", error.what());
        }
    }

    ImGui::EndDragDropTarget();
}

} // namespace PlagC::Editor
