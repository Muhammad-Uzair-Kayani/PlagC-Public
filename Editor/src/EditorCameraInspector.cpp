#include "pgcpch.h"
#include "EditorLayer.h"
#include <PlagC/Application.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>

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

struct Projected { ImVec2 point{}; bool valid=false; };
Projected Project(V3 world,const CameraDesc& camera,const ViewportState& state,const ImRect& rect)
{
    const auto basis=CameraBasis(camera);
    const V3 rel=Sub(world,V(camera.position));
    const float depth=Dot(rel,basis.forward);
    if(depth<=std::max(0.001f,camera.nearClip)) return {};
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
    return {{rect.Min.x+(ndcX*0.5f+0.5f)*rect.GetWidth(),
             rect.Min.y+(-ndcY*0.5f+0.5f)*rect.GetHeight()},true};
}

const AuthoringEntity* SelectedCamera(const std::vector<AuthoringEntity>& entities)
{
    const auto it=std::find_if(entities.begin(),entities.end(),[](const AuthoringEntity& e)
    {
        return e.selected && e.record.camera.has_value();
    });
    return it==entities.end()?nullptr:&*it;
}

CameraDesc AuthoredCamera(const AuthoringEntity& entity)
{
    const auto& component=*entity.record.camera;
    CameraDesc camera;
    camera.position=entity.record.transform.position;
    camera.target=component.target;
    camera.fov=component.fov;
    camera.nearClip=component.nearClip;
    camera.farClip=component.farClip;
    camera.aspect=component.aspect;
    return camera;
}

void DrawFrustum(ImDrawList* draw,const ImRect& rect,const CameraDesc& view,
                 const ViewportState& state,const AuthoringEntity& entity)
{
    const CameraDesc camera=AuthoredCamera(entity);
    const Basis basis=CameraBasis(camera);
    const float aspect=camera.aspect>0?camera.aspect:std::max(0.001f,rect.GetWidth()/rect.GetHeight());
    const float nearD=std::max(0.01f,camera.nearClip);
    const float farD=std::min(std::max(nearD*1.01f,camera.farClip),std::max(20.0f,nearD*200.0f));
    const float tanHalf=std::tan(std::clamp(camera.fov,1.0f,179.0f)*0.5f*Pi/180.0f);

    auto plane=[&](float distance)
    {
        const float halfH=distance*tanHalf,halfW=halfH*aspect;
        const V3 center=Add(V(camera.position),Mul(basis.forward,distance));
        return std::array<V3,4>{
            Add(Add(center,Mul(basis.right,-halfW)),Mul(basis.up, halfH)),
            Add(Add(center,Mul(basis.right, halfW)),Mul(basis.up, halfH)),
            Add(Add(center,Mul(basis.right, halfW)),Mul(basis.up,-halfH)),
            Add(Add(center,Mul(basis.right,-halfW)),Mul(basis.up,-halfH))};
    };
    const auto nearP=plane(nearD),farP=plane(farD);
    std::array<Projected,4> n{},f{};
    for(int i=0;i<4;++i){n[i]=Project(nearP[i],view,state,rect);f[i]=Project(farP[i],view,state,rect);}
    const ImU32 colour=IM_COL32(86,205,255,210);
    draw->PushClipRect(rect.Min,rect.Max,true);
    for(int i=0;i<4;++i)
    {
        const int j=(i+1)%4;
        if(n[i].valid&&n[j].valid) draw->AddLine(n[i].point,n[j].point,colour,1.5f);
        if(f[i].valid&&f[j].valid) draw->AddLine(f[i].point,f[j].point,colour,1.2f);
        if(n[i].valid&&f[i].valid) draw->AddLine(n[i].point,f[i].point,colour,1.2f);
    }
    const auto origin=Project(V(camera.position),view,state,rect);
    if(origin.valid) draw->AddCircleFilled(origin.point,4.0f,colour);
    draw->PopClipRect();
}

void LogCameraError(Engine& engine,const std::exception& error)
{
    engine.Logger().Write(Severity::Error,"EDITOR-CAMERA",error.what());
}

} // namespace

