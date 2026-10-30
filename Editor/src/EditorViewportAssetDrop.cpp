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
Vec3 Add(Vec3 a, Vec3 b){ return {a.x+b.x,a.y+b.y,a.z+b.z}; }
Vec3 Sub(Vec3 a, Vec3 b){ return {a.x-b.x,a.y-b.y,a.z-b.z}; }
Vec3 Mul(Vec3 value,float scalar){ return {value.x*scalar,value.y*scalar,value.z*scalar}; }
float Dot(Vec3 a,Vec3 b){ return a.x*b.x+a.y*b.y+a.z*b.z; }
Vec3 Cross(Vec3 a,Vec3 b){ return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
float Length(Vec3 value){ return std::sqrt(Dot(value,value)); }
Vec3 Normalize(Vec3 value){ const float length=Length(value); return length>1e-6f?Mul(value,1.0f/length):Vec3{}; }

void DrawPlayStateOverlay(EditorSession& session, const ImRect& rect, ImDrawList& draw)
{
    if(!session.Playing()) return;

    const bool paused=session.Paused();
    const ImU32 accent=paused?IM_COL32(255,184,61,235):IM_COL32(72,220,110,235);
    const ImU32 background=IM_COL32(17,21,28,220);
    const char* label=paused
        ? "PAUSED  |  Authoring locked  |  F6 Resume  F7 Step  F5 Stop"
        : "PLAYING  |  Authoring locked  |  F6 Pause  F5 Stop";
    const ImVec2 textSize=ImGui::CalcTextSize(label);
    const ImVec2 badgeMin{rect.Min.x+10.0f,rect.Min.y+10.0f};
    const ImVec2 badgeMax{badgeMin.x+textSize.x+18.0f,badgeMin.y+textSize.y+10.0f};

    // This pass runs after EditorViewportPresenter::Draw, so runtime-state feedback stays
    // above the rendered scene instead of being covered by the viewport target image.
    draw.AddRect(rect.Min,rect.Max,accent,0.0f,0,2.0f);
    draw.AddRectFilled(badgeMin,badgeMax,background,4.0f);
    draw.AddRect(badgeMin,badgeMax,accent,4.0f,0,1.0f);
    draw.AddText(ImVec2(badgeMin.x+9.0f,badgeMin.y+5.0f),accent,label);
}

Vec3 DropPosition(const CameraDesc& camera,const ViewportState& viewport,const ImRect& rect,ImVec2 mouse)
{
    const float width=std::max(1.0f,rect.GetWidth()),height=std::max(1.0f,rect.GetHeight());
    const float u=std::clamp((mouse.x-rect.Min.x)/width,0.0f,1.0f);
    const float v=std::clamp((mouse.y-rect.Min.y)/height,0.0f,1.0f);
    const float ndcX=u*2.0f-1.0f,ndcY=1.0f-v*2.0f;

    Vec3 forward=Normalize(Sub(camera.target,camera.position));
    if(Length(forward)<1e-5f) forward={0,0,-1};
    Vec3 referenceUp{0,1,0};
    if(std::fabs(Dot(forward,referenceUp))>0.999f) referenceUp={0,0,1};
    Vec3 right=Normalize(Cross(forward,referenceUp));
    const Vec3 up=Normalize(Cross(right,forward));
    const float aspect=camera.aspect>0.0f?camera.aspect:width/height;

    Vec3 origin=camera.position;
    Vec3 direction=forward;
    if(viewport.orthographic)
    {
        const float halfH=std::max(0.25f,viewport.orthographicHeight*0.5f);
        origin=Add(origin,Mul(right,ndcX*halfH*aspect));
        origin=Add(origin,Mul(up,ndcY*halfH));
    }
    else
    {
        constexpr float pi=3.14159265358979323846f;
        const float tanHalfFov=std::tan(std::clamp(camera.fov,1.0f,179.0f)*0.5f*pi/180.0f);
        direction=Add(direction,Mul(right,ndcX*aspect*tanHalfFov));
        direction=Add(direction,Mul(up,ndcY*tanHalfFov));
        direction=Normalize(direction);
    }

    if(std::abs(direction.y)>1e-5f)
    {
        const float t=-origin.y/direction.y;
        if(t>std::max(0.01f,camera.nearClip)) return Add(origin,Mul(direction,t));
    }
    float fallbackDistance=Length(Sub(camera.target,camera.position));
    if(fallbackDistance<std::max(1.0f,camera.nearClip)) fallbackDistance=10.0f;
    return Add(origin,Mul(direction,fallbackDistance));
}

} // namespace

void EditorLayer::AppendViewportAssetDrop(Engine& engine,EditorSession& session,
                                          const CameraDesc& editorCamera)
{
    if(!ImGui::GetCurrentContext()) return;
    if(!ImGui::Begin("Viewport")){ ImGui::End(); return; }
    ImGuiWindow* window=ImGui::GetCurrentWindow();
    const ImRect rect=window->WorkRect;
    if(rect.GetWidth()<=1.0f||rect.GetHeight()<=1.0f){ ImGui::End(); return; }

    DrawPlayStateOverlay(session,rect,*window->DrawList);

    if(!session.EditingEnabled()){ ImGui::End(); return; }
    const ImGuiID surface=window->GetID("ViewportSurface");
    if(ImGui::BeginDragDropTargetCustom(rect,surface))
    {
        if(const ImGuiPayload* payload=ImGui::AcceptDragDropPayload(CookedModelPayload))
        {
            try
            {
                const auto position=DropPosition(editorCamera,viewport_,rect,ImGui::GetIO().MousePos);
                CreateCookedModelEntity(engine,session,static_cast<const char*>(payload->Data),position);
            }
            catch(const std::exception& error)
            {
                engine.Logger().Write(Severity::Error,"EDITOR-ASSETS",error.what());
            }
        }
        ImGui::EndDragDropTarget();
    }
    ImGui::End();
}

} // namespace PlagC::Editor
