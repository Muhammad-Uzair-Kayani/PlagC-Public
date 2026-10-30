#include "pgcpch.h"
#include <PlagC/Physics/PhysicsSystem.h>
#include <PlagC/LatencyProfiler.h>
#include "PhysicsContactRegistry.h"
#include "../World/WorldInternal.h"
#include <map>

namespace PlagC {
namespace {
using Clock=std::chrono::steady_clock;
double Ms(Clock::time_point t){return std::chrono::duration<double,std::milli>(Clock::now()-t).count();}
using ContactKey=std::pair<std::uint64_t,std::uint64_t>;
}

struct PhysicsSystem::Impl {
    World::Impl* world;
    std::weak_ptr<WorldLifetime> lifetime;
    PhysicsWorld physics;
    FixedStepClock clock;

    struct Binding {
        Entity entity;
        entt::entity raw;
        PhysicsBodyHandle body;
        MotionType motion;
        std::uint64_t version,transform;
        Transform target;
        std::uint64_t stamp=0;
    };

    std::unordered_map<std::uint64_t,Binding> bindings;
    std::unordered_map<std::uint64_t,Binding*> byBody;
    std::vector<Binding*> inputs;
    std::vector<TransformUpdate> presentation;
    std::vector<PhysicsBodyHandle> candidates;
    std::uint64_t structure=0,frame=0;
    PhysicsFrameStats stats;
    std::vector<TriggerEvent> events;
    std::vector<CollisionEvent> collisionEvents;
    std::map<ContactKey,bool> collisionOccupied;

    Impl(World::Impl& w,const PhysicsConfig& c):world(&w),lifetime(w.lifetime),physics(c),clock(c.fixedHz,c.maxCatchUpSteps){}

    void Check()const{
        Thread::RequireMain();
        if(lifetime.expired())throw std::logic_error("physics world binding expired");
    }

    Binding& Get(Entity e){
        Check();
        auto it=bindings.find(e.value);
        if(it==bindings.end())throw std::invalid_argument("entity has no synchronized physics body; call Advance(0)");
        return it->second;
    }

    void Reconcile(){
        Check();
        auto& w=*world;
        if(structure==w.physicsStructure)return;
        const auto start=Clock::now();
        std::vector<PhysicsBodyHandle> removed;
        std::vector<std::uint64_t> removedEntities;
        for(auto& [id,b]:bindings)
            if(!w.registry.valid(b.raw)||!w.registry.all_of<PhysicsComponent>(b.raw)||w.registry.get<WorldChanges>(b.raw).physics!=b.version){
                removed.push_back(b.body);
                removedEntities.push_back(id);
            }
        physics.DestroyBatch(removed);
        for(auto id:removedEntities){
            byBody.erase(bindings.at(id).body.value);
            bindings.erase(id);
        }
        if(!removed.empty()){
            std::erase_if(collisionOccupied,[&](const auto& item){
                return std::any_of(removed.begin(),removed.end(),[&](PhysicsBodyHandle body){
                    return item.first.first==body.value||item.first.second==body.value;
                });
            });
        }

        std::vector<PhysicsBodyDesc> descriptions;
        std::vector<entt::entity> entities;
        for(auto raw:w.registry.view<PhysicsComponent,Transform,Entity>()){
            if(bindings.contains(w.registry.get<Entity>(raw).value))continue;
            descriptions.push_back({w.registry.get<PhysicsComponent>(raw),w.registry.get<Transform>(raw)});
            entities.push_back(raw);
        }
        auto bodies=physics.CreateBatch(descriptions);
        try{
            for(std::size_t i=0;i<bodies.size();++i){
                auto raw=entities[i];
                auto e=w.registry.get<Entity>(raw);
                auto& changes=w.registry.get<WorldChanges>(raw);
                auto it=bindings.emplace(e.value,Binding{e,raw,bodies[i],descriptions[i].component.body.motion,changes.physics,changes.transform,descriptions[i].transform}).first;
                byBody.emplace(bodies[i].value,&it->second);
            }
        }catch(...){
            for(std::size_t i=0;i<bodies.size();++i){
                byBody.erase(bodies[i].value);
                bindings.erase(w.registry.get<Entity>(entities[i]).value);
                physics.DestroyBody(bodies[i]);
            }
            throw;
        }
        inputs.clear();
        for(auto& [id,b]:bindings)if(b.motion!=MotionType::Dynamic)inputs.push_back(&b);
        std::erase_if(candidates,[&](auto h){return !byBody.contains(h.value);});
        structure=w.physicsStructure;
        stats.structureMs+=Ms(start);
    }