void EditorLayer::AppendCameraInspector(Engine& engine,EditorSession& session,CameraDesc& editorCamera)
{
    AppendAnimationStateMachine(engine, session);

    const auto snapshot=session.ViewDocument().Snapshot();
    const AuthoringEntity* selected=SelectedCamera(snapshot);

    if(cameraPreview_)
    {
        if(!selected||selected->persistentID!=cameraPreviewPersistentID_||session.Playing())
        {
            if(cameraPreviewRestore_) editorCamera=*cameraPreviewRestore_;
            cameraPreview_=false;cameraPreviewPersistentID_=0;cameraPreviewRestore_.reset();
        }
        else
        {
            editorCamera=AuthoredCamera(*selected);
            viewport_.orthographic=false;
            engine.Graphics().SetCameraProjection(CameraProjection::Perspective);
            engine.Graphics().SetCamera(editorCamera);
        }
    }

    if(ImGuiWindow* viewportWindow=ImGui::FindWindowByName("Viewport");
       selected&&viewportWindow&&viewportWindow->WasActive&&!viewportWindow->Collapsed&&!cameraPreview_)
    {
        const ImRect rect=viewportWindow->WorkRect;
        if(rect.GetWidth()>40.0f&&rect.GetHeight()>40.0f)
            DrawFrustum(viewportWindow->DrawList,rect,editorCamera,viewport_,*selected);
    }

    if(!showInspector_||!session.EditingEnabled()) return;
    if(!ImGui::Begin("Inspector")){ImGui::End();return;}
    ImGui::SeparatorText("Camera Inspector");
    if(!selected)
    {
        ImGui::TextDisabled("Select an entity with a Camera component.");
        ImGui::End();
        return;
    }

    auto& document=session.EditDocument();
    CameraComponent camera=*selected->record.camera;
    auto commitCamera=[&]
    {
        try{document.SetCamera(selected->entity,camera);componentRevision_=document.Revision();}
        catch(const std::exception& error){LogCameraError(engine,error);componentRevision_=0;}
    };
    ImGui::DragFloat("FOV##CameraInspector",&camera.fov,0.25f,1.0f,179.0f,"%.1f deg");
    if(ImGui::IsItemDeactivatedAfterEdit()) commitCamera();
    ImGui::DragFloat("Near Clip##CameraInspector",&camera.nearClip,0.01f,0.001f,FLT_MAX,"%.3f");
    if(ImGui::IsItemDeactivatedAfterEdit()) commitCamera();
    ImGui::DragFloat("Far Clip##CameraInspector",&camera.farClip,1.0f,0.01f,FLT_MAX,"%.2f");
    if(ImGui::IsItemDeactivatedAfterEdit()) commitCamera();
    ImGui::DragFloat("Aspect Override##CameraInspector",&camera.aspect,0.01f,0.0f,10.0f,"%.3f");
    if(ImGui::IsItemDeactivatedAfterEdit()) commitCamera();

    if(selected->activeCamera)
    {
        if(ImGui::Button("Clear Active Camera##CameraInspector"))
        {
            try{document.ClearActiveCamera();componentRevision_=document.Revision();}
            catch(const std::exception& error){LogCameraError(engine,error);}
        }
    }
    else if(ImGui::Button("Make Active Camera##CameraInspector"))
    {
        try{document.SetActiveCamera(selected->entity);componentRevision_=document.Revision();}
        catch(const std::exception& error){LogCameraError(engine,error);}
    }

    if(ImGui::Button("Align Camera to View"))
    {
        try
        {
            Transform transform=selected->record.transform;
            transform.position=editorCamera.position;
            CameraComponent aligned=camera;
            aligned.target=editorCamera.target;
            aligned.fov=editorCamera.fov;
            aligned.nearClip=editorCamera.nearClip;
            aligned.farClip=editorCamera.farClip;
            aligned.aspect=editorCamera.aspect;
            document.SetTransform(selected->entity,transform);
            document.SetCamera(selected->entity,aligned);
            componentRevision_=document.Revision();
        }
        catch(const std::exception& error){LogCameraError(engine,error);}
    }
    ImGui::SameLine();
    if(ImGui::Button("Align View to Camera"))
    {
        editorCamera=AuthoredCamera(*selected);
        viewport_.orthographic=false;
        engine.Graphics().SetCameraProjection(CameraProjection::Perspective);
        engine.Graphics().SetCamera(editorCamera);
    }

    if(!cameraPreview_)
    {
        if(ImGui::Button("Preview Through Camera"))
        {
            cameraPreviewRestore_=editorCamera;
            cameraPreviewPersistentID_=selected->persistentID;
            cameraPreview_=true;
            editorCamera=AuthoredCamera(*selected);
            viewport_.orthographic=false;
            engine.Graphics().SetCameraProjection(CameraProjection::Perspective);
            engine.Graphics().SetCamera(editorCamera);
        }
    }
    else
    {
        if(ImGui::Button("Exit Camera Preview"))
        {
            if(cameraPreviewRestore_) editorCamera=*cameraPreviewRestore_;
            cameraPreview_=false;cameraPreviewPersistentID_=0;cameraPreviewRestore_.reset();
            engine.Graphics().SetCamera(editorCamera);
        }
        ImGui::SameLine();ImGui::TextDisabled("Scene View is rendering through this camera.");
    }

    ImGui::TextDisabled("Cyan viewport lines visualize the selected camera frustum.");
    ImGui::End();
}

} // namespace PlagC::Editor