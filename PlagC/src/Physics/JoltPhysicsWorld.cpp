#include "pgcpch.h"
#include <PlagC/Physics/PhysicsWorld.h>
#include <PlagC/Thread.h>
#include "PhysicsShapes.h"
#include "JoltJobs.h"
#include "JoltContacts.h"
#include <Jolt/Physics/Collision/CollisionCollectorImpl.h>
#include <Jolt/Physics/Collision/ShapeCast.h>
#include <Jolt/Physics/Collision/TransformedShape.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Constraints/PointConstraint.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/FixedConstraint.h>
#include <Jolt/Physics/Body/BodyLockMulti.h>
#include <Jolt/Physics/Collision/GroupFilterTable.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <unordered_set>
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
JPH::EMotionType Motion(MotionType t){switch(t){case MotionType::Static:return JPH::EMotionType::Static;case MotionType::Kinematic:return JPH::EMotionType::Kinematic;default:return JPH::EMotionType::Dynamic;}}
}
struct PhysicsWorld::Impl {
 std::shared_ptr<Runtime> runtime;
 PhysicsConfig config;Broad broad;Pairs pairs;BroadFilter filter;
 JPH::TempAllocatorMalloc allocator;
 std::unique_ptr<JPH::JobSystem> jobs;
 JoltContacts contacts;
 JPH::Ref<JPH::GroupFilterTable> groupFilter=new JPH::GroupFilterTable(1);
 JPH::CharacterVsCharacterCollisionSimple characterCollision;
 JPH::PhysicsSystem physics;
 struct Record {JPH::BodyID id;PhysicsBodyDesc desc;JPH::Vec3 previous,current;JPH::Quat prevRotation,rotation;std::uint64_t stamp=0;};
 std::unordered_map<std::uint64_t,Record> records;
 std::vector<std::pair<std::uint64_t,Record*>> slots;
 std::vector<PhysicsPoseUpdate> changed;
 std::vector<PhysicsBodyHandle> teleports;
 JPH::BodyIDVector before,after;
 std::uint64_t epoch=0;
 PhysicsStats stats;
 struct ConstraintRecord {ConstraintDesc description;JPH::Ref<JPH::TwoBodyConstraint> constraint;};
 std::unordered_map<std::uint64_t,ConstraintRecord> constraints;
 struct CharacterRecord {CharacterDesc description;JPH::Ref<JPH::CharacterVirtual> character;Vec3 input{};bool jump=false;};
 std::unordered_map<std::uint64_t,CharacterRecord> characters;
 std::vector<TriggerEvent> triggerEvents;