    void Inputs(){
        const auto start=Clock::now();
        auto& r=world->registry;
        for(auto* b:inputs){
            const auto version=r.get<WorldChanges>(b->raw).transform;
            if(version!=b->transform){
                b->target=r.get<Transform>(b->raw);
                if(b->motion==MotionType::Static)physics.Teleport(b->body,b->target);
                b->transform=version;
            }
            if(b->motion==MotionType::Kinematic)physics.MoveKinematic(b->body,b->target);
        }
        stats.inputMs+=Ms(start);
    }

    void Outputs(){
        const auto start=Clock::now();
        auto& r=world->registry;
        for(const auto& pose:physics.ChangedBodies()){
            auto& b=*byBody.at(pose.body.value);
            if(b.motion==MotionType::Static)continue;
            r.get<Transform>(b.raw)=pose.transform;
            world->localByEntity.at(b.entity.value)=pose.transform;
            b.transform=++r.get<WorldChanges>(b.raw).transform;
            ++stats.ecsWrites;
        }
        stats.outputMs+=Ms(start);
        LatencyProfiler::OnPhysicsOutput();
    }

    bool StillOverlapping(Binding& a,Binding& b){
        auto& r=world->registry;
        if(!r.valid(a.raw)||!r.valid(b.raw)||!r.all_of<PhysicsComponent,Transform>(a.raw)||!r.all_of<PhysicsComponent>(b.raw))return false;
        const auto& component=r.get<PhysicsComponent>(a.raw);
        if(component.body.sensor||r.get<PhysicsComponent>(b.raw).body.sensor)return false;
        const auto hits=physics.Overlap(component.collider,r.get<Transform>(a.raw),component.body.layer);
        return std::find(hits.begin(),hits.end(),b.body)!=hits.end();
    }

