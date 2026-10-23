#include "pgcpch.h"
#include <PlagC/Physics/PhysicsSystem.h>
#include "../World/WorldInternal.h"
namespace PlagC {
namespace {
using Clock=std::chrono::steady_clock;
double Ms(Clock::time_point t){return std::chrono::duration<double,std::milli>(Clock::now()-t).count();}
}
struct PhysicsSystem::Impl {
    World::Impl* world;
    std::weak_ptr<WorldLifetime> lifetime;
    PhysicsWorld physics;
    FixedStepClock clock;
    struct Binding {Entity entity;entt::entity raw;PhysicsBodyHandle body;MotionType motion;std::uint64_t version,transform;Transform target;std::uint64_t stamp=0;};
    std::unordered_map<std::uint64_t,Binding> bindings;
    std::unordered_map<std::uint64_t,Binding*> byBody;
    std::vector<Binding*> inputs;
    std::vector<TransformUpdate> presentation;
    std::vector<PhysicsBodyHandle> candidates;
    std::uint64_t structure=0,frame=0;
    PhysicsFrameStats stats;
    Impl(World::Impl& w,const PhysicsConfig& c):world(&w),lifetime(w.lifetime),physics(c),clock(c.fixedHz,c.maxCatchUpSteps){}
    void Check()const{Thread::RequireMain();if(lifetime.expired())throw std::logic_error("physics world binding expired");}
    Binding& Get(Entity e){Check();auto it=bindings.find(e.value);if(it==bindings.end())throw std::invalid_argument("entity has no synchronized physics body; call Advance(0)");return it->second;}
    void Reconcile(){
        Check();auto& w=*world;if(structure==w.physicsStructure)return;
        const auto start=Clock::now();
        // Structural changes only: stable bindings avoid per-frame snapshots and names.
        for(auto it=bindings.begin();it!=bindings.end();){
            auto& b=it->second;
            if(!w.registry.valid(b.raw)||!w.registry.all_of<PhysicsComponent>(b.raw)||w.registry.get<WorldChanges>(b.raw).physics!=b.version){
                byBody.erase(b.body.value);physics.DestroyBody(b.body);it=bindings.erase(it);
            }else ++it;
        }
        std::vector<PhysicsBodyDesc> descriptions;std::vector<entt::entity> entities;
        for(auto raw:w.registry.view<PhysicsComponent,Transform,Entity>()){
            if(bindings.contains(w.registry.get<Entity>(raw).value))continue;
            descriptions.push_back({w.registry.get<PhysicsComponent>(raw),w.registry.get<Transform>(raw)});entities.push_back(raw);
        }
        auto bodies=physics.CreateBatch(descriptions);
        try {
            for(std::size_t i=0;i<bodies.size();++i){
                auto raw=entities[i];auto e=w.registry.get<Entity>(raw);auto& changes=w.registry.get<WorldChanges>(raw);
                auto it=bindings.emplace(e.value,Binding{e,raw,bodies[i],descriptions[i].component.body.motion,changes.physics,changes.transform,descriptions[i].transform}).first;
                byBody.emplace(bodies[i].value,&it->second);
            }
        }catch(...){
            for(std::size_t i=0;i<bodies.size();++i){byBody.erase(bodies[i].value);bindings.erase(w.registry.get<Entity>(entities[i]).value);physics.DestroyBody(bodies[i]);}throw;
        }
        inputs.clear();for(auto& [id,b]:bindings)if(b.motion!=MotionType::Dynamic)inputs.push_back(&b);
        std::erase_if(candidates,[&](auto h){return !byBody.contains(h.value);});
        structure=w.physicsStructure;stats.structureMs+=Ms(start);
    }
    void Inputs(){
        const auto start=Clock::now();auto& r=world->registry;
        for(auto* b:inputs){
            const auto version=r.get<WorldChanges>(b->raw).transform;
            if(version!=b->transform){b->target=r.get<Transform>(b->raw);if(b->motion==MotionType::Static)physics.Teleport(b->body,b->target);b->transform=version;}
            // Re-submit target at each fixed step to stop kinematics at their target.
            if(b->motion==MotionType::Kinematic)physics.MoveKinematic(b->body,b->target);
        }
        stats.inputMs+=Ms(start);
    }
    void Outputs(){
        const auto start=Clock::now();auto& r=world->registry;
        for(const auto& pose:physics.ChangedBodies()){
            auto& b=*byBody.at(pose.body.value);
            if(b.motion==MotionType::Static)continue;
            r.get<Transform>(b.raw)=pose.transform;b.transform=++r.get<WorldChanges>(b.raw).transform;++stats.ecsWrites;
        }
        stats.outputMs+=Ms(start);
    }
};
PhysicsSystem::PhysicsSystem(World& w,const PhysicsConfig& c){
    Thread::RequireMain();if(w.impl_->physicsAttached)throw std::logic_error("World already has a physics system");
    impl_=std::make_unique<Impl>(*w.impl_,c);w.impl_->physicsAttached=true;
}
PhysicsSystem::~PhysicsSystem(){Thread::RequireMain();if(impl_&&!impl_->lifetime.expired())impl_->world->physicsAttached=false;}
void PhysicsSystem::Advance(double dt){
    auto& p=*impl_;p.Check();p.stats={};p.Reconcile();const auto plan=p.clock.Advance(dt);
    p.stats.steps=plan.steps;p.stats.alpha=plan.alpha;p.stats.droppedSeconds=plan.droppedSeconds;
    // Retain final sleeping poses across multiple catch-up steps in this frame.
    ++p.frame;
    if(plan.steps)p.candidates.clear();
    for(unsigned i=0;i<plan.steps;++i){
        p.Inputs();p.physics.Step();p.stats.stepMs+=p.physics.Statistics().updateMs;p.Outputs();
        for(const auto& pose:p.physics.ChangedBodies()){auto& b=*p.byBody.at(pose.body.value);if(b.stamp!=p.frame){b.stamp=p.frame;p.candidates.push_back(pose.body);}}
    }
    const auto start=Clock::now();p.presentation.clear();p.presentation.reserve(p.candidates.size());
    for(auto h:p.candidates){auto it=p.byBody.find(h.value);if(it!=p.byBody.end()&&it->second->motion!=MotionType::Static)p.presentation.push_back({it->second->entity,p.physics.Interpolated(h,plan.alpha)});}
    p.stats.presentationMs=Ms(start);p.stats.physics=p.physics.Statistics();
}
void PhysicsSystem::Teleport(Entity e,const Transform& t){
    auto& p=*impl_;p.Reconcile();auto& b=p.Get(e);p.physics.Teleport(b.body,t);
    p.world->registry.get<Transform>(b.raw)=t;b.transform=++p.world->registry.get<WorldChanges>(b.raw).transform;b.target=t;
    for(auto& u:p.presentation)if(u.entity==e)u.transform=t;
}
void PhysicsSystem::ApplyImpulse(Entity e,Vec3 v){impl_->Reconcile();impl_->physics.ApplyImpulse(impl_->Get(e).body,v);}
void PhysicsSystem::ApplyForce(Entity e,Vec3 v){impl_->Reconcile();impl_->physics.ApplyForce(impl_->Get(e).body,v);}
void PhysicsSystem::SetVelocity(Entity e,Vec3 v){impl_->Reconcile();impl_->physics.SetVelocity(impl_->Get(e).body,v);}
std::optional<RayHit> PhysicsSystem::Raycast(Vec3 a,Vec3 b,CollisionLayer l){impl_->Reconcile();return impl_->physics.Raycast(a,b,l);}
Entity PhysicsSystem::EntityFor(PhysicsBodyHandle h)const{impl_->Check();auto it=impl_->byBody.find(h.value);if(it==impl_->byBody.end())return {};const auto& b=*it->second;return impl_->world->registry.valid(b.raw)&&impl_->world->registry.all_of<PhysicsComponent>(b.raw)?b.entity:Entity{};}
PhysicsBodyState PhysicsSystem::Read(Entity e)const{impl_->Reconcile();return impl_->physics.Read(impl_->Get(e).body);}
std::span<const TransformUpdate> PhysicsSystem::Presentation()const{impl_->Check();return impl_->presentation;}
PhysicsFrameStats PhysicsSystem::Statistics()const{impl_->Check();return impl_->stats;}
}
