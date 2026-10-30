#include "pgcpch.h"
#include "EditorLayer.h"
#include <PlagC/Application.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <string>

namespace PlagC::Editor {
namespace {

constexpr float Pi = 3.14159265358979323846f;
struct V3 { float x=0, y=0, z=0; };
V3 V(Vec3 v) { return {v.x,v.y,v.z}; }
V3 Add(V3 a,V3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
V3 Sub(V3 a,V3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
V3 Mul(V3 a,float s) { return {a.x*s,a.y*s,a.z*s}; }
float Dot(V3 a,V3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
V3 Cross(V3 a,V3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
float Length(V3 a) { return std::sqrt(Dot(a,a)); }
V3 Normalize(V3 a) { const float n=Length(a); return n>1e-6f?Mul(a,1.0f/n):V3{}; }

struct Basis { V3 forward,right,up; };
Basis CameraBasis(const CameraDesc& camera)
{
    V3 forward=Normalize(Sub(V(camera.target),V(camera.position)));
    if(Length(forward)<1e-5f) forward={0,0,-1};
    V3 reference={0,1,0};
    if(std::fabs(Dot(forward,reference))>.999f) reference={0,0,1};
    V3 right=Normalize(Cross(forward,reference));
    return {forward,right,Normalize(Cross(right,forward))};
}

struct Projected { ImVec2 point{}; bool valid=false; };
Projected Project(V3 world,const CameraDesc& camera,const ViewportState& state,const ImRect& rect)
{
    const auto basis=CameraBasis(camera);
    const V3 rel=Sub(world,V(camera.position));
    const float depth=Dot(rel,basis.forward);
    if(depth<=std::max(.001f,camera.nearClip)) return {};
    const float aspect=camera.aspect>0?camera.aspect:std::max(.001f,rect.GetWidth()/rect.GetHeight());
    float x=0,y=0;
    if(state.orthographic)
    {
        const float halfHeight=std::max(.001f,state.orthographicHeight*.5f);
        x=Dot(rel,basis.right)/(halfHeight*aspect);
        y=Dot(rel,basis.up)/halfHeight;
    }
    else
    {
        const float f=1.0f/std::tan(std::clamp(camera.fov,1.0f,179.0f)*.5f*Pi/180.0f);
        x=(Dot(rel,basis.right)/depth)*f/aspect;
        y=(Dot(rel,basis.up)/depth)*f;
    }
    return {{rect.Min.x+(x*.5f+.5f)*rect.GetWidth(),rect.Min.y+(-y*.5f+.5f)*rect.GetHeight()},true};
}

V3 Rotate(V3 point,Vec3 degrees)
{
    const float rz=degrees.z*Pi/180.0f,rx=degrees.x*Pi/180.0f,ry=degrees.y*Pi/180.0f;
    {const float c=std::cos(rz),s=std::sin(rz);point={point.x*c-point.y*s,point.x*s+point.y*c,point.z};}
    {const float c=std::cos(rx),s=std::sin(rx);point={point.x,point.y*c-point.z*s,point.y*s+point.z*c};}
    {const float c=std::cos(ry),s=std::sin(ry);point={point.x*c+point.z*s,point.y,-point.x*s+point.z*c};}
    return point;
}

V3 WorldPoint(const Transform& transform,V3 local)
{
    local={local.x*transform.scale.x,local.y*transform.scale.y,local.z*transform.scale.z};
    return Add(V(transform.position),Rotate(local,transform.rotationDegrees));
}

void Segment(ImDrawList* draw,const ImRect& rect,const CameraDesc& camera,const ViewportState& state,
             V3 a,V3 b,ImU32 colour,float width=1.4f)
{
    const auto pa=Project(a,camera,state,rect),pb=Project(b,camera,state,rect);
    if(pa.valid&&pb.valid) draw->AddLine(pa.point,pb.point,colour,width);
}

void Circle(ImDrawList* draw,const ImRect& rect,const CameraDesc& camera,const ViewportState& state,
            const Transform& transform,V3 center,float radius,int plane,ImU32 colour)
{
    constexpr int Steps=32;
    V3 previous{};bool havePrevious=false;
    for(int i=0;i<=Steps;++i)
    {
        const float angle=2.0f*Pi*float(i)/float(Steps);
        const float c=std::cos(angle),s=std::sin(angle);
        V3 point=center;
        if(plane==0){point.x+=c*radius;point.y+=s*radius;}
        else if(plane==1){point.x+=c*radius;point.z+=s*radius;}
        else{point.y+=c*radius;point.z+=s*radius;}
        point=WorldPoint(transform,point);
        if(havePrevious) Segment(draw,rect,camera,state,previous,point,colour);
        previous=point;havePrevious=true;
    }
}

void Box(ImDrawList* draw,const ImRect& rect,const CameraDesc& camera,const ViewportState& state,
         const Transform& transform,Vec3 dimensions,ImU32 colour)
{
    std::array<V3,8> points{{
        {-dimensions.x,-dimensions.y,-dimensions.z},{ dimensions.x,-dimensions.y,-dimensions.z},
        { dimensions.x, dimensions.y,-dimensions.z},{-dimensions.x, dimensions.y,-dimensions.z},
        {-dimensions.x,-dimensions.y, dimensions.z},{ dimensions.x,-dimensions.y, dimensions.z},
        { dimensions.x, dimensions.y, dimensions.z},{-dimensions.x, dimensions.y, dimensions.z}}};
    for(auto& point:points) point=WorldPoint(transform,point);
    constexpr int edges[][2]={{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
    for(const auto& edge:edges) Segment(draw,rect,camera,state,points[edge[0]],points[edge[1]],colour);
}

void DrawCollider(ImDrawList* draw,const ImRect& rect,const CameraDesc& camera,const ViewportState& state,
                  const AuthoringEntity& entity,ImU32 colour)
{
    if(!entity.record.physics) return;
    const auto& collider=entity.record.physics->collider;
    const auto& transform=entity.record.transform;
    switch(collider.type)
    {
    case ColliderType::Box:
        Box(draw,rect,camera,state,transform,collider.dimensions,colour);break;
    case ColliderType::Sphere:
        Circle(draw,rect,camera,state,transform,{},collider.dimensions.x,0,colour);
        Circle(draw,rect,camera,state,transform,{},collider.dimensions.x,1,colour);
        Circle(draw,rect,camera,state,transform,{},collider.dimensions.x,2,colour);break;
    case ColliderType::Cylinder:
    case ColliderType::Capsule:
    {
        const float radius=collider.dimensions.x,halfHeight=collider.dimensions.y;
        Circle(draw,rect,camera,state,transform,{0,-halfHeight,0},radius,1,colour);
        Circle(draw,rect,camera,state,transform,{0, halfHeight,0},radius,1,colour);
        for(int i=0;i<8;++i)
        {
            const float angle=2.0f*Pi*float(i)/8.0f,c=std::cos(angle),s=std::sin(angle);
            Segment(draw,rect,camera,state,
                WorldPoint(transform,{c*radius,-halfHeight,s*radius}),
                WorldPoint(transform,{c*radius, halfHeight,s*radius}),colour);
        }
        if(collider.type==ColliderType::Capsule)
        {
            Circle(draw,rect,camera,state,transform,{},halfHeight+radius,0,colour);
            Circle(draw,rect,camera,state,transform,{},halfHeight+radius,2,colour);
        }
        break;
    }
    case ColliderType::Mesh:
        if(collider.geometry&&!collider.geometry->indices.empty())
        {
            const auto& geometry=*collider.geometry;
            const std::size_t limit=std::min<std::size_t>(geometry.indices.size(),12000);
            for(std::size_t i=0;i+2<limit;i+=3)
            {
                const auto ia=geometry.indices[i],ib=geometry.indices[i+1],ic=geometry.indices[i+2];
                if(ia>=geometry.vertices.size()||ib>=geometry.vertices.size()||ic>=geometry.vertices.size()) continue;
                const auto a=WorldPoint(transform,V(geometry.vertices[ia]));
                const auto b=WorldPoint(transform,V(geometry.vertices[ib]));
                const auto c=WorldPoint(transform,V(geometry.vertices[ic]));
                Segment(draw,rect,camera,state,a,b,colour);Segment(draw,rect,camera,state,b,c,colour);Segment(draw,rect,camera,state,c,a,colour);
            }
        }
        else Box(draw,rect,camera,state,transform,collider.dimensions,colour);
        break;
    default:
        // Convex/compound/heightfield geometry has no stable editor edge topology in the authoring contract.
        // Draw its authored bounds rather than depending on Jolt debug-renderer implementation objects.
        Box(draw,rect,camera,state,transform,collider.dimensions,colour);break;
    }

    const auto com=Project(WorldPoint(transform,V(collider.centerOfMassOffset)),camera,state,rect);
    if(com.valid)
    {
        draw->AddLine({com.point.x-5,com.point.y},{com.point.x+5,com.point.y},colour,1.5f);
        draw->AddLine({com.point.x,com.point.y-5},{com.point.x,com.point.y+5},colour,1.5f);
    }
}

const char* MotionName(MotionType motion)
{
    switch(motion){case MotionType::Static:return "Static";case MotionType::Dynamic:return "Dynamic";case MotionType::Kinematic:return "Kinematic";}
    return "Unknown";
}
const char* LayerName(CollisionLayer layer)
{
    switch(layer){case CollisionLayer::StaticWorld:return "Static World";case CollisionLayer::Dynamic:return "Dynamic";case CollisionLayer::Character:return "Character";case CollisionLayer::Vehicle:return "Vehicle";case CollisionLayer::Projectile:return "Projectile";case CollisionLayer::Sensor:return "Sensor";case CollisionLayer::Debris:return "Debris";case CollisionLayer::Ragdoll:return "Ragdoll";default:return "Unknown";}
}
const char* ColliderName(ColliderType type)
{
    switch(type){case ColliderType::Box:return "Box";case ColliderType::Sphere:return "Sphere";case ColliderType::Capsule:return "Capsule";case ColliderType::Cylinder:return "Cylinder";case ColliderType::ConvexHull:return "Convex Hull";case ColliderType::Compound:return "Compound";case ColliderType::Mesh:return "Mesh";case ColliderType::Heightfield:return "Heightfield";}
    return "Unknown";
}

std::string Validate(const PhysicsComponent& physics,const Transform& transform)
{
    const auto& body=physics.body;const auto& collider=physics.collider;
    if((body.collisionMask&~AllCollisionLayers)!=0) return "Collision mask contains an unknown layer bit.";
    if(body.sensor!=(body.layer==CollisionLayer::Sensor)) return "Trigger bodies must use the Sensor collision layer.";
    if(!body.sensor&&((body.motion==MotionType::Static)!=(body.layer==CollisionLayer::StaticWorld))) return "Static bodies must use Static World; moving bodies must use a non-static layer.";
    if(!std::isfinite(body.mass)||body.mass<=0) return "Mass must be positive.";
    if(!std::isfinite(body.gravityScale)) return "Gravity scale must be finite.";
    if(!std::isfinite(body.linearDamping)||body.linearDamping<0||body.linearDamping>1||!std::isfinite(body.angularDamping)||body.angularDamping<0||body.angularDamping>1) return "Linear and angular damping must be between 0 and 1.";
    if(collider.dimensions.x<=0||collider.dimensions.y<=0||collider.dimensions.z<=0) return "Collider dimensions must be positive.";
    if(!std::isfinite(collider.material.friction)||collider.material.friction<0) return "Friction cannot be negative.";
    if(!std::isfinite(collider.material.restitution)||collider.material.restitution<0||collider.material.restitution>1) return "Restitution must be between 0 and 1.";
    if((collider.type==ColliderType::Sphere||collider.type==ColliderType::Capsule)&&!(transform.scale.x==transform.scale.y&&transform.scale.x==transform.scale.z)) return "Sphere and capsule colliders require uniform Transform scale.";
    if(collider.type==ColliderType::Cylinder&&transform.scale.x!=transform.scale.z) return "Cylinder colliders require matching X/Z Transform scale.";
    if(unsigned(collider.type)>=unsigned(ColliderType::ConvexHull)&&!collider.geometry) return "This geometry-backed collider has no collider geometry.";
    if((collider.type==ColliderType::Mesh||collider.type==ColliderType::Heightfield)&&body.motion!=MotionType::Static) return "Mesh and heightfield colliders are static-only.";
    return {};
}

void Log(Engine& engine,const std::exception& error)
{
    engine.Logger().Write(Severity::Error,"EDITOR-PHYSICS",error.what());
}

} // namespace

void EditorLayer::AppendPhysicsInspector(Engine& engine,EditorSession& session,const CameraDesc& editorCamera)
{
    const auto entities=session.ViewDocument().Snapshot();
    const auto selectedIt=std::find_if(entities.begin(),entities.end(),[](const AuthoringEntity& entity){return entity.selected;});
    const AuthoringEntity* selected=selectedIt==entities.end()?nullptr:&*selectedIt;

    if(physicsDebugVisible_&&!session.Playing())
    {
        if(ImGuiWindow* viewportWindow=ImGui::FindWindowByName("Viewport");viewportWindow&&viewportWindow->WasActive&&!viewportWindow->Collapsed)
        {
            const ImRect rect=viewportWindow->WorkRect;
            if(rect.GetWidth()>40&&rect.GetHeight()>40)
            {
                viewportWindow->DrawList->PushClipRect(rect.Min,rect.Max,true);
                for(const auto& entity:entities)
                    if(entity.record.physics&&(physicsDebugAll_||entity.selected))
                        DrawCollider(viewportWindow->DrawList,rect,editorCamera,viewport_,entity,
                            entity.selected?IM_COL32(255,190,55,235):IM_COL32(80,210,140,150));
                viewportWindow->DrawList->PopClipRect();
            }
        }
    }

    if(!showInspector_||!selected||!selected->record.physics) return;
    if(!ImGui::Begin("Inspector")){ImGui::End();return;}
    ImGui::SeparatorText("Physics Inspector");

    const bool readOnly=!session.EditingEnabled()||hierarchy_.Locked(selected->persistentID);
    PhysicsComponent physics=*selected->record.physics;
    std::string validation=Validate(physics,selected->record.transform);
    auto commit=[&]
    {
        validation=Validate(physics,selected->record.transform);
        if(readOnly||!validation.empty()) return;
        try
        {
            session.EditDocument().SetPhysics(selected->entity,physics);
            componentPhysics_=physics;
            componentRevision_=session.EditDocument().Revision();
        }
        catch(const std::exception& error){Log(engine,error);componentRevision_=0;}
    };

    ImGui::BeginDisabled(readOnly);
    if(ImGui::BeginTable("PhysicsMilestone10Properties",2,
        ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_BordersInnerV|ImGuiTableFlags_PadOuterX))
    {
        ImGui::TableSetupColumn("Property",ImGuiTableColumnFlags_WidthFixed,110.0f);
        ImGui::TableSetupColumn("Value",ImGuiTableColumnFlags_WidthStretch);
        auto row=[](const char* label)
        {
            ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled("%s",label);ImGui::TableSetColumnIndex(1);ImGui::SetNextItemWidth(-FLT_MIN);
        };

        row("Body Type");
        if(ImGui::BeginCombo("##PhysicsBodyType",MotionName(physics.body.motion)))
        {
            for(MotionType motion:{MotionType::Static,MotionType::Dynamic,MotionType::Kinematic})
            {
                const bool current=physics.body.motion==motion;
                if(ImGui::Selectable(MotionName(motion),current))
                {
                    physics.body.motion=motion;
                    if(physics.body.sensor) physics.body.layer=CollisionLayer::Sensor;
                    else physics.body.layer=motion==MotionType::Static?CollisionLayer::StaticWorld:CollisionLayer::Dynamic;
                    if((physics.collider.type==ColliderType::Mesh||physics.collider.type==ColliderType::Heightfield)&&motion!=MotionType::Static)
                        physics.collider.type=ColliderType::Box;
                    commit();
                }
                if(current) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        row("Mass");
        ImGui::BeginDisabled(physics.body.motion!=MotionType::Dynamic);
        ImGui::DragFloat("##PhysicsMass",&physics.body.mass,.05f,.001f,FLT_MAX,"%.3f");
        if(ImGui::IsItemDeactivatedAfterEdit()) commit();
        ImGui::EndDisabled();

        row("Gravity Scale");
        ImGui::DragFloat("##PhysicsGravity",&physics.body.gravityScale,.01f,-100.0f,100.0f,"%.3f");
        if(ImGui::IsItemDeactivatedAfterEdit()) commit();

        row("Trigger");
        bool sensor=physics.body.sensor;
        if(ImGui::Checkbox("##PhysicsTrigger",&sensor))
        {
            physics.body.sensor=sensor;
            physics.body.layer=sensor?CollisionLayer::Sensor:(physics.body.motion==MotionType::Static?CollisionLayer::StaticWorld:CollisionLayer::Dynamic);
            commit();
        }

        row("Layer");
        if(physics.body.sensor) ImGui::TextDisabled("Sensor (trigger-owned)");
        else if(ImGui::BeginCombo("##PhysicsLayer",LayerName(physics.body.layer)))
        {
            for(CollisionLayer layer:{CollisionLayer::StaticWorld,CollisionLayer::Dynamic,CollisionLayer::Character,CollisionLayer::Vehicle,CollisionLayer::Projectile,CollisionLayer::Debris,CollisionLayer::Ragdoll})
            {
                const bool allowed=physics.body.motion==MotionType::Static?layer==CollisionLayer::StaticWorld:layer!=CollisionLayer::StaticWorld;
                ImGui::BeginDisabled(!allowed);
                const bool current=physics.body.layer==layer;
                if(ImGui::Selectable(LayerName(layer),current)){physics.body.layer=layer;commit();}
                if(current) ImGui::SetItemDefaultFocus();
                ImGui::EndDisabled();
            }
            ImGui::EndCombo();
        }

        row("Collision Mask");
        if(ImGui::BeginCombo("##PhysicsMask","Edit layers..."))
        {
            if(ImGui::Button("All")){physics.body.collisionMask=AllCollisionLayers;commit();}
            ImGui::SameLine();
            if(ImGui::Button("None")){physics.body.collisionMask=0;commit();}
            ImGui::Separator();
            for(unsigned i=0;i<CollisionMatrix::Count;++i)
            {
                const auto layer=static_cast<CollisionLayer>(i);
                bool enabled=(physics.body.collisionMask&CollisionLayerBit(layer))!=0;
                if(ImGui::Checkbox(LayerName(layer),&enabled))
                {
                    if(enabled) physics.body.collisionMask|=CollisionLayerBit(layer);
                    else physics.body.collisionMask&=~CollisionLayerBit(layer);
                    commit();
                }
            }
            ImGui::EndCombo();
        }

        row("Collider Type");
        if(ImGui::BeginCombo("##PhysicsColliderType",ColliderName(physics.collider.type)))
        {
            for(ColliderType type:{ColliderType::Box,ColliderType::Sphere,ColliderType::Capsule,ColliderType::Cylinder})
            {
                const bool current=physics.collider.type==type;
                if(ImGui::Selectable(ColliderName(type),current)){physics.collider.type=type;physics.collider.geometry.reset();commit();}
                if(current) ImGui::SetItemDefaultFocus();
            }
            ImGui::Separator();
            for(ColliderType type:{ColliderType::ConvexHull,ColliderType::Compound,ColliderType::Mesh,ColliderType::Heightfield})
            {
                const bool current=physics.collider.type==type;
                ImGui::BeginDisabled(!current);
                ImGui::Selectable(ColliderName(type),current);
                ImGui::EndDisabled();
            }
            ImGui::EndCombo();
        }

        row("Dimensions");
        ImGui::DragFloat3("##PhysicsDimensions",&physics.collider.dimensions.x,.01f,.001f,FLT_MAX,"%.3f");
        if(ImGui::IsItemDeactivatedAfterEdit()) commit();

        row("Friction");
        ImGui::DragFloat("##PhysicsFriction",&physics.collider.material.friction,.01f,0.0f,FLT_MAX,"%.3f");
        if(ImGui::IsItemDeactivatedAfterEdit()) commit();

        row("Restitution");
        ImGui::DragFloat("##PhysicsRestitution",&physics.collider.material.restitution,.01f,0.0f,1.0f,"%.3f");
        if(ImGui::IsItemDeactivatedAfterEdit()) commit();

        row("Linear Damping");
        ImGui::DragFloat("##PhysicsLinearDamping",&physics.body.linearDamping,.01f,0.0f,1.0f,"%.3f");
        if(ImGui::IsItemDeactivatedAfterEdit()) commit();

        row("Angular Damping");
        ImGui::DragFloat("##PhysicsAngularDamping",&physics.body.angularDamping,.01f,0.0f,1.0f,"%.3f");
        if(ImGui::IsItemDeactivatedAfterEdit()) commit();

        row("Allow Sleep");
        if(ImGui::Checkbox("##PhysicsSleep",&physics.body.allowSleep)) commit();

        row("Continuous CCD");
        if(ImGui::Checkbox("##PhysicsCCD",&physics.body.continuous)) commit();

        row("Collision Group");
        int group=static_cast<int>(std::min<std::uint32_t>(physics.body.collisionGroup,static_cast<std::uint32_t>(INT_MAX)));
        ImGui::InputInt("##PhysicsCollisionGroup",&group,1,100);
        if(ImGui::IsItemDeactivatedAfterEdit()){physics.body.collisionGroup=static_cast<std::uint32_t>(std::max(0,group));commit();}

        row("Center of Mass");
        ImGui::DragFloat3("##PhysicsCOM",&physics.collider.centerOfMassOffset.x,.01f);
        if(ImGui::IsItemDeactivatedAfterEdit()) commit();

        ImGui::EndTable();
    }
    ImGui::EndDisabled();

    validation=Validate(physics,selected->record.transform);
    if(!validation.empty())
    {
        ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(1.0f,.53f,.30f,1.0f));
        ImGui::TextWrapped("%s",validation.c_str());
        ImGui::PopStyleColor();
    }

    ImGui::SeparatorText("Collider Debug");
    ImGui::Checkbox("Show collider debug",&physicsDebugVisible_);
    ImGui::SameLine();
    ImGui::BeginDisabled(!physicsDebugVisible_);
    ImGui::Checkbox("All physics entities",&physicsDebugAll_);
    ImGui::EndDisabled();
    ImGui::TextDisabled("Yellow: selected collider. Green: other physics colliders. Debug drawing is editor-only and hidden during Play.");
    if(unsigned(physics.collider.type)>=unsigned(ColliderType::ConvexHull))
        ImGui::TextDisabled("Geometry-backed collider types retain their imported/runtime geometry; the Inspector never fabricates geometry data.");

    ImGui::End();
}

} // namespace PlagC::Editor