    void CollectCollisions(){
        const auto started=Clock::now();
        std::map<ContactKey,bool> next;
        for(const auto pair:PhysicsContactRegistry::Snapshot()){
            auto a=byBody.find(pair.a),b=byBody.find(pair.b);
            if(a==byBody.end()||b==byBody.end())continue;
            const ContactKey key{pair.a,pair.b};
            next[key]=true;
            const bool was=collisionOccupied.contains(key);
            collisionEvents.push_back({a->second->body,b->second->body,was?CollisionPhase::Stay:CollisionPhase::Enter});
        }
        for(const auto& [key,occupied]:collisionOccupied){
            if(next.contains(key))continue;
            auto a=byBody.find(key.first),b=byBody.find(key.second);
            if(a==byBody.end()||b==byBody.end())continue;
            if(StillOverlapping(*a->second,*b->second)){
                next[key]=true;
                collisionEvents.push_back({a->second->body,b->second->body,CollisionPhase::Stay});
            }else{
                collisionEvents.push_back({a->second->body,b->second->body,CollisionPhase::Exit});
            }
        }
        collisionOccupied=std::move(next);
        stats.collisionEvents=collisionEvents.size();
        stats.eventsMs+=Ms(started);
    }
};

PhysicsSystem::PhysicsSystem(World& w,const PhysicsConfig& c){
    Thread::RequireMain();
    if(w.impl_->physicsAttached)throw std::logic_error("World already has a physics system");
    impl_=std::make_unique<Impl>(*w.impl_,c);
    w.impl_->physicsAttached=true;
}

PhysicsSystem::~PhysicsSystem(){
    Thread::RequireMain();
    if(impl_&&!impl_->lifetime.expired())impl_->world->physicsAttached=false;
}

void PhysicsSystem::Synchronize(){impl_->Reconcile();}

void PhysicsSystem::Advance(double dt,const std::function<void(double)>& beforeFixedStep){
    auto& p=*impl_;
    p.Check();
    p.stats={};
    p.events.clear();
    p.collisionEvents.clear();
    p.Reconcile();
    const auto plan=p.clock.Advance(dt);
    LatencyProfiler::OnPhysicsPlan(plan.steps,plan.alpha,plan.droppedSeconds);
    p.stats.steps=plan.steps;
    p.stats.alpha=plan.alpha;
    p.stats.droppedSeconds=plan.droppedSeconds;
    ++p.frame;
    if(plan.steps)p.candidates.clear();
    for(unsigned i=0;i<plan.steps;++i){
        if(beforeFixedStep){
            beforeFixedStep(p.clock.StepSeconds());
            p.Reconcile();
        }
        p.Inputs();
        p.physics.Step();
        LatencyProfiler::OnPhysicsStep();
        auto tick=p.physics.Statistics();
        p.stats.stepMs+=tick.updateMs;
        p.stats.eventsMs+=tick.eventsMs;
        p.stats.charactersMs+=tick.charactersMs;
        p.Outputs();
        const auto events=p.physics.TriggerEvents();
        p.events.insert(p.events.end(),events.begin(),events.end());
        p.CollectCollisions();
        for(const auto& pose:p.physics.ChangedBodies()){
            auto& b=*p.byBody.at(pose.body.value);
            if(b.stamp!=p.frame){
                b.stamp=p.frame;
                p.candidates.push_back(pose.body);
            }
        }
    }
    const auto start=Clock::now();
    p.presentation.clear();
    p.presentation.reserve(p.candidates.size());
    for(auto h:p.candidates){
        auto it=p.byBody.find(h.value);
        if(it!=p.byBody.end()&&it->second->motion!=MotionType::Static)
            p.presentation.push_back({it->second->entity,p.physics.Interpolated(h,plan.alpha)});
    }
    p.stats.presentationMs=Ms(start);
    p.stats.physics=p.physics.Statistics();
}

void PhysicsSystem::Teleport(Entity e,const Transform& t){auto& p=*impl_;p.Reconcile();auto& b=p.Get(e);p.physics.Teleport(b.body,t);p.world->registry.get<Transform>(b.raw)=t;p.world->localByEntity.at(e.value)=t;b.transform=++p.world->registry.get<WorldChanges>(b.raw).transform;b.target=t;for(auto& u:p.presentation)if(u.entity==e)u.transform=t;}
void PhysicsSystem::ApplyImpulse(Entity e,Vec3 v){impl_->Reconcile();impl_->physics.ApplyImpulse(impl_->Get(e).body,v);}
void PhysicsSystem::ApplyForce(Entity e,Vec3 v){impl_->Reconcile();impl_->physics.ApplyForce(impl_->Get(e).body,v);}
void PhysicsSystem::SetVelocity(Entity e,Vec3 v){impl_->Reconcile();impl_->physics.SetVelocity(impl_->Get(e).body,v);}
std::optional<RayHit> PhysicsSystem::Raycast(Vec3 a,Vec3 b,CollisionLayer l){impl_->Reconcile();return impl_->physics.Raycast(a,b,l);}
Entity PhysicsSystem::EntityFor(PhysicsBodyHandle h)const{impl_->Check();auto it=impl_->byBody.find(h.value);if(it==impl_->byBody.end())return {};const auto& b=*it->second;return impl_->world->registry.valid(b.raw)&&impl_->world->registry.all_of<PhysicsComponent>(b.raw)?b.entity:Entity{};}
PhysicsBodyState PhysicsSystem::Read(Entity e)const{impl_->Reconcile();return impl_->physics.Read(impl_->Get(e).body);}
std::span<const TransformUpdate> PhysicsSystem::Presentation()const{impl_->Check();return impl_->presentation;}
PhysicsFrameStats PhysicsSystem::Statistics()const{impl_->Check();auto stats=impl_->stats;stats.physics=impl_->physics.Statistics();return stats;}
void PhysicsSystem::SetAngularVelocity(Entity e,Vec3 v){impl_->Reconcile();impl_->physics.SetAngularVelocity(impl_->Get(e).body,v);}
PhysicsBodyHandle PhysicsSystem::BodyFor(Entity e)const{impl_->Reconcile();return impl_->Get(e).body;}
std::optional<RayHit> PhysicsSystem::ShapeCast(const ColliderComponent& c,const Transform& t,Vec3 v,CollisionLayer l){impl_->Reconcile();return impl_->physics.ShapeCast(c,t,v,l);}
std::vector<PhysicsBodyHandle> PhysicsSystem::Overlap(const ColliderComponent& c,const Transform& t,CollisionLayer l){impl_->Reconcile();return impl_->physics.Overlap(c,t,l);}
ConstraintHandle PhysicsSystem::CreateConstraint(Entity a,Entity b,ConstraintDesc d){impl_->Reconcile();d.a=impl_->Get(a).body;d.b=impl_->Get(b).body;return impl_->physics.CreateConstraint(d);}
void PhysicsSystem::DestroyConstraint(ConstraintHandle h){impl_->Check();impl_->physics.DestroyConstraint(h);}
CharacterHandle PhysicsSystem::CreateCharacter(const CharacterDesc& d){impl_->Reconcile();return impl_->physics.CreateCharacter(d);}
void PhysicsSystem::DestroyCharacter(CharacterHandle h){impl_->Check();impl_->physics.DestroyCharacter(h);}
void PhysicsSystem::SetCharacterInput(CharacterHandle h,Vec3 v,bool jump){impl_->Check();LatencyProfiler::OnMovementIntent(v);LatencyProfiler::OnPhysicsInput(v);impl_->physics.SetCharacterInput(h,v,jump);}
CharacterState PhysicsSystem::ReadCharacter(CharacterHandle h)const{impl_->Check();return impl_->physics.ReadCharacter(h);}
std::span<const TriggerEvent> PhysicsSystem::TriggerEvents()const{impl_->Check();return impl_->events;}
std::span<const CollisionEvent> PhysicsSystem::CollisionEvents()const{impl_->Check();return impl_->collisionEvents;}
MeshData PhysicsSystem::BuildColliderMesh(const ColliderComponent& c)const{impl_->Check();return impl_->physics.BuildColliderMesh(c);}

}
