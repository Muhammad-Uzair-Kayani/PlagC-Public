#include "pgcpch.h"
#include "EditorVisualAssets.h"
#include <algorithm>
#include <cmath>
#include <string>

namespace PlagC::Editor {
namespace {

ImVec2 Add(ImVec2 a, ImVec2 b) { return {a.x + b.x, a.y + b.y}; }
ImVec2 Mul(ImVec2 a, float s) { return {a.x * s, a.y * s}; }

void Line(ImDrawList* draw, ImVec2 a, ImVec2 b, ImU32 colour, float width = 1.8f)
{
    draw->AddLine(a, b, colour, width);
}

void Box(ImDrawList* draw, ImVec2 center, float half, ImU32 colour, float rounding = 1.5f)
{
    draw->AddRect({center.x-half, center.y-half}, {center.x+half, center.y+half},
                  colour, rounding, 0, 1.6f);
}

} // namespace

ImFont* EditorVisualAssets::TextFont() noexcept
{
    // Dear ImGui creates its ProggyClean-derived default font from embedded data.
    // We intentionally use that baked font instead of shipping a raw font file.
    return ImGui::GetCurrentContext() ? ImGui::GetFont() : nullptr;
}

const char* EditorVisualAssets::AccessibleName(EditorIcon icon) noexcept
{
    switch (icon)
    {
    case EditorIcon::Entity: return "Entity";
    case EditorIcon::Folder: return "Folder";
    case EditorIcon::Asset: return "Asset";
    case EditorIcon::Model: return "Model";
    case EditorIcon::Texture: return "Texture";
    case EditorIcon::Material: return "Material";
    case EditorIcon::Camera: return "Camera";
    case EditorIcon::Light: return "Light";
    case EditorIcon::Physics: return "Physics";
    case EditorIcon::Save: return "Save";
    case EditorIcon::Undo: return "Undo";
    case EditorIcon::Redo: return "Redo";
    case EditorIcon::Translate: return "Translate";
    case EditorIcon::Rotate: return "Rotate";
    case EditorIcon::Scale: return "Scale";
    case EditorIcon::World: return "World";
    case EditorIcon::Local: return "Local";
    case EditorIcon::Pivot: return "Pivot";
    case EditorIcon::Center: return "Center";
    case EditorIcon::Snap: return "Snap";
    case EditorIcon::Play: return "Play";
    case EditorIcon::Pause: return "Pause";
    case EditorIcon::Resume: return "Resume";
    case EditorIcon::Step: return "Step";
    case EditorIcon::Stop: return "Stop";
    case EditorIcon::Settings: return "Settings";
    }
    return "Editor icon";
}

void EditorVisualAssets::DrawIcon(ImDrawList* draw,
                                  EditorIcon icon,
                                  ImVec2 c,
                                  float size,
                                  ImU32 colour)
{
    if (!draw || size <= 1.0f)
        return;

    const float r = size * 0.5f;
    const float q = size * 0.24f;
    const float w = std::max(1.2f, size * 0.10f);

    switch (icon)
    {
    case EditorIcon::Save:
        draw->AddRect({c.x-r*.72f,c.y-r*.72f},{c.x+r*.72f,c.y+r*.72f},colour,1.5f,0,w);
        draw->AddRect({c.x-r*.38f,c.y-r*.66f},{c.x+r*.30f,c.y-r*.18f},colour,1.0f,0,w);
        draw->AddRect({c.x-r*.38f,c.y+r*.10f},{c.x+r*.38f,c.y+r*.56f},colour,1.0f,0,w);
        break;
    case EditorIcon::Undo:
    case EditorIcon::Redo:
    {
        const float s = icon == EditorIcon::Undo ? -1.0f : 1.0f;
        draw->PathClear();
        draw->PathArcTo(c, r*.62f, icon == EditorIcon::Undo ? 3.8f : -0.65f,
                        icon == EditorIcon::Undo ? 6.9f : 2.45f, 18);
        draw->PathStroke(colour, 0, w);
        const ImVec2 tip{c.x+s*r*.72f,c.y-r*.28f};
        draw->AddTriangleFilled(tip,
            {tip.x-s*r*.34f,tip.y-r*.24f},
            {tip.x-s*r*.34f,tip.y+r*.24f},colour);
        break;
    }
    case EditorIcon::Translate:
        Line(draw,{c.x-r*.72f,c.y},{c.x+r*.72f,c.y},colour,w);
        Line(draw,{c.x,c.y-r*.72f},{c.x,c.y+r*.72f},colour,w);
        draw->AddTriangleFilled({c.x+r*.82f,c.y},{c.x+r*.45f,c.y-r*.20f},{c.x+r*.45f,c.y+r*.20f},colour);
        draw->AddTriangleFilled({c.x,c.y-r*.82f},{c.x-r*.20f,c.y-r*.45f},{c.x+r*.20f,c.y-r*.45f},colour);
        break;
    case EditorIcon::Rotate:
        draw->PathClear();
        draw->PathArcTo(c,r*.58f,-2.6f,2.0f,20);
        draw->PathStroke(colour,0,w);
        draw->AddTriangleFilled({c.x+r*.70f,c.y+r*.18f},{c.x+r*.30f,c.y+r*.08f},{c.x+r*.55f,c.y-r*.18f},colour);
        break;
    case EditorIcon::Scale:
        Line(draw,{c.x-r*.58f,c.y+r*.58f},{c.x+r*.58f,c.y-r*.58f},colour,w);
        Box(draw,{c.x-r*.58f,c.y+r*.58f},r*.20f,colour);
        Box(draw,{c.x+r*.58f,c.y-r*.58f},r*.20f,colour);
        break;
    case EditorIcon::Play:
    case EditorIcon::Resume:
        draw->AddTriangleFilled({c.x-r*.42f,c.y-r*.62f},{c.x-r*.42f,c.y+r*.62f},{c.x+r*.68f,c.y},colour);
        break;
    case EditorIcon::Pause:
        draw->AddRectFilled({c.x-r*.55f,c.y-r*.62f},{c.x-r*.15f,c.y+r*.62f},colour,1.0f);
        draw->AddRectFilled({c.x+r*.15f,c.y-r*.62f},{c.x+r*.55f,c.y+r*.62f},colour,1.0f);
        break;
    case EditorIcon::Step:
        draw->AddTriangleFilled({c.x-r*.62f,c.y-r*.58f},{c.x-r*.62f,c.y+r*.58f},{c.x+r*.26f,c.y},colour);
        draw->AddRectFilled({c.x+r*.34f,c.y-r*.58f},{c.x+r*.58f,c.y+r*.58f},colour,1.0f);
        break;
    case EditorIcon::Stop:
        draw->AddRectFilled({c.x-r*.54f,c.y-r*.54f},{c.x+r*.54f,c.y+r*.54f},colour,1.5f);
        break;
    case EditorIcon::Settings:
        draw->AddCircle(c,r*.28f,colour,16,w);
        for(int i=0;i<8;++i)
        {
            const float a=static_cast<float>(i)*3.1415926535f/4.0f;
            const ImVec2 p0{c.x+std::cos(a)*r*.45f,c.y+std::sin(a)*r*.45f};
            const ImVec2 p1{c.x+std::cos(a)*r*.78f,c.y+std::sin(a)*r*.78f};
            Line(draw,p0,p1,colour,w);
        }
        break;
    case EditorIcon::Folder:
        draw->AddRect({c.x-r*.72f,c.y-r*.25f},{c.x+r*.72f,c.y+r*.58f},colour,2.0f,0,w);
        draw->AddPolyline((ImVec2[3]){{c.x-r*.68f,c.y-r*.25f},{c.x-r*.38f,c.y-r*.58f},{c.x+r*.10f,c.y-r*.58f}},3,colour,0,w);
        break;
    case EditorIcon::Camera:
        draw->AddRect({c.x-r*.70f,c.y-r*.42f},{c.x+r*.38f,c.y+r*.42f},colour,2.0f,0,w);
        draw->AddTriangleFilled({c.x+r*.38f,c.y-r*.28f},{c.x+r*.76f,c.y-r*.50f},{c.x+r*.76f,c.y+r*.50f},colour);
        draw->AddCircle(c,r*.20f,colour,12,w);
        break;
    case EditorIcon::Light:
        draw->AddCircle({c.x,c.y-r*.12f},r*.36f,colour,16,w);
        Line(draw,{c.x-r*.23f,c.y+r*.28f},{c.x+r*.23f,c.y+r*.28f},colour,w);
        Line(draw,{c.x-r*.16f,c.y+r*.48f},{c.x+r*.16f,c.y+r*.48f},colour,w);
        for(int i=0;i<5;++i)
        {
            const float a=-2.45f+static_cast<float>(i)*1.22f;
            Line(draw,{c.x+std::cos(a)*r*.55f,c.y-r*.12f+std::sin(a)*r*.55f},
                      {c.x+std::cos(a)*r*.78f,c.y-r*.12f+std::sin(a)*r*.78f},colour,w);
        }
        break;
    case EditorIcon::Physics:
        draw->AddCircle(c,r*.62f,colour,18,w);
        Line(draw,{c.x-r*.62f,c.y},{c.x+r*.62f,c.y},colour,w);
        Line(draw,{c.x,c.y-r*.62f},{c.x,c.y+r*.62f},colour,w);
        draw->AddCircle(c,r*.16f,colour,12,w);
        break;
    case EditorIcon::Texture:
        Box(draw,c,r*.68f,colour);
        draw->AddCircle({c.x-r*.28f,c.y-r*.25f},r*.12f,colour,10,w);
        draw->AddPolyline((ImVec2[4]){{c.x-r*.55f,c.y+r*.42f},{c.x-r*.12f,c.y},{c.x+r*.12f,c.y+r*.22f},{c.x+r*.55f,c.y-r*.12f}},4,colour,0,w);
        break;
    case EditorIcon::Material:
        draw->AddCircle(c,r*.63f,colour,18,w);
        draw->AddCircle({c.x-r*.20f,c.y-r*.14f},r*.11f,colour,10,w);
        draw->AddCircle({c.x+r*.20f,c.y-r*.05f},r*.09f,colour,10,w);
        draw->AddCircle({c.x,c.y+r*.25f},r*.08f,colour,10,w);
        break;
    case EditorIcon::Model:
    case EditorIcon::Entity:
    case EditorIcon::Asset:
        Box(draw,c,r*.58f,colour);
        Line(draw,{c.x-r*.58f,c.y-r*.20f},{c.x,c.y+r*.16f},colour,w);
        Line(draw,{c.x+r*.58f,c.y-r*.20f},{c.x,c.y+r*.16f},colour,w);
        Line(draw,{c.x,c.y+r*.16f},{c.x,c.y+r*.58f},colour,w);
        break;
    case EditorIcon::World:
        draw->AddCircle(c,r*.62f,colour,18,w);
        draw->AddEllipse(c,{r*.28f,r*.62f},colour,0.0f,18,w);
        Line(draw,{c.x-r*.58f,c.y},{c.x+r*.58f,c.y},colour,w);
        break;
    case EditorIcon::Local:
        Line(draw,{c.x-r*.55f,c.y+r*.50f},{c.x+r*.58f,c.y+r*.50f},colour,w);
        Line(draw,{c.x-r*.55f,c.y+r*.50f},{c.x-r*.55f,c.y-r*.58f},colour,w);
        draw->AddCircle({c.x-r*.55f,c.y+r*.50f},r*.12f,colour,10,w);
        break;
    case EditorIcon::Pivot:
        draw->AddCircle(c,r*.55f,colour,16,w);
        draw->AddCircleFilled(c,r*.14f,colour);
        break;
    case EditorIcon::Center:
        draw->AddCircleFilled(c,r*.22f,colour);
        Line(draw,{c.x-r*.62f,c.y},{c.x+r*.62f,c.y},colour,w);
        Line(draw,{c.x,c.y-r*.62f},{c.x,c.y+r*.62f},colour,w);
        break;
    case EditorIcon::Snap:
        draw->PathClear();
        draw->PathLineTo({c.x-r*.55f,c.y-r*.48f});
        draw->PathLineTo({c.x-r*.55f,c.y+r*.28f});
        draw->PathArcTo({c.x,c.y+r*.28f},r*.55f,3.14159265f,0.0f,12);
        draw->PathLineTo({c.x+r*.55f,c.y-r*.48f});
        draw->PathStroke(colour,0,w);
        break;
    }
}

void EditorVisualAssets::DrawInlineIcon(EditorIcon icon, float size, ImU32 colour)
{
    const ImVec2 topLeft = ImGui::GetCursorScreenPos();
    ImGui::Dummy({size,size});
    DrawIcon(ImGui::GetWindowDrawList(), icon,
             {topLeft.x+size*0.5f,topLeft.y+size*0.5f}, size, colour);
}

bool EditorVisualAssets::IconButton(EditorIcon icon,
                                    std::string_view id,
                                    std::string_view tooltip,
                                    bool enabled,
                                    bool active,
                                    ImVec2 size)
{
    if (!ImGui::GetCurrentContext())
        return false;

    const std::string widget = "##IconButton." + std::string(id);
    ImGui::BeginDisabled(!enabled);
    if (active)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    }

    const bool pressed = ImGui::Button(widget.c_str(), size);
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    const ImVec2 center{(min.x+max.x)*0.5f,(min.y+max.y)*0.5f};
    const ImU32 colour = ImGui::GetColorU32(enabled ? ImGuiCol_Text : ImGuiCol_TextDisabled);
    DrawIcon(ImGui::GetWindowDrawList(), icon, center,
             std::min(max.x-min.x,max.y-min.y)*0.62f, colour);

    if (active)
        ImGui::PopStyleColor(2);
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        ImGui::SetTooltip("%.*s", static_cast<int>(tooltip.size()), tooltip.data());
    ImGui::EndDisabled();
    return pressed && enabled;
}

} // namespace PlagC::Editor
