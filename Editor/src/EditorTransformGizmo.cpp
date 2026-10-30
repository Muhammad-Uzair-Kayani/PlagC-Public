#include "pgcpch.h"
#include "EditorLayer.h"
#include <PlagC/Application.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace PlagC::Editor {
namespace {

struct V3 { float x=0, y=0, z=0; };

V3 Add(V3 a, V3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
V3 Sub(V3 a, V3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
V3 Mul(V3 a, float s) { return {a.x*s,a.y*s,a.z*s}; }
float Dot(V3 a, V3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
V3 Cross(V3 a, V3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
float Length(V3 a) { return std::sqrt(Dot(a,a)); }
V3 Normalize(V3 a)
{
    const float length=Length(a);
    return length>1e-6f ? Mul(a,1.0f/length) : V3{};
}
V3 ToV3(Vec3 v) { return {v.x,v.y,v.z}; }

struct ProjectedPoint { ImVec2 screen{}; float depth=0; bool visible=false; };

ProjectedPoint Project(V3 world, const CameraDesc& camera, const ImRect& rect)
{
    const V3 eye=ToV3(camera.position);
    V3 forward=Normalize(Sub(ToV3(camera.target),eye));
    if (Length(forward)<1e-5f) forward={0,0,-1};
    V3 right=Normalize(Cross(forward,{0,1,0}));
    if (Length(right)<1e-5f) right={1,0,0};
    const V3 up=Normalize(Cross(right,forward));
    const V3 rel=Sub(world,eye);
    const float depth=Dot(rel,forward);
    if (depth<=std::max(0.001f,camera.nearClip)) return {{},depth,false};

    constexpr float Pi=3.14159265358979323846f;
    const float f=1.0f/std::tan(std::clamp(camera.fov,1.0f,179.0f)*0.5f*Pi/180.0f);
    const float aspect=camera.aspect>0.0f ? camera.aspect : std::max(0.001f,rect.GetWidth()/rect.GetHeight());
    const float ndcX=(Dot(rel,right)/depth)*f/aspect;
    const float ndcY=(Dot(rel,up)/depth)*f;
    const ImVec2 screen{
        rect.Min.x+(ndcX*0.5f+0.5f)*rect.GetWidth(),
        rect.Min.y+(-ndcY*0.5f+0.5f)*rect.GetHeight()
    };
    return {screen,depth,ndcX>=-1.25f&&ndcX<=1.25f&&ndcY>=-1.25f&&ndcY<=1.25f};
}

float DistanceToSegment(ImVec2 p, ImVec2 a, ImVec2 b)
{
    const float vx=b.x-a.x, vy=b.y-a.y;
    const float wx=p.x-a.x, wy=p.y-a.y;
    const float len2=vx*vx+vy*vy;
    if (len2<=1e-6f) return std::sqrt(wx*wx+wy*wy);
    const float t=std::clamp((wx*vx+wy*vy)/len2,0.0f,1.0f);
    const float dx=p.x-(a.x+vx*t), dy=p.y-(a.y+vy*t);
    return std::sqrt(dx*dx+dy*dy);
}

float Snap(float value, float step)
{
    return step>0.0f ? std::round(value/step)*step : value;
}

float& AxisValue(Vec3& value, int axis)
{
    if (axis==0) return value.x;
    if (axis==1) return value.y;
    return value.z;
}

const char* ModeName(GizmoMode mode)
{
    switch (mode)
    {
    case GizmoMode::Translate: return "Translate [W]";
    case GizmoMode::Rotate: return "Rotate [E]";
    case GizmoMode::Scale: return "Scale [R]";
    }
    return "Gizmo";
}

} // namespace

void EditorLayer::FinishGizmoEdit(Engine& engine, bool commit)
{
    viewport_.input.gizmoActive=false;
    if (!gizmoEdit_)
        return;
    try
    {
        if (commit) gizmoEdit_->Commit();
        else gizmoEdit_->Cancel();
    }
    catch (const std::exception& error)
    {
        engine.Logger().Write(Severity::Error,"EDITOR",error.what());
    }
    gizmoEdit_.reset();
    gizmoPersistentID_=0;
    gizmoAxis_=-1;
    inspectorRevision_=0;
    componentRevision_=0;
}

void EditorLayer::AppendTransformGizmo(Engine& engine, EditorSession& session,
                                       const CameraDesc& editorCamera)
{
    viewport_.input.gizmoActive = gizmoEdit_ != nullptr;
    if (!session.EditingEnabled())
    {
        if (gizmoEdit_) FinishGizmoEdit(engine,false);
        return;
    }

    ImGuiWindow* window=ImGui::FindWindowByName("Viewport");
    if (!window || !window->WasActive || window->Collapsed)
    {
        if (gizmoEdit_) FinishGizmoEdit(engine,false);
        return;
    }
    const ImRect rect=window->WorkRect;
    if (rect.GetWidth()<8.0f || rect.GetHeight()<8.0f)
        return;

    const auto entities=session.ViewDocument().Snapshot();
    const auto selected=std::find_if(entities.begin(),entities.end(),
        [](const AuthoringEntity& e){ return e.selected; });
    if (selected==entities.end())
    {
        if (gizmoEdit_) FinishGizmoEdit(engine,false);
        return;
    }

    ImGuiIO& io=ImGui::GetIO();
    const bool viewportFocused=viewport_.input.viewportFocused;
    if (!gizmoEdit_ && !io.WantTextInput && viewportFocused && !io.KeyAlt && !io.KeyCtrl)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_W,false)) viewport_.gizmo=GizmoMode::Translate;
        if (ImGui::IsKeyPressed(ImGuiKey_E,false)) viewport_.gizmo=GizmoMode::Rotate;
        if (ImGui::IsKeyPressed(ImGuiKey_R,false)) viewport_.gizmo=GizmoMode::Scale;
    }

    const V3 origin=ToV3(selected->record.transform.position);
    const auto projectedOrigin=Project(origin,editorCamera,rect);
    if (!projectedOrigin.visible)
    {
        if (gizmoEdit_) FinishGizmoEdit(engine,false);
        return;
    }

    const V3 eye=ToV3(editorCamera.position);
    const float distance=std::max(0.1f,Length(Sub(origin,eye)));
    constexpr float Pi=3.14159265358979323846f;
    const float worldPerPixel=2.0f*distance*std::tan(std::clamp(editorCamera.fov,1.0f,179.0f)*0.5f*Pi/180.0f)
                            / std::max(1.0f,rect.GetHeight());
    const float axisWorldLength=std::clamp(worldPerPixel*82.0f,0.1f,1000.0f);
    constexpr std::array<V3,3> axes{{{1,0,0},{0,1,0},{0,0,1}}};
    constexpr std::array<ImU32,3> colors{{IM_COL32(230,75,75,255),IM_COL32(75,220,95,255),IM_COL32(80,135,245,255)}};
    std::array<ProjectedPoint,3> endpoints{};
    for (int axis=0;axis<3;++axis)
        endpoints[axis]=Project(Add(origin,Mul(axes[axis],axisWorldLength)),editorCamera,rect);

    const bool mouseInside=rect.Contains(io.MousePos);
    int hoveredAxis=-1;
    float bestDistance=std::numeric_limits<float>::max();
    // Do not use ImGuiIO::WantCaptureMouse here. The viewport itself is an ImGui window,
    // so global capture is expected while hovering it. Local viewport policy is authoritative.
    if (!gizmoEdit_ && mouseInside && viewport_.input.viewportHovered && !viewport_.input.uiBlocksMouse)
    {
        for (int axis=0;axis<3;++axis)
        {
            if (!endpoints[axis].visible) continue;
            const float distanceToAxis=DistanceToSegment(io.MousePos,projectedOrigin.screen,endpoints[axis].screen);
            if (distanceToAxis<9.0f && distanceToAxis<bestDistance)
            {
                bestDistance=distanceToAxis;
                hoveredAxis=axis;
            }
        }
    }

    viewport_.input.gizmoActive = gizmoEdit_ != nullptr || hoveredAxis>=0;

    if (!gizmoEdit_ && hoveredAxis>=0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        try
        {
            auto& document=session.EditDocument();
            auto edit=document.BeginTransformEdit(selected->entity);
            gizmoEdit_=std::make_unique<SceneDocument::TransformEdit>(std::move(edit));
            gizmoPersistentID_=selected->persistentID;
            gizmoAxis_=hoveredAxis;
            gizmoStart_=selected->record.transform;
            gizmoMouseStartX_=io.MousePos.x;
            gizmoMouseStartY_=io.MousePos.y;
            viewport_.input.gizmoActive=true;
        }
        catch (const std::exception& error)
        {
            engine.Logger().Write(Severity::Error,"EDITOR",error.what());
            FinishGizmoEdit(engine,false);
        }
    }

    if (gizmoEdit_)
    {
        if (!session.EditDocument().FindByPersistentID(gizmoPersistentID_))
        {
            FinishGizmoEdit(engine,false);
            return;
        }
        const int axis=gizmoAxis_;
        if (axis<0 || axis>=3 || !endpoints[axis].visible)
        {
            FinishGizmoEdit(engine,false);
            return;
        }
        const ImVec2 end=endpoints[axis].screen;
        const float sx=end.x-projectedOrigin.screen.x, sy=end.y-projectedOrigin.screen.y;
        const float screenLength=std::sqrt(sx*sx+sy*sy);
        if (screenLength>1.0f)
        {
            const float ux=sx/screenLength, uy=sy/screenLength;
            const float dx=io.MousePos.x-gizmoMouseStartX_, dy=io.MousePos.y-gizmoMouseStartY_;
            Transform preview=gizmoStart_;
            if (viewport_.gizmo==GizmoMode::Translate)
            {
                float delta=(dx*ux+dy*uy)/screenLength*axisWorldLength;
                if (viewport_.snapEnabled) delta=Snap(delta,viewport_.translationSnap);
                AxisValue(preview.position,axis)=AxisValue(gizmoStart_.position,axis)+delta;
            }
            else if (viewport_.gizmo==GizmoMode::Scale)
            {
                float delta=(dx*ux+dy*uy)/screenLength;
                if (viewport_.snapEnabled) delta=Snap(delta,viewport_.scaleSnap);
                AxisValue(preview.scale,axis)=std::max(0.001f,AxisValue(gizmoStart_.scale,axis)+delta);
            }
            else
            {
                const float tx=-uy, ty=ux;
                float degrees=(dx*tx+dy*ty)*0.5f;
                if (viewport_.snapEnabled) degrees=Snap(degrees,viewport_.rotationSnapDegrees);
                AxisValue(preview.rotationDegrees,axis)=AxisValue(gizmoStart_.rotationDegrees,axis)+degrees;
            }
            try
            {
                gizmoEdit_->Preview(preview);
            }
            catch (const std::exception& error)
            {
                engine.Logger().Write(Severity::Error,"EDITOR",error.what());
                FinishGizmoEdit(engine,false);
                return;
            }
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Escape,false))
            FinishGizmoEdit(engine,false);
        else if (ImGui::IsMouseReleased(ImGuiMouseButton_Left))
            FinishGizmoEdit(engine,true);
    }

