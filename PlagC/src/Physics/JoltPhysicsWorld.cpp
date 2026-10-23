#include "pgcpch.h"
#include <PlagC/Physics/PhysicsWorld.h>
#include <PlagC/Thread.h>
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/CastResult.h>
namespace PlagC {
namespace {
using Clock=std::chrono::steady_clock;
double Ms(Clock::time_point start){return std::chrono::duration<double,std::milli>(Clock::now()-start).count();}
void Require(bool ok,const char* what){if(!ok)throw std::invalid_argument(what);}
JPH::Vec3 V(Vec3 v){Require(std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z),"nonfinite physics vector");return {v.x,v.y,v.z};}
Vec3 V(JPH::Vec3Arg v){return {v.GetX(),v.GetY(),v.GetZ()};}
JPH::Quat Rotation(Vec3 e){constexpr float rad=3.14159265358979323846f/180;return JPH::Quat::sRotation(JPH::Vec3::sAxisY(),e.y*rad)*JPH::Quat::sRotation(JPH::Vec3::sAxisX(),e.x*rad)*JPH::Quat::sRotation(JPH::Vec3::sAxisZ(),e.z*rad);}
Vec3 Euler(JPH::QuatArg q){
 constexpr float deg=180/3.14159265358979323846f;
 const auto x=q*JPH::Vec3::sAxisX(),y=q*JPH::Vec3::sAxisY(),z=q*JPH::Vec3::sAxisZ();
 const float pitch=std::asin(std::clamp(-z.GetY(),-1.f,1.f));
 if(std::abs(std::cos(pitch))<1e-5f)return {pitch*deg,std::atan2(-x.GetZ(),x.GetX())*deg,0};
 return {pitch*deg,std::atan2(z.GetX(),z.GetZ())*deg,std::atan2(x.GetY(),y.GetY())*deg};
}
struct Runtime {
 Runtime(){JPH::RegisterDefaultAllocator();if(JPH::Factory::sInstance)throw std::logic_error("Jolt factory already owned externally");JPH::Factory::sInstance=new JPH::Factory;JPH::RegisterTypes();}
 ~Runtime(){JPH::UnregisterTypes();delete JPH::Factory::sInstance;JPH::Factory::sInstance=nullptr;}
};
std::shared_ptr<Runtime> Acquire(){static std::weak_ptr<Runtime> weak;auto r=weak.lock();if(!r){r=std::make_shared<Runtime>();weak=r;}return r;}
std::atomic<std::uint64_t> nextHandle{1};
struct Broad final:JPH::BroadPhaseLayerInterface {
 JPH::uint GetNumBroadPhaseLayers()const override{return 2;}
 JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer l)const override{return JPH::BroadPhaseLayer(l==0?0:1);}
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
 const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer l)const override{return l.GetValue()==0?"Static":"Moving";}
