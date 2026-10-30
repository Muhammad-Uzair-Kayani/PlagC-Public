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
constexpr float Pi=3.14159265358979323846f;
struct V3{float x=0,y=0,z=0;};
V3 V(Vec3 v){return {v.x,v.y,v.z};}
V3 Add(V3 a,V3 b){return {a.x+b.x,a.y+b.y,a.z+b.z};}
V3 Sub(V3 a,V3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
V3 Mul(V3 a,float s){return {a.x*s,a.y*s,a.z*s};}
float Dot(V3 a,V3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
V3 Cross(V3 a,V3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
float Length(V3 a){return std::sqrt(Dot(a,a));}
V3 Normalize(V3 a){const float n=Length(a);return n>1e-6f?Mul(a,1.0f/n):V3{};}
struct Basis{V3 forward,right,up;};
Basis CameraBasis(const CameraDesc& c){V3 f=Normalize(Sub(V(c.target),V(c.position)));if(Length(f)<1e-5f)f={0,0,-1};V3 u{0,1,0};if(std::fabs(Dot(f,u))>.999f)u={0,0,1};V3 r=Normalize(Cross(f,u));return {f,r,Normalize(Cross(r,f))};}
struct Projected{ImVec2 point{};bool valid=false;};
Projected Project(V3 world,const CameraDesc& camera,const ViewportState& state,const ImRect& rect){
 const auto basis=CameraBasis(camera);const V3 rel=Sub(world,V(camera.position));const float depth=Dot(rel,basis.forward);
 if(depth<=std::max(.001f,camera.nearClip))return {};
 const float aspect=camera.aspect>0?camera.aspect:std::max(.001f,rect.GetWidth()/rect.GetHeight());float x=0,y=0;
 if(state.orthographic){const float hh=std::max(.001f,state.orthographicHeight*.5f);x=Dot(rel,basis.right)/(hh*aspect);y=Dot(rel,basis.up)/hh;}
 else{const float f=1/std::tan(std::clamp(camera.fov,1.f,179.f)*.5f*Pi/180);x=(Dot(rel,basis.right)/depth)*f/aspect;y=(Dot(rel,basis.up)/depth)*f;}
 return {{rect.Min.x+(x*.5f+.5f)*rect.GetWidth(),rect.Min.y+(-y*.5f+.5f)*rect.GetHeight()},true};
}
V3 Rotate(V3 p,Vec3 degrees){
 const float rz=degrees.z*Pi/180,rx=degrees.x*Pi/180,ry=degrees.y*Pi/180;
 {const float c=std::cos(rz),s=std::sin(rz);p={p.x*c-p.y*s,p.x*s+p.y*c,p.z};}
 {const float c=std::cos(rx),s=std::sin(rx);p={p.x,p.y*c-p.z*s,p.y*s+p.z*c};}
 {const float c=std::cos(ry),s=std::sin(ry);p={p.x*c+p.z*s,p.y,-p.x*s+p.z*c};}
 return p;
}
V3 WorldPoint(const Transform& t,V3 local){local={local.x*t.scale.x,local.y*t.scale.y,local.z*t.scale.z};return Add(V(t.position),Rotate(local,t.rotationDegrees));}
void Segment(ImDrawList* draw,const ImRect& rect,const CameraDesc& camera,const ViewportState& state,V3 a,V3 b,ImU32 colour,float width=1.4f){const auto pa=Project(a,camera,state,rect),pb=Project(b,camera,state,rect);if(pa.valid&&pb.valid)draw->AddLine(pa.point,pb.point,colour,width);}
void Circle(ImDrawList* draw,const ImRect& rect,const CameraDesc& camera,const ViewportState& state,const Transform& t,V3 center,float radius,int plane,ImU32 colour){
 constexpr int Steps=32;V3 prev{};bool have=false;for(int i=0;i<=Steps;++i){const float a=2*Pi*float(i)/Steps;V3 p=center;if(plane==0){p.x+=std::cos(a)*radius;p.y+=std::sin(a)*radius;}else if(plane==1){p.x+=std::cos(a)*radius;p.z+=std::sin(a)*radius;}else{p.y+=std::cos(a)*radius;p.z+=std::sin(a)*radius;}p=WorldPoint(t,p);if(have)Segment(draw,rect,camera,state,prev,p,colour);prev=p;have=true;}}
void Box(ImDrawList* draw,const ImRect& rect,const CameraDesc& camera,const ViewportState& state,const Transform& t,Vec3 d,ImU32 colour){
 std::array<V3,8> p{{{-d.x,-d.y,-d.z},{d.x,-d.y,-d.z},{d.x,d.y,-d.z},{-d.x,d.y,-d.z},{-d.x,-d.y,d.z},{d.x,-d.y,d.z},{d.x,d.y,d.z},{-d.x,d.y,d.z}}};for(auto& v:p)v=WorldPoint(t,v);
 constexpr int e[][2]={{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};for(auto edge:e)Segment(draw,rect,camera,state,p[edge[0]],p[edge[1]],colour);
}
void DrawCollider(ImDrawList* draw,const ImRect& rect,const CameraDesc& camera,const ViewportState& state,const AuthoringEntity& entity,ImU32 colour){
 if(!entity.record.physics)return;const auto& c=entity.record.physics->collider;const auto& t=entity.record.transform;
 switch(c.type){
 case ColliderType::Box:Box(draw,rect,camera,state,t,c.dimensions,colour);break;
 case ColliderType::Sphere:Circle(draw,rect,camera,state,t,{},c.dimensions.x,0,colour);Circle(draw,rect,camera,state,t,{},c.dimensions.x,1,colour);Circle(draw,rect,camera,state,t,{},c.dimensions.x,2,colour);break;
 case ColliderType::Cylinder:{const float r=c.dimensions.x,h=c.dimensions.y;Circle(draw,rect,camera,state,t,{0,-h,0},r,1,colour);Circle(draw,rect,camera,state,t,{0,h,0},r,1,colour);for(int i=0;i<8;++i){const float a=2*Pi*i/8;Segment(draw,rect,camera,state,WorldPoint(t,{std::cos(a)*r,-h,std::sin(a)*r}),WorldPoint(t,{std::cos(a)*r,h,std::sin(a)*r}),colour);}break;}
 case ColliderType::Capsule:{const float r=c.dimensions.x,h=c.dimensions.y;Circle(draw,rect,camera,state,t,{0,-h,0},r,1,colour);Circle(draw,rect,camera,state,t,{0,h,0},r,1,colour);for(int i=0;i<8;++i){const float a=2*Pi*i/8;Segment(draw,rect,camera,state,WorldPoint(t,{std::cos(a)*r,-h,std::sin(a)*r}),WorldPoint(t,{std::cos(a)*r,h,std::sin(a)*r}),colour);}Circle(draw,rect,camera,state,t,{},h+r,0,colour);Circle(draw,rect,camera,state,t,{},h+r,2,colour);break;}
 case ColliderType::Mesh:
  if(c.geometry&&!c.geometry->indices.empty()){const auto& g=*c.geometry;const std::size_t limit=std::min<std::size_t>(g.indices.size(),12000);for(std::size_t i=0;i+2<limit;i+=3){const auto ia=g.indices[i],ib=g.indices[i+1],ic=g.indices[i+2];if(ia>=g.vertices.size()||ib>=g.vertices.size()||ic>=g.vertices.size())continue;const auto a=WorldPoint(t,V(g.vertices[ia])),b=WorldPoint(t,V(g.vertices[ib])),d=WorldPoint(t,V(g.vertices[ic]));Segment(draw,rect,camera,state,a,b,colour);Segment(draw,rect,camera,state,b,d,colour);Segment(draw,rect,camera,state,d,a,colour);}}else Box(draw,rect,camera,state,t,c.dimensions,colour);break;
 default:Box(draw,rect,camera,state,t,c.dimensions,colour);break;
 }
 const auto com=Project(WorldPoint(t,V(c.centerOfMassOffset)),camera,state,rect);if(com.valid){draw->AddLine({com.point.x-5,com.point.y},{com.point.x+5,com.point.y},colour,1.5f);draw->AddLine({com.point.x,com.point.y-5},{com.point.x,com.point.y+5},colour,1.5f);}
}
const char* LayerName(CollisionLayer layer){switch(layer){case CollisionLayer::StaticWorld:return "Static World";case CollisionLayer::Dynamic:return "Dynamic";case CollisionLayer::Character:return "Character";case CollisionLayer::Vehicle:return "Vehicle";case CollisionLayer::Projectile:return "Projectile";case CollisionLayer::Sensor:return "Sensor";case CollisionLayer::Debris:return "Debris";case CollisionLayer::Ragdoll:return "Ragdoll";default:return "Unknown";}}
const char* ColliderName(ColliderType type){switch(type){case ColliderType::Box:return "Box";case ColliderType::Sphere:return "Sphere";case ColliderType::Capsule:return "Capsule";case ColliderType::Cylinder:return "Cylinder";case ColliderType::ConvexHull:return "Convex Hull";case ColliderType::Compound:return "Compound";case ColliderType::Mesh:return "Mesh";case ColliderType::Heightfield:return "Heightfield";}return "Unknown";}
std::string Validate(const PhysicsComponent& p,const Transform& t){
 const auto& b=p.body;const auto& c=p.collider;if((b.collisionMask&~AllCollisionLayers)!=0)return "Collision mask contains an unknown layer bit.";
 if(b.sensor!=(b.layer==CollisionLayer::Sensor))return "Trigger bodies must use the Sensor collision layer.";
 if(!b.sensor&&((b.motion==MotionType::Static)!=(b.layer==CollisionLayer::StaticWorld)))return "Static bodies must use Static World; moving bodies must use a non-static layer.";
 if(!std::isfinite(b.mass)||b.mass<=0)return "Mass must be positive.";if(b.linearDamping<0||b.linearDamping>1||b.angularDamping<0||b.angularDamping>1)return "Linear and angular damping must be between 0 and 1.";
 if(c.dimensions.x<=0||c.dimensions.y<=0||c.dimensions.z<=0)return "Collider dimensions must be positive.";if(c.material.friction<0)return "Friction cannot be negative.";if(c.material.restitution<0||c.material.restitution>1)return "Restitution must be between 0 and 1.";
 if((c.type==ColliderType::Sphere||c.type==ColliderType::Capsule)&&!(t.scale.x==t.scale.y&&t.scale.x==t.scale.z))return "Sphere and capsule colliders require uniform Transform scale.";if(c.type==ColliderType::Cylinder&&t.scale.x!=t.scale.z)return "Cylinder colliders require matching X/Z Transform scale.";
 if(unsigned(c.type)>=unsigned(ColliderType::ConvexHull)&&!c.geometry)return "This geometry-backed collider has no collider geometry.";if((c.type==ColliderType::Mesh||c.type==ColliderType::Heightfield)&&b.motion!=MotionType::Static)return "Mesh and heightfield colliders are static-only.";return {};
}
void Log(Engine& engine,const std::exception& error){engine.Logger().Write(Severity::Error,"EDITOR-PHYSICS",error.what());}
}

void EditorLayer::AppendPhysicsInspector(Engine& engine,EditorSession& session,const CameraDesc& editorCamera)
{
 const auto entities=session.ViewDocument().Snapshot();const auto it=std::find_if(entities.begin(),entities.end(),[](const AuthoringEntity& e){return e.selected;});const AuthoringEntity* selected=it==entities.end()?nullptr:&*it;
 if(physicsDebugVisible_&&!session.Playing())if(ImGuiWindow* w=ImGui::FindWindowByName("Viewport");w&&w->WasActive&&!w->Collapsed){const ImRect rect=w->WorkRect;if(rect.GetWidth()>40&&rect.GetHeight()>40){w->DrawList->PushClipRect(rect.Min,rect.Max,true);for(const auto& entity:entities)if(entity.record.physics&&(physicsDebugAll_||entity.selected))DrawCollider(w->DrawList,rect,editorCamera,viewport_,entity,entity.selected?IM_COL32(255,190,55,235):IM_COL32(80,210,140,150));w->DrawList->PopClipRect();}}
 if(!showInspector_||!selected||!selected->record.physics)return;
 if(!ImGui::Begin("Inspector")){ImGui::End();return;}ImGui::SeparatorText("Physics Inspector");
 const bool readOnly=!session.EditingEnabled()||hierarchy_.Locked(selected->persistentID);PhysicsComponent physics=*selected->record.physics;std::string validation=Validate(physics,selected->record.transform);
 auto commit=[&]{validation=Validate(physics,selected->record.transform);if(readOnly||!validation.empty())return;try{session.EditDocument().SetPhysics(selected->entity,physics);componentPhysics_=physics;componentRevision_=session.EditDocument().Revision();}catch(const std::exception& e){Log(engine,e);}};
 ImGui::BeginDisabled(readOnly);
 if(ImGui::BeginTable("PhysicsAdvancedProperties",2,ImGuiTableFlags_SizingStretchProp|ImGuiTableFlags_BordersInnerV|ImGuiTableFlags_PadOuterX)){
  ImGui::TableSetupColumn("Property",ImGuiTableColumnFlags_WidthFixed,105);ImGui::TableSetupColumn("Value",ImGuiTableColumnFlags_WidthStretch);
  auto row=[](const char* label){ImGui::TableNextRow();ImGui::TableSetColumnIndex(0);ImGui::AlignTextToFramePadding();ImGui::TextDisabled("%s",label);ImGui::TableSetColumnIndex(1);ImGui::SetNextItemWidth(-FLT_MIN);};
  row("Trigger");bool sensor=physics.body.sensor;if(ImGui::Checkbox("##PhysicsTrigger",&sensor)){physics.body.sensor=sensor;physics.body.layer=sensor?CollisionLayer::Sensor:(physics.body.motion==MotionType::Static?CollisionLayer::StaticWorld:CollisionLayer::Dynamic);commit();}
  row("Layer");if(physics.body.sensor)ImGui::TextDisabled("Sensor (trigger-owned)");else if(ImGui::BeginCombo("##PhysicsLayer",LayerName(physics.body.layer))){for(CollisionLayer layer:{CollisionLayer::StaticWorld,CollisionLayer::Dynamic,CollisionLayer::Character,CollisionLayer::Vehicle,CollisionLayer::Projectile,CollisionLayer::Debris,CollisionLayer::Ragdoll}){const bool allowed=physics.body.motion==MotionType::Static?layer==CollisionLayer::StaticWorld:layer!=CollisionLayer::StaticWorld;ImGui::BeginDisabled(!allowed);const bool current=physics.body.layer==layer;if(ImGui::Selectable(LayerName(layer),current)){physics.body.layer=layer;commit();}if(current)ImGui::SetItemDefaultFocus();ImGui::EndDisabled();}ImGui::EndCombo();}
  row("Collision Mask");if(ImGui::BeginCombo("##PhysicsMask","Edit layers...")){if(ImGui::Button("All")){physics.body.collisionMask=AllCollisionLayers;commit();}ImGui::SameLine();if(ImGui::Button("None")){physics.body.collisionMask=0;commit();}for(unsigned i=0;i<CollisionMatrix::Count;++i){const auto layer=static_cast<CollisionLayer>(i);bool on=(physics.body.collisionMask&CollisionLayerBit(layer))!=0;if(ImGui::Checkbox(LayerName(layer),&on)){if(on)physics.body.collisionMask|=CollisionLayerBit(layer);else physics.body.collisionMask&=~CollisionLayerBit(layer);commit();}}ImGui::EndCombo();}
  row("Linear Damping");if(ImGui::DragFloat("##PhysicsLinearDamping",&physics.body.linearDamping,.01f,0,1,"%.3f")&&ImGui::IsItemDeactivatedAfterEdit())commit();
  row("Angular Damping");if(ImGui::DragFloat("##PhysicsAngularDamping",&physics.body.angularDamping,.01f,0,1,"%.3f")&&ImGui::IsItemDeactivatedAfterEdit())commit();
  row("Sleep");if(ImGui::Checkbox("##PhysicsSleep",&physics.body.allowSleep))commit();
  row("Continuous");if(ImGui::Checkbox("##PhysicsCCD",&physics.body.continuous))commit();
  row("Collision Group");int group=int(physics.body.collisionGroup);if(ImGui::InputInt("##PhysicsCollisionGroup",&group,1,100)&&ImGui::IsItemDeactivatedAfterEdit()){physics.body.collisionGroup=std::uint32_t(std::max(0,group));commit();}
  row("Center of Mass");if(ImGui::DragFloat3("##PhysicsCOM",&physics.collider.centerOfMassOffset.x,.01f)&&ImGui::IsItemDeactivatedAfterEdit())commit();
  row("Collider Type");ImGui::TextUnformatted(ColliderName(physics.collider.type));
  ImGui::EndTable();
 }
 ImGui::EndDisabled();
 if(!validation.empty()){ImGui::PushStyleColor(ImGuiCol_Text,ImVec4(1,.53f,.30f,1));ImGui::TextWrapped("%s",validation.c_str());ImGui::PopStyleColor();}
 ImGui::SeparatorText("Collider Debug");ImGui::Checkbox("Show collider debug",&physicsDebugVisible_);ImGui::SameLine();ImGui::BeginDisabled(!physicsDebugVisible_);ImGui::Checkbox("All physics entities",&physicsDebugAll_);ImGui::EndDisabled();
 ImGui::TextDisabled("Yellow: selected collider. Green: other physics colliders. Debug drawing is editor-only and hidden during Play.");
 if(unsigned(physics.collider.type)>=unsigned(ColliderType::ConvexHull))ImGui::TextDisabled("Geometry-backed collider data is visualized when available; this milestone does not synthesize imported collision geometry.");
 ImGui::End();
}

} // namespace PlagC::Editor