 explicit Impl(const PhysicsConfig& c):runtime(Acquire()),config(c),pairs(c.collisions),filter(pairs) {
  if(c.scheduler==PhysicsScheduler::PlagC)jobs=std::make_unique<JoltJobs>(*c.sharedJobs);else jobs=std::make_unique<JPH::JobSystemThreadPool>(JPH::cMaxPhysicsJobs,JPH::cMaxPhysicsBarriers,int(c.workers));
  // Equal subgroup IDs already disable self-collision in GroupFilterTable.
  slots.resize(c.maxBodies);changed.reserve(c.maxBodies);teleports.reserve(c.maxBodies);before.reserve(c.maxBodies);after.reserve(c.maxBodies);
  physics.Init(c.maxBodies,0,c.maxBodyPairs,c.maxContacts,broad,filter,pairs);physics.SetGravity(V(c.gravity));physics.SetContactListener(&contacts);stats.workers=c.scheduler==PhysicsScheduler::PlagC?c.sharedJobs->WorkerCount():c.workers;
 }
 Record& Get(PhysicsBodyHandle h){Thread::RequireMain();auto it=records.find(h.value);if(it==records.end())throw std::invalid_argument("invalid stale or foreign physics handle");return it->second;}
 Transform Pose(const Record& r)const{return {V(r.current),Euler(r.rotation),r.desc.transform.scale};}
 void CheckPose(Record& r,const Transform& t){auto d=r.desc;d.transform=t;ValidatePhysicsBody(d);Require(t.scale.x==r.desc.transform.scale.x&&t.scale.y==r.desc.transform.scale.y&&t.scale.z==r.desc.transform.scale.z,"physics scale changes require collider rebuild");}
 void UpdateCharacters();
 void CollectTriggers();
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
 Require(c.scheduler==PhysicsScheduler::Native||(c.scheduler==PhysicsScheduler::PlagC&&c.sharedJobs),"PlagC physics scheduler requires a live JobSystem");
 impl_=std::make_unique<Impl>(c);
}
PhysicsWorld::~PhysicsWorld(){
 Thread::RequireMain();if(!impl_)return;
 auto& p=*impl_;if(auto* adapter=dynamic_cast<JoltJobs*>(p.jobs.get()))adapter->Drain();
 for(auto& [id,c]:p.characters)p.characterCollision.Remove(c.character);p.characters.clear();
 for(auto& [id,c]:p.constraints)p.physics.RemoveConstraint(c.constraint);p.constraints.clear();
 p.physics.SetContactListener(nullptr);
 auto& bi=p.physics.GetBodyInterface();for(auto& [id,r]:p.records){bi.RemoveBody(r.id);bi.DestroyBody(r.id);}
}
std::vector<PhysicsBodyHandle> PhysicsWorld::CreateBatch(std::span<const PhysicsBodyDesc> descriptions){
 Thread::RequireMain();auto& p=*impl_;Require(descriptions.size()<=p.config.maxBodies-p.records.size(),"physics body capacity exceeded");
 for(const auto& d:descriptions)ValidatePhysicsBody(d);
 std::vector<PhysicsBodyHandle> result;result.reserve(descriptions.size());JPH::BodyIDVector ids;ids.reserve(descriptions.size());
 auto& bi=p.physics.GetBodyInterface();
 try{
  for(const auto& d:descriptions){
   auto shape=MakePhysicsShape(d.component.collider,d.transform.scale);const auto& c=d.component;JPH::BodyCreationSettings settings(shape,V(d.transform.position),Rotation(d.transform.rotationDegrees),Motion(c.body.motion),static_cast<JPH::ObjectLayer>(c.body.layer));
   settings.mIsSensor=c.body.sensor;settings.mCollideKinematicVsNonDynamic=c.body.sensor||c.body.motion==MotionType::Kinematic;
   if(c.body.collisionGroup)settings.mCollisionGroup=JPH::CollisionGroup(p.groupFilter,c.body.collisionGroup,0);
   settings.mFriction=c.collider.material.friction;settings.mRestitution=c.collider.material.restitution;settings.mGravityFactor=c.body.gravityScale;settings.mAllowSleeping=c.body.allowSleep;
   settings.mLinearDamping=c.body.linearDamping;settings.mAngularDamping=c.body.angularDamping;settings.mMotionQuality=c.body.continuous?JPH::EMotionQuality::LinearCast:JPH::EMotionQuality::Discrete;
   settings.mOverrideMassProperties=JPH::EOverrideMassProperties::CalculateInertia;settings.mMassPropertiesOverride.mMass=c.body.mass;
   const auto handle=nextHandle.fetch_add(1);settings.mUserData=handle;
   auto* body=bi.CreateBody(settings);if(!body)throw std::runtime_error("Jolt body pool exhausted");
   try{
    auto it=p.records.emplace(handle,Impl::Record{body->GetID(),d,V(d.transform.position),V(d.transform.position),Rotation(d.transform.rotationDegrees),Rotation(d.transform.rotationDegrees)}).first;
    p.slots[body->GetID().GetIndex()]={handle,&it->second};
    p.contacts.RegisterBody(handle,c.body.layer,c.body.collisionMask);
   }catch(...){
    p.contacts.UnregisterBody(handle);
    auto record=p.records.find(handle);if(record!=p.records.end()){p.slots[record->second.id.GetIndex()]={};p.records.erase(record);}
    bi.DestroyBody(body->GetID());throw;
   }
   result.push_back({handle});ids.push_back(body->GetID());
  }
  if(!ids.empty()){auto state=bi.AddBodiesPrepare(ids.data(),int(ids.size()));bi.AddBodiesFinalize(ids.data(),int(ids.size()),state,JPH::EActivation::Activate);}
 }catch(...){for(auto h:result){auto& r=p.records.at(h.value);p.slots[r.id.GetIndex()]={};p.contacts.UnregisterBody(h.value);bi.DestroyBody(r.id);p.records.erase(h.value);}throw;}
 for(auto h:result){p.stats.sensors+=p.records.at(h.value).desc.component.body.sensor;switch(p.records.at(h.value).desc.component.body.motion){case MotionType::Static:++p.stats.statics;break;case MotionType::Dynamic:++p.stats.dynamics;break;case MotionType::Kinematic:++p.stats.kinematics;break;}}
 return result;
}
PhysicsBodyHandle PhysicsWorld::CreateBody(const PhysicsBodyDesc& d){return CreateBatch(std::span(&d,1)).front();}
void PhysicsWorld::DestroyBody(PhysicsBodyHandle h){DestroyBatch(std::span(&h,1));}
PhysicsBodyState PhysicsWorld::Read(PhysicsBodyHandle h)const{auto& r=impl_->Get(h);auto& bi=impl_->physics.GetBodyInterface();JPH::RVec3 pos;JPH::Quat rot;bi.GetPositionAndRotation(r.id,pos,rot);return {{V(JPH::Vec3(pos)),Euler(rot),r.desc.transform.scale},V(bi.GetLinearVelocity(r.id)),r.desc.component.body.motion,bi.IsActive(r.id),V(bi.GetAngularVelocity(r.id))};}
void PhysicsWorld::SetVelocity(PhysicsBodyHandle h,Vec3 v){auto& r=impl_->Get(h);Require(r.desc.component.body.motion!=MotionType::Static,"static body velocity");impl_->physics.GetBodyInterface().SetLinearVelocity(r.id,V(v));impl_->physics.GetBodyInterface().ActivateBody(r.id);}
void PhysicsWorld::ApplyImpulse(PhysicsBodyHandle h,Vec3 v){auto& r=impl_->Get(h);Require(r.desc.component.body.motion==MotionType::Dynamic,"impulse requires dynamic body");impl_->physics.GetBodyInterface().AddImpulse(r.id,V(v));}
void PhysicsWorld::ApplyForce(PhysicsBodyHandle h,Vec3 v){auto& r=impl_->Get(h);Require(r.desc.component.body.motion==MotionType::Dynamic,"force requires dynamic body");impl_->physics.GetBodyInterface().AddForce(r.id,V(v));}
void PhysicsWorld::Wake(PhysicsBodyHandle h){auto& r=impl_->Get(h);Require(r.desc.component.body.motion!=MotionType::Static,"cannot activate static body");impl_->physics.GetBodyInterface().ActivateBody(r.id);}
void PhysicsWorld::Teleport(PhysicsBodyHandle h,const Transform& t){auto& p=*impl_;auto& r=p.Get(h);p.CheckPose(r,t);p.teleports.push_back(h);p.physics.GetBodyInterface().SetPositionAndRotation(r.id,V(t.position),Rotation(t.rotationDegrees),r.desc.component.body.motion==MotionType::Static?JPH::EActivation::DontActivate:JPH::EActivation::Activate);r.previous=r.current=V(t.position);r.prevRotation=r.rotation=Rotation(t.rotationDegrees);}
void PhysicsWorld::MoveKinematic(PhysicsBodyHandle h,const Transform& t){auto& r=impl_->Get(h);Require(r.desc.component.body.motion==MotionType::Kinematic,"kinematic target requires kinematic body");impl_->CheckPose(r,t);impl_->physics.GetBodyInterface().MoveKinematic(r.id,V(t.position),Rotation(t.rotationDegrees),float(StepSeconds()));}
void PhysicsWorld::Step(){
 Thread::RequireMain();auto& p=*impl_;p.changed.clear();p.before.clear();p.after.clear();++p.epoch;
 p.UpdateCharacters();
 p.physics.GetActiveBodies(JPH::EBodyType::RigidBody,p.before);const auto start=Clock::now();
 const auto error=p.physics.Update(float(StepSeconds()),1,&p.allocator,p.jobs.get());
 if(auto* adapter=dynamic_cast<JoltJobs*>(p.jobs.get()))adapter->Drain();
 p.stats.updateMs=Ms(start);
 if(error!=JPH::EPhysicsUpdateError::None)throw std::runtime_error("Jolt update capacity error: "+std::to_string(static_cast<unsigned>(error)));
 p.physics.GetActiveBodies(JPH::EBodyType::RigidBody,p.after);
 for(auto id:p.before)p.Publish(id);
 for(auto id:p.after)p.Publish(id);
 for(auto h:p.teleports){auto it=p.records.find(h.value);if(it!=p.records.end())p.Publish(it->second.id);}p.teleports.clear();p.stats.publishedPoses=p.changed.size();p.CollectTriggers();
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
PhysicsStats PhysicsWorld::Statistics()const{Thread::RequireMain();auto s=impl_->stats;s.bodies=unsigned(impl_->records.size());s.active=impl_->physics.GetNumActiveBodies(JPH::EBodyType::RigidBody);unsigned dynamicActive=0;const auto* ids=impl_->physics.GetActiveBodiesUnsafe(JPH::EBodyType::RigidBody);for(unsigned i=0;i<s.active;++i)dynamicActive+=impl_->slots[ids[i].GetIndex()].second->desc.component.body.motion==MotionType::Dynamic;s.sleeping=s.dynamics-dynamicActive;s.contacts=impl_->contacts.contacts.size();s.constraints=impl_->constraints.size();s.characters=impl_->characters.size();
 if(impl_->config.sharedJobs){auto j=impl_->config.sharedJobs->Stats();s.jobsSubmitted=j.submitted;s.jobsExecuted=j.executed;}return s;}
double PhysicsWorld::StepSeconds()const{Thread::RequireMain();return 1/impl_->config.fixedHz;}

namespace {
struct QueryFilter final:JPH::ObjectLayerFilter {
 const CollisionMatrix& matrix;unsigned layer;
 QueryFilter(const CollisionMatrix& m,CollisionLayer l):matrix(m),layer(unsigned(l)){Require(layer<CollisionMatrix::Count,"invalid query layer");}
 bool ShouldCollide(JPH::ObjectLayer l)const override{return matrix.pairs[layer][l];}
};
JPH::ShapeRefC QueryShape(const ColliderComponent& collider,const Transform& t){
 PhysicsBodyDesc d;d.component.collider=collider;d.transform=t;ValidatePhysicsBody(d);return MakePhysicsShape(collider,t.scale);
}
}
std::optional<RayHit> PhysicsWorld::ShapeCast(const ColliderComponent& c,const Transform& t,Vec3 direction,CollisionLayer layer){
 Thread::RequireMain();auto& p=*impl_;auto shape=QueryShape(c,t);auto displacement=V(direction);Require(displacement.LengthSq()>0,"empty shape cast");QueryFilter filter(p.config.collisions,layer);
 auto start=Clock::now();++p.stats.shapeCasts;JPH::ClosestHitCollisionCollector<JPH::CastShapeCollector> collector;
 auto cast=JPH::RShapeCast::sFromWorldTransform(shape,JPH::Vec3::sReplicate(1),JPH::RMat44::sRotationTranslation(Rotation(t.rotationDegrees),V(t.position)),displacement);
 p.physics.GetNarrowPhaseQuery().CastShape(cast,{},JPH::RVec3::sZero(),collector,{},filter);p.stats.queriesMs+=Ms(start);
 if(!collector.HadHit())return {};const auto& hit=collector.mHit;JPH::BodyLockRead lock(p.physics.GetBodyLockInterface(),hit.mBodyID2);if(!lock.Succeeded())return {};
 return RayHit{{lock.GetBody().GetUserData()},V(hit.mContactPointOn2),V(-hit.mPenetrationAxis.NormalizedOr(JPH::Vec3::sAxisY())),hit.mFraction};
}
std::vector<PhysicsBodyHandle> PhysicsWorld::Overlap(const ColliderComponent& c,const Transform& t,CollisionLayer layer){
 Thread::RequireMain();auto& p=*impl_;auto shape=QueryShape(c,t);QueryFilter filter(p.config.collisions,layer);auto start=Clock::now();++p.stats.overlaps;
 JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;
 const auto transform=JPH::RMat44::sRotationTranslation(Rotation(t.rotationDegrees),V(t.position)).PreTranslated(shape->GetCenterOfMass());
 p.physics.GetNarrowPhaseQuery().CollideShape(shape,JPH::Vec3::sReplicate(1),transform,{},JPH::RVec3::sZero(),collector,{},filter);
 std::vector<PhysicsBodyHandle> result;
 for(const auto& hit:collector.mHits){JPH::BodyLockRead lock(p.physics.GetBodyLockInterface(),hit.mBodyID2);if(lock.Succeeded())result.push_back({lock.GetBody().GetUserData()});}
 std::sort(result.begin(),result.end(),[](auto a,auto b){return a.value<b.value;});result.erase(std::unique(result.begin(),result.end()),result.end());p.stats.queriesMs+=Ms(start);return result;
}
void PhysicsWorld::DestroyBatch(std::span<const PhysicsBodyHandle> bodies){
 Thread::RequireMain();auto& p=*impl_;std::unordered_set<std::uint64_t> seen;JPH::BodyIDVector ids;ids.reserve(bodies.size());
 for(auto h:bodies){auto& r=p.Get(h);Require(seen.insert(h.value).second,"duplicate body in removal batch");ids.push_back(r.id);}
 if(ids.empty())return;
 for(auto it=p.constraints.begin();it!=p.constraints.end();){const auto& d=it->second.description;if(seen.contains(d.a.value)||seen.contains(d.b.value)){p.physics.RemoveConstraint(it->second.constraint);it=p.constraints.erase(it);}else ++it;}
 auto& bi=p.physics.GetBodyInterface();bi.RemoveBodies(ids.data(),int(ids.size()));for(auto h:bodies)p.contacts.UnregisterBody(h.value);bi.DestroyBodies(ids.data(),int(ids.size()));
 for(auto h:bodies){const auto& r=p.records.at(h.value);p.slots[r.id.GetIndex()]={};p.stats.sensors-=r.desc.component.body.sensor;switch(r.desc.component.body.motion){case MotionType::Static:--p.stats.statics;break;case MotionType::Dynamic:--p.stats.dynamics;break;case MotionType::Kinematic:--p.stats.kinematics;break;}p.records.erase(h.value);}
 std::erase_if(p.changed,[&](const auto& c){return seen.contains(c.body.value);});std::erase_if(p.teleports,[&](auto h){return seen.contains(h.value);});
}
void PhysicsWorld::SetAngularVelocity(PhysicsBodyHandle h,Vec3 velocity){auto& r=impl_->Get(h);Require(r.desc.component.body.motion!=MotionType::Static,"static angular velocity");impl_->physics.GetBodyInterface().SetAngularVelocity(r.id,V(velocity));}
ConstraintHandle PhysicsWorld::CreateConstraint(const ConstraintDesc& d){
 Thread::RequireMain();auto& p=*impl_;auto& a=p.Get(d.a);auto& b=p.Get(d.b);Require(d.a!=d.b,"self constraint");
 Require(a.desc.component.body.motion==MotionType::Dynamic||b.desc.component.body.motion==MotionType::Dynamic,"constraint needs a dynamic body");
 Require(!a.desc.component.body.sensor&&!b.desc.component.body.sensor,"sensor constraint");
 (void)V(d.anchorA);(void)V(d.anchorB);auto axis=V(d.axis);Require(axis.LengthSq()>.0001f,"invalid hinge axis");axis=axis.Normalized();
 Require(std::isfinite(d.minDistance)&&std::isfinite(d.maxDistance)&&d.minDistance>=0&&d.maxDistance>=d.minDistance,"invalid distance limits");
 Require(std::isfinite(d.minAngleDegrees)&&std::isfinite(d.maxAngleDegrees)&&d.minAngleDegrees>=-180&&d.minAngleDegrees<=0&&d.maxAngleDegrees>=0&&d.maxAngleDegrees<=180,"invalid hinge limits");
 Require(std::isfinite(d.motorSpeed)&&std::isfinite(d.maxMotorTorque)&&d.maxMotorTorque>=0,"invalid motor");
 JPH::BodyID ids[]{a.id,b.id};JPH::BodyLockMultiWrite lock(p.physics.GetBodyLockInterface(),ids,2);auto* ba=lock.GetBody(0);auto* bb=lock.GetBody(1);Require(ba&&bb,"constraint bodies unavailable");
 JPH::Ref<JPH::TwoBodyConstraint> constraint;
 switch(d.type){
 case ConstraintType::Distance:{JPH::DistanceConstraintSettings s;s.mPoint1=V(d.anchorA);s.mPoint2=V(d.anchorB);s.mMinDistance=d.minDistance;s.mMaxDistance=d.maxDistance;constraint=s.Create(*ba,*bb);break;}
 case ConstraintType::Point:{JPH::PointConstraintSettings s;s.mPoint1=V(d.anchorA);s.mPoint2=V(d.anchorB);constraint=s.Create(*ba,*bb);break;}
 case ConstraintType::Fixed:{JPH::FixedConstraintSettings s;s.mAutoDetectPoint=true;constraint=s.Create(*ba,*bb);break;}
 case ConstraintType::Hinge:{JPH::HingeConstraintSettings s;s.mPoint1=V(d.anchorA);s.mPoint2=V(d.anchorB);s.mHingeAxis1=s.mHingeAxis2=axis;s.mNormalAxis1=s.mNormalAxis2=axis.GetNormalizedPerpendicular();constexpr float rad=3.14159265358979323846f/180;s.mLimitsMin=d.minAngleDegrees*rad;s.mLimitsMax=d.maxAngleDegrees*rad;s.mMotorSettings.mMinTorqueLimit=-d.maxMotorTorque;s.mMotorSettings.mMaxTorqueLimit=d.maxMotorTorque;auto* hinge=static_cast<JPH::HingeConstraint*>(s.Create(*ba,*bb));constraint=hinge;if(d.motor){hinge->SetMotorState(JPH::EMotorState::Velocity);hinge->SetTargetAngularVelocity(d.motorSpeed);}break;}
 default:throw std::invalid_argument("unknown constraint type");
 }
 const auto id=nextHandle.fetch_add(1);p.constraints.emplace(id,Impl::ConstraintRecord{d,constraint});p.physics.AddConstraint(constraint);return {id};
}
void PhysicsWorld::DestroyConstraint(ConstraintHandle h){Thread::RequireMain();auto& p=*impl_;auto it=p.constraints.find(h.value);Require(it!=p.constraints.end(),"stale constraint");p.physics.RemoveConstraint(it->second.constraint);p.constraints.erase(it);}
CharacterHandle PhysicsWorld::CreateCharacter(const CharacterDesc& d){
 Thread::RequireMain();auto& p=*impl_;(void)V(d.position);Require(std::isfinite(d.radius)&&d.radius>=.01f&&std::isfinite(d.halfHeight)&&d.halfHeight>=.01f&&std::isfinite(d.mass)&&d.mass>0&&std::isfinite(d.maxSlopeDegrees)&&d.maxSlopeDegrees>0&&d.maxSlopeDegrees<90&&std::isfinite(d.stepHeight)&&d.stepHeight>=0,"invalid character settings");
 JPH::CharacterVirtualSettings s;s.mShape=new JPH::CapsuleShape(d.halfHeight,d.radius);s.mMass=d.mass;s.mMaxSlopeAngle=d.maxSlopeDegrees*3.14159265358979323846f/180;
 auto id=nextHandle.fetch_add(1);JPH::Ref<JPH::CharacterVirtual> character=new JPH::CharacterVirtual(&s,V(d.position),JPH::Quat::sIdentity(),id,&p.physics);character->SetCharacterVsCharacterCollision(&p.characterCollision);
 p.characters.emplace(id,Impl::CharacterRecord{d,character});p.characterCollision.Add(character);return {id};
}
void PhysicsWorld::DestroyCharacter(CharacterHandle h){Thread::RequireMain();auto& p=*impl_;auto it=p.characters.find(h.value);Require(it!=p.characters.end(),"stale character");p.characterCollision.Remove(it->second.character);p.characters.erase(it);}
void PhysicsWorld::SetCharacterInput(CharacterHandle h,Vec3 input,bool jump){Thread::RequireMain();auto& p=*impl_;auto it=p.characters.find(h.value);Require(it!=p.characters.end(),"stale character");(void)V(input);Require(input.y==0,"character input must be horizontal");it->second.input=input;it->second.jump|=jump;}
CharacterState PhysicsWorld::ReadCharacter(CharacterHandle h)const{Thread::RequireMain();auto it=impl_->characters.find(h.value);Require(it!=impl_->characters.end(),"stale character");const auto& c=*it->second.character;return {V(JPH::Vec3(c.GetPosition())),V(c.GetLinearVelocity()),c.GetGroundState()==JPH::CharacterBase::EGroundState::OnGround};}
void PhysicsWorld::Impl::UpdateCharacters(){
 auto start=Clock::now();const float dt=float(1/config.fixedHz);const auto gravity=V(config.gravity);
 // Stable identity order makes crowd resolution repeatable despite hash iteration order.
 std::vector<std::uint64_t> order;for(auto& [id,c]:characters)order.push_back(id);std::sort(order.begin(),order.end());
 for(auto id:order){auto& record=characters.at(id);auto& c=*record.character;c.UpdateGroundVelocity();
  auto velocity=c.GetLinearVelocity();auto input=V(record.input);
  if(c.GetGroundState()==JPH::CharacterBase::EGroundState::OnGround&&velocity.GetY()<=c.GetGroundVelocity().GetY()+.1f){velocity=c.GetGroundVelocity()+input;if(record.jump)velocity+=JPH::Vec3(0,5,0);}
  else velocity=JPH::Vec3(input.GetX(),velocity.GetY(),input.GetZ());
  record.jump=false;c.SetLinearVelocity(velocity+gravity*dt);JPH::CharacterVirtual::ExtendedUpdateSettings settings;settings.mWalkStairsStepUp=JPH::Vec3(0,record.description.stepHeight,0);
  c.ExtendedUpdate(dt,gravity,settings,physics.GetDefaultBroadPhaseLayerFilter(unsigned(CollisionLayer::Character)),physics.GetDefaultLayerFilter(unsigned(CollisionLayer::Character)),{},{},allocator);
 }stats.charactersMs=Ms(start);
}
void PhysicsWorld::Impl::CollectTriggers(){
 auto start=Clock::now();triggerEvents.clear(); // Workers have completed; no ECS or user code ran inside callbacks.
 for(auto it=contacts.counts.begin();it!=contacts.counts.end();){
  const auto pair=it->first;auto sensor=records.find(pair.first),other=records.find(pair.second);bool inside=it->second>0&&sensor!=records.end()&&other!=records.end();
  // Jolt removes sleeping contacts. Verify these candidates before emitting Exit,
  // so a sleeping prop inside a zone does not generate a false exit event.
  if(!inside&&sensor!=records.end()&&other!=records.end()){
   auto& sr=sensor->second;auto shape=physics.GetBodyInterface().GetShape(sr.id);auto transform=physics.GetBodyInterface().GetCenterOfMassTransform(sr.id);
   struct OnlyBody:JPH::BodyFilter {JPH::BodyID id;explicit OnlyBody(JPH::BodyID i):id(i){}bool ShouldCollide(const JPH::BodyID& b)const override{return b==id;}} filter(other->second.id);
   JPH::AnyHitCollisionCollector<JPH::CollideShapeCollector> collector;
   physics.GetNarrowPhaseQuery().CollideShape(shape,JPH::Vec3::sReplicate(1),transform,{},JPH::RVec3::sZero(),collector,{},{},filter);inside=collector.HadHit();
  }
  const bool was=contacts.occupied.contains(pair);
  if(inside){triggerEvents.push_back({{pair.first},{pair.second},was?TriggerPhase::Stay:TriggerPhase::Enter});contacts.occupied[pair]=true;++it;}
  else {if(was)triggerEvents.push_back({{pair.first},{pair.second},TriggerPhase::Exit});contacts.occupied.erase(pair);it=contacts.counts.erase(it);}
 }
 stats.triggerEvents=triggerEvents.size();stats.eventsMs=Ms(start);
}
std::span<const TriggerEvent> PhysicsWorld::TriggerEvents()const{Thread::RequireMain();return impl_->triggerEvents;}

MeshData PhysicsWorld::BuildColliderMesh(const ColliderComponent& c)const{
 Thread::RequireMain();PhysicsBodyDesc d;d.component.collider=c;d.component.body.motion=MotionType::Static;d.component.body.layer=CollisionLayer::StaticWorld;ValidatePhysicsBody(d);
 auto shape=MakePhysicsShape(c,{1,1,1});JPH::Shape::GetTrianglesContext context;
 JPH::AllHitCollisionCollector<JPH::TransformedShapeCollector> leaves;
 shape->TransformShape(JPH::Mat44::sTranslation(shape->GetCenterOfMass()),leaves);
 MeshData mesh;JPH::Float3 vertices[3*128+1];
 for(const auto& leaf:leaves.mHits){
 leaf.GetTrianglesStart(context,leaf.GetWorldSpaceBounds(),JPH::RVec3::sZero());
 while(auto count=leaf.GetTrianglesNext(context,128,vertices)){
  for(int i=0;i<count;++i){auto a=JPH::Vec3::sLoadFloat3Unsafe(vertices[3*i]);auto b=JPH::Vec3::sLoadFloat3Unsafe(vertices[3*i+1]);auto c2=JPH::Vec3::sLoadFloat3Unsafe(vertices[3*i+2]);auto n=(b-a).Cross(c2-a).NormalizedOr(JPH::Vec3::sAxisY());
   for(auto point:{a,b,c2}){mesh.indices.push_back(std::uint32_t(mesh.vertices.size()));mesh.vertices.push_back({V(point),V(n),0,0});}
  }
 }}return mesh;
}
}