#endif
};
struct Pairs final:JPH::ObjectLayerPairFilter {
 CollisionMatrix matrix;
 explicit Pairs(const CollisionMatrix& m):matrix(m){}
 bool ShouldCollide(JPH::ObjectLayer a,JPH::ObjectLayer b)const override{return matrix.pairs[a][b];}
};
struct BroadFilter final:JPH::ObjectVsBroadPhaseLayerFilter {
 const Pairs& pairs;explicit BroadFilter(const Pairs& p):pairs(p){}
 bool ShouldCollide(JPH::ObjectLayer a,JPH::BroadPhaseLayer b)const override{
  if(b.GetValue()==0)return pairs.matrix.pairs[a][0];
  for(unsigned i=1;i<CollisionMatrix::Count;++i)if(pairs.matrix.pairs[a][i])return true;
  return false;
 }
};
JPH::ShapeRefC Shape(const PhysicsBodyDesc& d){
 const auto& c=d.component.collider;auto s=d.transform.scale;auto v=c.dimensions;
 JPH::ShapeSettings::ShapeResult result;
 switch(c.type){
 case ColliderType::Box:result=JPH::BoxShapeSettings({v.x*s.x,v.y*s.y,v.z*s.z},std::min({.05f,v.x*s.x*.1f,v.y*s.y*.1f,v.z*s.z*.1f})).Create();break;
 case ColliderType::Sphere:result=JPH::SphereShapeSettings(v.x*s.x).Create();break;
 case ColliderType::Capsule:result=JPH::CapsuleShapeSettings(v.y*s.y,v.x*s.x).Create();break;
 case ColliderType::Cylinder:result=JPH::CylinderShapeSettings(v.y*s.y,v.x*s.x,std::min({.05f,v.x*s.x*.1f,v.y*s.y*.1f})).Create();break;
 }
 if(result.HasError())throw std::invalid_argument(result.GetError().c_str());
 return result.Get();
}
JPH::EMotionType Motion(MotionType t){switch(t){case MotionType::Static:return JPH::EMotionType::Static;case MotionType::Kinematic:return JPH::EMotionType::Kinematic;default:return JPH::EMotionType::Dynamic;}}
}
struct PhysicsWorld::Impl {
 std::shared_ptr<Runtime> runtime;
 PhysicsConfig config;Broad broad;Pairs pairs;BroadFilter filter;
 JPH::TempAllocatorMalloc allocator;
 JPH::JobSystemThreadPool jobs;
 JPH::PhysicsSystem physics;
 struct Record {JPH::BodyID id;PhysicsBodyDesc desc;JPH::Vec3 previous,current;JPH::Quat prevRotation,rotation;std::uint64_t stamp=0;};
 std::unordered_map<std::uint64_t,Record> records;
 std::vector<std::pair<std::uint64_t,Record*>> slots;
 std::vector<PhysicsPoseUpdate> changed;
 std::vector<PhysicsBodyHandle> teleports;
 JPH::BodyIDVector before,after;
 std::uint64_t epoch=0;
 PhysicsStats stats;
 explicit Impl(const PhysicsConfig& c):runtime(Acquire()),config(c),pairs(c.collisions),filter(pairs),jobs(JPH::cMaxPhysicsJobs,JPH::cMaxPhysicsBarriers,int(c.workers)) {
  slots.resize(c.maxBodies);changed.reserve(c.maxBodies);teleports.reserve(c.maxBodies);before.reserve(c.maxBodies);after.reserve(c.maxBodies);
  physics.Init(c.maxBodies,0,c.maxBodyPairs,c.maxContacts,broad,filter,pairs);physics.SetGravity(V(c.gravity));stats.workers=c.workers;
 }
 Record& Get(PhysicsBodyHandle h){Thread::RequireMain();auto it=records.find(h.value);if(it==records.end())throw std::invalid_argument("invalid stale or foreign physics handle");return it->second;}
 Transform Pose(const Record& r)const{return {V(r.current),Euler(r.rotation),r.desc.transform.scale};}
 void CheckPose(Record& r,const Transform& t){auto d=r.desc;d.transform=t;ValidatePhysicsBody(d);Require(t.scale.x==r.desc.transform.scale.x&&t.scale.y==r.desc.transform.scale.y&&t.scale.z==r.desc.transform.scale.z,"physics scale changes require collider rebuild");}
 void Publish(JPH::BodyID id){
  auto [handle,r]=slots[id.GetIndex()];if(!r||r->id!=id||r->stamp==epoch)return;r->stamp=epoch;
  const auto& bi=physics.GetBodyInterface();
  r->previous=r->current;r->prevRotation=r->rotation;
  JPH::RVec3 p;bi.GetPositionAndRotation(id,p,r->rotation);r->current=JPH::Vec3(p);
  const bool active=bi.IsActive(id);
  if(!active){r->previous=r->current;r->prevRotation=r->rotation;} // Publish final settled pose, no lingering interpolation.
  changed.push_back({{handle},Pose(*r),active});
 }
};
PhysicsWorld::PhysicsWorld(const PhysicsConfig& c){
 Thread::RequireMain();Require(c.maxBodies>0&&c.maxBodies<=1000000&&c.maxBodyPairs>0&&c.maxBodyPairs<=10000000&&c.maxContacts>0&&c.maxContacts<=1000000&&c.workers<=64,"invalid physics capacity/worker budget");
 Require(std::isfinite(c.fixedHz)&&c.fixedHz>=1&&c.fixedHz<=240,"invalid physics frequency");(void)V(c.gravity);
 for(unsigned a=0;a<CollisionMatrix::Count;++a)for(unsigned b=0;b<CollisionMatrix::Count;++b)Require(c.collisions.pairs[a][b]==c.collisions.pairs[b][a],"asymmetric collision matrix");
 impl_=std::make_unique<Impl>(c);
}
PhysicsWorld::~PhysicsWorld(){Thread::RequireMain();if(impl_){auto& bi=impl_->physics.GetBodyInterface();for(auto& [id,r]:impl_->records){bi.RemoveBody(r.id);bi.DestroyBody(r.id);}}}
std::vector<PhysicsBodyHandle> PhysicsWorld::CreateBatch(std::span<const PhysicsBodyDesc> descriptions){
 Thread::RequireMain();auto& p=*impl_;Require(descriptions.size()<=p.config.maxBodies-p.records.size(),"physics body capacity exceeded");
 for(const auto& d:descriptions)ValidatePhysicsBody(d);
 std::vector<PhysicsBodyHandle> result;result.reserve(descriptions.size());JPH::BodyIDVector ids;ids.reserve(descriptions.size());
 auto& bi=p.physics.GetBodyInterface();
 try{
  for(const auto& d:descriptions){
   auto shape=Shape(d);const auto& c=d.component;JPH::BodyCreationSettings settings(shape,V(d.transform.position),Rotation(d.transform.rotationDegrees),Motion(c.body.motion),static_cast<JPH::ObjectLayer>(c.body.layer));
   settings.mFriction=c.collider.material.friction;settings.mRestitution=c.collider.material.restitution;settings.mGravityFactor=c.body.gravityScale;settings.mAllowSleeping=c.body.allowSleep;
   settings.mLinearDamping=c.body.linearDamping;settings.mAngularDamping=c.body.angularDamping;settings.mMotionQuality=c.body.continuous?JPH::EMotionQuality::LinearCast:JPH::EMotionQuality::Discrete;
   settings.mOverrideMassProperties=JPH::EOverrideMassProperties::CalculateInertia;settings.mMassPropertiesOverride.mMass=c.body.mass;
   const auto handle=nextHandle.fetch_add(1);settings.mUserData=handle;
   auto* body=bi.CreateBody(settings);if(!body)throw std::runtime_error("Jolt body pool exhausted");
   try{auto it=p.records.emplace(handle,Impl::Record{body->GetID(),d,V(d.transform.position),V(d.transform.position),Rotation(d.transform.rotationDegrees),Rotation(d.transform.rotationDegrees)}).first;
    p.slots[body->GetID().GetIndex()]={handle,&it->second};
   }catch(...){bi.DestroyBody(body->GetID());throw;}
   result.push_back({handle});ids.push_back(body->GetID());
  }
  if(!ids.empty()){auto state=bi.AddBodiesPrepare(ids.data(),int(ids.size()));bi.AddBodiesFinalize(ids.data(),int(ids.size()),state,JPH::EActivation::Activate);}
 }catch(...){for(auto h:result){auto& r=p.records.at(h.value);p.slots[r.id.GetIndex()]={};bi.DestroyBody(r.id);p.records.erase(h.value);}throw;}
 for(auto h:result){switch(p.records.at(h.value).desc.component.body.motion){case MotionType::Static:++p.stats.statics;break;case MotionType::Dynamic:++p.stats.dynamics;break;case MotionType::Kinematic:++p.stats.kinematics;break;}}
 return result;
}
PhysicsBodyHandle PhysicsWorld::CreateBody(const PhysicsBodyDesc& d){return CreateBatch(std::span(&d,1)).front();}
void PhysicsWorld::DestroyBody(PhysicsBodyHandle h){auto& p=*impl_;auto& r=p.Get(h);auto& bi=p.physics.GetBodyInterface();bi.RemoveBody(r.id);bi.DestroyBody(r.id);p.slots[r.id.GetIndex()]={};switch(r.desc.component.body.motion){case MotionType::Static:--p.stats.statics;break;case MotionType::Dynamic:--p.stats.dynamics;break;case MotionType::Kinematic:--p.stats.kinematics;break;}p.records.erase(h.value);std::erase_if(p.changed,[&](auto& c){return c.body==h;});}
PhysicsBodyState PhysicsWorld::Read(PhysicsBodyHandle h)const{auto& r=impl_->Get(h);auto& bi=impl_->physics.GetBodyInterface();JPH::RVec3 pos;JPH::Quat rot;bi.GetPositionAndRotation(r.id,pos,rot);return {{V(JPH::Vec3(pos)),Euler(rot),r.desc.transform.scale},V(bi.GetLinearVelocity(r.id)),r.desc.component.body.motion,bi.IsActive(r.id)};}
void PhysicsWorld::SetVelocity(PhysicsBodyHandle h,Vec3 v){auto& r=impl_->Get(h);Require(r.desc.component.body.motion!=MotionType::Static,"static body velocity");impl_->physics.GetBodyInterface().SetLinearVelocity(r.id,V(v));impl_->physics.GetBodyInterface().ActivateBody(r.id);}
void PhysicsWorld::ApplyImpulse(PhysicsBodyHandle h,Vec3 v){auto& r=impl_->Get(h);Require(r.desc.component.body.motion==MotionType::Dynamic,"impulse requires dynamic body");impl_->physics.GetBodyInterface().AddImpulse(r.id,V(v));}
void PhysicsWorld::ApplyForce(PhysicsBodyHandle h,Vec3 v){auto& r=impl_->Get(h);Require(r.desc.component.body.motion==MotionType::Dynamic,"force requires dynamic body");impl_->physics.GetBodyInterface().AddForce(r.id,V(v));}
void PhysicsWorld::Wake(PhysicsBodyHandle h){auto& r=impl_->Get(h);Require(r.desc.component.body.motion!=MotionType::Static,"cannot activate static body");impl_->physics.GetBodyInterface().ActivateBody(r.id);}
void PhysicsWorld::Teleport(PhysicsBodyHandle h,const Transform& t){auto& p=*impl_;auto& r=p.Get(h);p.CheckPose(r,t);p.teleports.push_back(h);p.physics.GetBodyInterface().SetPositionAndRotation(r.id,V(t.position),Rotation(t.rotationDegrees),r.desc.component.body.motion==MotionType::Static?JPH::EActivation::DontActivate:JPH::EActivation::Activate);r.previous=r.current=V(t.position);r.prevRotation=r.rotation=Rotation(t.rotationDegrees);}
void PhysicsWorld::MoveKinematic(PhysicsBodyHandle h,const Transform& t){auto& r=impl_->Get(h);Require(r.desc.component.body.motion==MotionType::Kinematic,"kinematic target requires kinematic body");impl_->CheckPose(r,t);impl_->physics.GetBodyInterface().MoveKinematic(r.id,V(t.position),Rotation(t.rotationDegrees),float(StepSeconds()));}
void PhysicsWorld::Step(){
 Thread::RequireMain();auto& p=*impl_;p.changed.clear();p.before.clear();p.after.clear();++p.epoch;
 p.physics.GetActiveBodies(JPH::EBodyType::RigidBody,p.before);const auto start=Clock::now();
 const auto error=p.physics.Update(float(StepSeconds()),1,&p.allocator,&p.jobs);p.stats.updateMs=Ms(start);
 if(error!=JPH::EPhysicsUpdateError::None)throw std::runtime_error("Jolt update capacity error: "+std::to_string(static_cast<unsigned>(error)));
 p.physics.GetActiveBodies(JPH::EBodyType::RigidBody,p.after);
 for(auto id:p.before)p.Publish(id);
 for(auto id:p.after)p.Publish(id);
 for(auto h:p.teleports){auto it=p.records.find(h.value);if(it!=p.records.end())p.Publish(it->second.id);}p.teleports.clear();p.stats.publishedPoses=p.changed.size();
}
std::span<const PhysicsPoseUpdate> PhysicsWorld::ChangedBodies()const{Thread::RequireMain();return impl_->changed;}
Transform PhysicsWorld::Interpolated(PhysicsBodyHandle h,double alpha)const{Require(std::isfinite(alpha)&&alpha>=0&&alpha<=1,"invalid interpolation alpha");const auto& r=impl_->Get(h);auto pos=r.previous+(r.current-r.previous)*float(alpha);return {V(pos),Euler(r.prevRotation.SLERP(r.rotation,float(alpha))),r.desc.transform.scale};}
std::optional<RayHit> PhysicsWorld::Raycast(Vec3 origin,Vec3 displacement,CollisionLayer layer){
 Thread::RequireMain();auto& p=*impl_;(void)V(origin);auto direction=V(displacement);Require(direction.LengthSq()>0&&unsigned(layer)<CollisionMatrix::Count,"invalid ray");
 const auto start=Clock::now();++p.stats.raycasts;
 struct Filter final:JPH::ObjectLayerFilter {const CollisionMatrix& matrix;unsigned layer;Filter(const CollisionMatrix& m,unsigned l):matrix(m),layer(l){}bool ShouldCollide(JPH::ObjectLayer l)const override{return matrix.pairs[layer][l];}} filter(p.config.collisions,unsigned(layer));
 JPH::RRayCast ray(V(origin),direction);JPH::RayCastResult hit;
 const bool found=p.physics.GetNarrowPhaseQuery().CastRay(ray,hit,{},filter);p.stats.queriesMs+=Ms(start);if(!found)return {};
 JPH::BodyLockRead lock(p.physics.GetBodyLockInterface(),hit.mBodyID);if(!lock.Succeeded())return {};
 const auto point=ray.GetPointOnRay(hit.mFraction);return RayHit{{lock.GetBody().GetUserData()},V(JPH::Vec3(point)),V(lock.GetBody().GetWorldSpaceSurfaceNormal(hit.mSubShapeID2,point)),hit.mFraction};
}
PhysicsStats PhysicsWorld::Statistics()const{Thread::RequireMain();auto s=impl_->stats;s.bodies=unsigned(impl_->records.size());s.active=impl_->physics.GetNumActiveBodies(JPH::EBodyType::RigidBody);unsigned dynamicActive=0;const auto* ids=impl_->physics.GetActiveBodiesUnsafe(JPH::EBodyType::RigidBody);for(unsigned i=0;i<s.active;++i)dynamicActive+=impl_->slots[ids[i].GetIndex()].second->desc.component.body.motion==MotionType::Dynamic;s.sleeping=s.dynamics-dynamicActive;return s;}
double PhysicsWorld::StepSeconds()const{Thread::RequireMain();return 1/impl_->config.fixedHz;}
}
