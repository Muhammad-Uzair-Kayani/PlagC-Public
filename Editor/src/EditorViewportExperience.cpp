#include "pgcpch.h"
#include "EditorLayer.h"
#include <PlagC/Application.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace PlagC::Editor {
namespace {

constexpr float Pi = 3.14159265358979323846f;
struct V3 { float x=0, y=0, z=0; };
V3 V(Vec3 a){ return {a.x,a.y,a.z}; }
Vec3 P(V3 a){ return {a.x,a.y,a.z}; }
V3 Add(V3 a,V3 b){ return {a.x+b.x,a.y+b.y,a.z+b.z}; }
V3 Sub(V3 a,V3 b){ return {a.x-b.x,a.y-b.y,a.z-b.z}; }
V3 Mul(V3 a,float s){ return {a.x*s,a.y*s,a.z*s}; }
float Dot(V3 a,V3 b){ return a.x*b.x+a.y*b.y+a.z*b.z; }
V3 Cross(V3 a,V3 b){ return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
float Length(V3 a){ return std::sqrt(Dot(a,a)); }
V3 Normalize(V3 a){ const float n=Length(a); return n>1e-6f?Mul(a,1.0f/n):V3{}; }

struct Basis { V3 forward,right,up; };
Basis CameraBasis(const CameraDesc& camera)
{
    V3 forward=Normalize(Sub(V(camera.target),V(camera.position)));
    if(Length(forward)<1e-5f) forward={0,0,-1};
    V3 reference={0,1,0};
    if(std::fabs(Dot(forward,reference))>0.999f) reference={0,0,1};
    V3 right=Normalize(Cross(forward,reference));
    V3 up=Normalize(Cross(right,forward));
    return {forward,right,up};
}

struct Projected { ImVec2 point{}; float depth=0; bool valid=false; };
Projected Project(V3 world,const CameraDesc& camera,const ViewportState& state,const ImRect& rect)
{
    const auto basis=CameraBasis(camera);
    const V3 rel=Sub(world,V(camera.position));
    const float depth=Dot(rel,basis.forward);
    if(depth<=std::max(0.001f,camera.nearClip)) return {{},depth,false};
    const float aspect=camera.aspect>0?camera.aspect:std::max(0.001f,rect.GetWidth()/rect.GetHeight());
    float ndcX=0,ndcY=0;
    if(state.orthographic)
    {
        const float halfH=std::max(0.001f,state.orthographicHeight*0.5f);
        ndcX=Dot(rel,basis.right)/(halfH*aspect);
        ndcY=Dot(rel,basis.up)/halfH;
    }
    else
    {
        const float f=1.0f/std::tan(std::clamp(camera.fov,1.0f,179.0f)*0.5f*Pi/180.0f);
        ndcX=(Dot(rel,basis.right)/depth)*f/aspect;
        ndcY=(Dot(rel,basis.up)/depth)*f;
    }
    return {{rect.Min.x+(ndcX*0.5f+0.5f)*rect.GetWidth(),rect.Min.y+(-ndcY*0.5f+0.5f)*rect.GetHeight()},depth,true};
}

const AuthoringEntity* Selected(const std::vector<AuthoringEntity>& entities)
{
    const auto it=std::find_if(entities.begin(),entities.end(),[](const AuthoringEntity& e){return e.selected;});
    return it==entities.end()?nullptr:&*it;
}

float AdaptiveGridSpacing(const CameraDesc& camera)
{
    const float distance=std::max(0.5f,Length(Sub(V(camera.position),V(camera.target))));
    const float decade=std::pow(10.0f,std::floor(std::log10(distance)));
    const float normalized=distance/decade;
    if(normalized<2.0f) return decade*0.1f;
    if(normalized<5.0f) return decade*0.25f;
    return decade*0.5f;
}

void DrawGrid(ImDrawList* draw,const ImRect& rect,const CameraDesc& camera,const ViewportState& state)
{
    if(!state.gridVisible) return;
    const float spacing=AdaptiveGridSpacing(camera),extent=spacing*40.0f;
    const float cx=std::round(camera.target.x/spacing)*spacing,cz=std::round(camera.target.z/spacing)*spacing;
    draw->PushClipRect(rect.Min,rect.Max,true);
    for(int i=-40;i<=40;++i)
    {
        const float x=cx+i*spacing;
        auto a=Project({x,0,cz-extent},camera,state,rect),b=Project({x,0,cz+extent},camera,state,rect);
        if(a.valid&&b.valid){const bool major=(i%5)==0;draw->AddLine(a.point,b.point,major?IM_COL32(115,120,130,78):IM_COL32(95,100,110,38),major?1.1f:1.0f);}
        const float z=cz+i*spacing;
        a=Project({cx-extent,0,z},camera,state,rect);b=Project({cx+extent,0,z},camera,state,rect);
        if(a.valid&&b.valid){const bool major=(i%5)==0;draw->AddLine(a.point,b.point,major?IM_COL32(115,120,130,78):IM_COL32(95,100,110,38),major?1.1f:1.0f);}
    }
    auto x0=Project({cx-extent,0,0},camera,state,rect),x1=Project({cx+extent,0,0},camera,state,rect);
    if(x0.valid&&x1.valid) draw->AddLine(x0.point,x1.point,IM_COL32(210,70,70,160),1.8f);
    auto z0=Project({0,0,cz-extent},camera,state,rect),z1=Project({0,0,cz+extent},camera,state,rect);
    if(z0.valid&&z1.valid) draw->AddLine(z0.point,z1.point,IM_COL32(70,120,225,160),1.8f);
    draw->PopClipRect();
}

void DrawOrientation(ImDrawList* draw,const ImRect& rect,const CameraDesc& camera)
{
    const auto basis=CameraBasis(camera);
    const ImVec2 center{rect.Max.x-48.0f,rect.Min.y+48.0f};
    constexpr std::array<V3,3> axes{{{1,0,0},{0,1,0},{0,0,1}}};
    constexpr std::array<ImU32,3> colors{{IM_COL32(235,75,75,255),IM_COL32(80,220,105,255),IM_COL32(80,140,245,255)}};
    constexpr std::array<const char*,3> names{{"X","Y","Z"}};
    draw->AddCircleFilled(center,27.0f,IM_COL32(18,20,24,180),32);
    for(int i=0;i<3;++i)
    {
        const float sx=Dot(axes[i],basis.right),sy=-Dot(axes[i],basis.up);
        const ImVec2 end{center.x+sx*21.0f,center.y+sy*21.0f};
        draw->AddLine(center,end,colors[i],2.2f);
        draw->AddText(ImVec2(end.x-4,end.y-7),colors[i],names[i]);
    }
}

void DrawSelectionFeedback(ImDrawList* draw,const ImRect& rect,const CameraDesc& camera,
                           const ViewportState& state,const AuthoringEntity* selected)
{
    if(!state.selectionFeedback||!selected) return;
    const auto p=Project(V(selected->record.transform.position),camera,state,rect);
    if(!p.valid||!rect.Contains(p.point)) return;
    draw->AddCircle(p.point,13.0f,IM_COL32(255,196,70,245),32,2.0f);
    draw->AddCircle(p.point,17.0f,IM_COL32(255,196,70,100),32,1.0f);
    draw->AddText(ImVec2(p.point.x+19,p.point.y-8),IM_COL32(255,225,165,235),selected->record.name.c_str());
}

bool FocusSelection(EditorSession& session,CameraDesc& camera)
{
    const auto entities=session.ViewDocument().Snapshot();
    const auto* selected=Selected(entities);
    if(!selected) return false;
    const auto basis=CameraBasis(camera);
    const float distance=std::clamp(Length(Sub(V(camera.position),V(camera.target))),4.0f,250.0f);
    camera.target=selected->record.transform.position;
    camera.position=P(Sub(V(camera.target),Mul(basis.forward,distance)));
    return true;
}

bool FrameAll(EditorSession& session,CameraDesc& camera,ViewportState& state)
{
    const auto entities=session.ViewDocument().Snapshot();
    if(entities.empty()) return false;
    V3 lo=V(entities.front().record.transform.position),hi=lo;
    for(const auto& e:entities)
    {
        const V3 p=V(e.record.transform.position);
        lo.x=std::min(lo.x,p.x);lo.y=std::min(lo.y,p.y);lo.z=std::min(lo.z,p.z);
        hi.x=std::max(hi.x,p.x);hi.y=std::max(hi.y,p.y);hi.z=std::max(hi.z,p.z);
    }
    const V3 center=Mul(Add(lo,hi),0.5f);
    const float radius=std::max(2.0f,Length(Sub(hi,lo))*0.5f);
    const auto basis=CameraBasis(camera);
    float distance=radius*2.6f;
    if(!state.orthographic) distance=std::max(distance,radius/std::tan(std::clamp(camera.fov,10.0f,150.0f)*0.5f*Pi/180.0f)*1.2f);
    else state.orthographicHeight=std::max(4.0f,radius*2.4f);
    camera.target=P(center);camera.position=P(Sub(center,Mul(basis.forward,distance)));
    return true;
}

void ApplyViewPreset(const char* name,EditorSession& session,CameraDesc& camera)
{
    const auto entities=session.ViewDocument().Snapshot();
    V3 target=V(camera.target);
    if(const auto* selected=Selected(entities)) target=V(selected->record.transform.position);
    const float distance=std::max(6.0f,Length(Sub(V(camera.position),V(camera.target))));
    V3 direction{0,0,1};
    if(std::strcmp(name,"Back")==0) direction={0,0,-1};
    else if(std::strcmp(name,"Left")==0) direction={-1,0,0};
    else if(std::strcmp(name,"Right")==0) direction={1,0,0};
    else if(std::strcmp(name,"Top")==0) direction={0,1,0.001f};
    else if(std::strcmp(name,"Bottom")==0) direction={0,-1,0.001f};
    direction=Normalize(direction);camera.target=P(target);camera.position=P(Add(target,Mul(direction,distance)));
}

bool ToggleButton(const char* label,bool active)
{
    if(active) ImGui::PushStyleColor(ImGuiCol_Button,ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    const bool clicked=ImGui::Button(label);
    if(active) ImGui::PopStyleColor();
    return clicked;
}

} // namespace

void EditorLayer::AppendViewportExperience(Engine& engine,EditorSession& session,CameraDesc& editorCamera)
{
    AppendCameraInspector(engine,session,editorCamera);

    ImGuiWindow* viewportWindow=ImGui::FindWindowByName("Viewport");
    if(!viewportWindow||!viewportWindow->WasActive||viewportWindow->Collapsed) return;
    const ImRect rect=viewportWindow->WorkRect;
    if(rect.GetWidth()<40||rect.GetHeight()<40) return;

    const auto entities=session.ViewDocument().Snapshot();
    const auto* selected=Selected(entities);
    ImDrawList* draw=viewportWindow->DrawList;
    DrawGrid(draw,rect,editorCamera,viewport_);
    DrawSelectionFeedback(draw,rect,editorCamera,viewport_,selected);
    DrawOrientation(draw,rect,editorCamera);

    bool cameraChanged=false;
    if(!session.Playing()&&viewport_.input.viewportFocused&&!ImGui::GetIO().WantTextInput&&
       !ImGui::GetIO().KeyCtrl&&!ImGui::GetIO().KeyAlt&&ImGui::IsKeyPressed(ImGuiKey_F,false))
        cameraChanged=FocusSelection(session,editorCamera);

    ImGui::SetNextWindowPos(ImVec2(rect.Min.x+10.0f,rect.Min.y+9.0f),ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.82f);
    constexpr ImGuiWindowFlags flags=ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_NoDocking|
        ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoNav;
    if(ImGui::Begin("##PlagCViewportToolbar",nullptr,flags))
    {
        if(ToggleButton("Translate",viewport_.gizmo==GizmoMode::Translate))viewport_.gizmo=GizmoMode::Translate;
        ImGui::SameLine();if(ToggleButton("Rotate",viewport_.gizmo==GizmoMode::Rotate))viewport_.gizmo=GizmoMode::Rotate;
        ImGui::SameLine();if(ToggleButton("Scale",viewport_.gizmo==GizmoMode::Scale))viewport_.gizmo=GizmoMode::Scale;
        ImGui::SameLine();if(ToggleButton(viewport_.transformSpace==TransformSpace::World?"World":"Local",true))
            viewport_.transformSpace=viewport_.transformSpace==TransformSpace::World?TransformSpace::Local:TransformSpace::World;
        ImGui::SameLine();if(ToggleButton("Snap",viewport_.snapEnabled))viewport_.snapEnabled=!viewport_.snapEnabled;
        ImGui::SameLine();if(ToggleButton("Grid",viewport_.gridVisible))viewport_.gridVisible=!viewport_.gridVisible;
        ImGui::SameLine();if(ToggleButton("Gizmos",viewport_.gizmosVisible))viewport_.gizmosVisible=!viewport_.gizmosVisible;

        auto settings=engine.Graphics().Settings();
        ImGui::SameLine();if(ToggleButton("Wire",settings.wireframe)){settings.wireframe=!settings.wireframe;engine.Graphics().SetSettings(settings);}
        ImGui::SameLine();if(ToggleButton("Preview Light",viewport_.previewLighting))viewport_.previewLighting=!viewport_.previewLighting;
        ImGui::SameLine();if(ImGui::Button("Focus"))cameraChanged|=FocusSelection(session,editorCamera);
        ImGui::SameLine();if(ImGui::Button("Frame All"))cameraChanged|=FrameAll(session,editorCamera,viewport_);

        ImGui::SameLine();
        const char* projection=viewport_.orthographic?"Orthographic":"Perspective";
        if(ImGui::BeginCombo("##Projection",projection))
        {
            if(ImGui::Selectable("Perspective",!viewport_.orthographic)){viewport_.orthographic=false;cameraChanged=true;}
            if(ImGui::Selectable("Orthographic",viewport_.orthographic)){viewport_.orthographic=true;cameraChanged=true;}
            ImGui::Separator();
            constexpr std::array<const char*,6> presets{{"Front","Back","Left","Right","Top","Bottom"}};
            for(const char* preset:presets)
                if(ImGui::Selectable(preset)){ApplyViewPreset(preset,session,editorCamera);cameraChanged=true;}
            ImGui::EndCombo();
        }

        ImGui::SetNextItemWidth(85.0f);
        if(ImGui::DragFloat("Speed",&viewport_.cameraSpeed,0.1f,0.1f,200.0f,"%.1f"))
            viewport_.cameraSpeed=std::clamp(viewport_.cameraSpeed,0.1f,200.0f);
        ImGui::SameLine();settings=engine.Graphics().Settings();ImGui::SetNextItemWidth(85.0f);
        if(ImGui::DragFloat("Exposure",&settings.exposure,0.02f,0.1f,8.0f,"%.2f"))
        {settings.exposure=std::clamp(settings.exposure,0.1f,8.0f);engine.Graphics().SetSettings(settings);}
        if(viewport_.orthographic)
        {
            ImGui::SameLine();ImGui::SetNextItemWidth(85.0f);
            if(ImGui::DragFloat("Ortho Size",&viewport_.orthographicHeight,0.1f,0.5f,1000.0f,"%.1f"))
            {viewport_.orthographicHeight=std::clamp(viewport_.orthographicHeight,0.5f,1000.0f);cameraChanged=true;}
        }
        if(viewport_.snapEnabled)
        {
            ImGui::SetNextItemWidth(70);ImGui::DragFloat("Move",&viewport_.translationSnap,0.05f,0.01f,100.0f);
            ImGui::SameLine();ImGui::SetNextItemWidth(70);ImGui::DragFloat("Angle",&viewport_.rotationSnapDegrees,1.0f,1.0f,180.0f);
            ImGui::SameLine();ImGui::SetNextItemWidth(70);ImGui::DragFloat("Scale Snap",&viewport_.scaleSnap,0.01f,0.01f,10.0f);
        }

        const bool popup=ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel);
        viewport_.input.uiBlocksMouse=ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup)||popup;
        viewport_.input.uiBlocksKeyboard=ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)||popup;
    }
    ImGui::End();

    engine.Graphics().SetCameraProjection(viewport_.orthographic?CameraProjection::Orthographic:CameraProjection::Perspective,
                                          viewport_.orthographicHeight);
    if(cameraChanged)engine.Graphics().SetCamera(editorCamera);
}

} // namespace PlagC::Editor