    ImDrawList* draw=window->DrawList;
    draw->PushClipRect(rect.Min,rect.Max,true);
    for (int axis=0;axis<3;++axis)
    {
        if (!endpoints[axis].visible) continue;
        const bool active=(gizmoEdit_ && gizmoAxis_==axis) || (!gizmoEdit_ && hoveredAxis==axis);
        const ImU32 color=active ? IM_COL32(255,235,80,255) : colors[axis];
        draw->AddLine(projectedOrigin.screen,endpoints[axis].screen,color,active?4.0f:2.5f);
        const float radius=viewport_.gizmo==GizmoMode::Rotate ? 7.0f : 5.0f;
        if (viewport_.gizmo==GizmoMode::Scale)
            draw->AddRectFilled(ImVec2(endpoints[axis].screen.x-radius,endpoints[axis].screen.y-radius),
                                ImVec2(endpoints[axis].screen.x+radius,endpoints[axis].screen.y+radius),color);
        else
            draw->AddCircleFilled(endpoints[axis].screen,radius,color,16);
    }
    const ImVec2 labelPos{rect.Min.x+10.0f,rect.Min.y+10.0f};
    draw->AddText(labelPos,IM_COL32(235,235,235,230),ModeName(viewport_.gizmo));
    if (viewport_.snapEnabled)
        draw->AddText(ImVec2(labelPos.x,labelPos.y+18.0f),IM_COL32(190,210,255,220),"Snap ON");
    draw->PopClipRect();
}

} // namespace PlagC::Editor
