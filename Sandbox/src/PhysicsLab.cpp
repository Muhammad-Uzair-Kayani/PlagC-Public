#include "pgcpch.h"
#include "PhysicsLab.h"
using namespace PlagC;
void PhysicsLab::Start(Engine& engine,unsigned workers,unsigned count){
    auto& r=engine.Graphics();engine.SetFPSLocked(false);
    mesh_=r.CreateMesh(MakePrimitive(Primitive::Cube));
    MaterialDesc m;m.colour={.22f,.55f,.9f};material_=r.CreateMaterial(m);m.colour={.4f,.4f,.4f};floorMaterial_=r.CreateMaterial(m);
    auto floor=world_.Create("Static floor");world_.SetTransform(floor,{{0,-.5f,0},{},{60,1,60}});world_.SetRenderable(floor,{mesh_,floorMaterial_});
    PhysicsComponent p;p.body.motion=MotionType::Static;p.body.layer=CollisionLayer::StaticWorld;world_.SetPhysics(floor,p);
    p={};
    // Separated columns settle into short stacks; no per-frame animation of sleeping bodies.
    for(unsigned i=0;i<count;++i){
        auto e=world_.Create("Physics box");float x=float(i%20)*1.4f-13.3f,z=float((i/20)%20)*1.4f-13.3f,y=1+float(i/400)*1.15f;
        world_.SetTransform(e,{{x,y,z},{},{1,1,1}});world_.SetRenderable(e,{mesh_,material_});world_.SetPhysics(e,p);bodies_.push_back(e);
    }
    auto sun=world_.Create("Sun");world_.SetLight(sun,{});
    auto camera=world_.Create("Physics camera");world_.SetTransform(camera,{{35,28,40},{},{1,1,1}});world_.SetCamera(camera,{{0,3,0},55,.1f,500});world_.SetActiveCamera(camera);
    PhysicsConfig config;config.workers=workers;config.maxBodies=std::max(16384u,count+1);physics_=std::make_unique<PhysicsSystem>(world_,config);physics_->Advance(0);
    scene_=std::make_unique<SceneRenderer>(r);scene_->Sync(world_);
    keys_=engine.Events().Subscribe(EventType::KeyPressed,[this,&engine](const Event& e){
        if(e.key==KeyCode::Escape)engine.Stop();
        if(e.key==KeyCode::Space)paused_=!paused_;
        if(e.key==KeyCode::N)singleStep_=true;
        if(e.key==KeyCode::R)for(std::size_t i=0;i<bodies_.size();++i){Transform t{{float(i%20)*1.4f-13.3f,1+float(i/400)*1.15f,float((i/20)%20)*1.4f-13.3f},{},{1,1,1}};physics_->Teleport(bodies_[i],t);physics_->SetVelocity(bodies_[i],{});}
        if(e.key==KeyCode::I&&!bodies_.empty())physics_->ApplyImpulse(bodies_.front(),{0,8,0});
    });
    if(auto* w=engine.MainWindow())w->SetTitle("PlagC Physics Lab | Space pause | N step | R reset | I impulse | Escape");
    std::cout<<"[PHYSICS] fixed=60 Hz unlocked render; bodies="<<count+1<<" Jolt workers="<<workers<<" PlagC workers="<<engine.Jobs().WorkerCount()<<'\n';
}
void PhysicsLab::Frame(Engine&,double dt){
    const double simulationDt=singleStep_?1./60:(paused_?0:dt);singleStep_=false;
    physics_->Advance(simulationDt);scene_->Sync(world_,physics_->Presentation());
    elapsed_+=dt;if(elapsed_>=1){elapsed_=0;const auto s=physics_->Statistics();
        std::cout<<"[PHYSICS] bodies="<<s.physics.bodies<<" active="<<s.physics.active<<" sleeping="<<s.physics.sleeping<<" steps="<<s.steps<<" dropped="<<s.droppedSeconds<<" ecsWrites="<<s.ecsWrites<<" stepMs="<<s.stepMs<<" inputMs="<<s.inputMs<<" outputMs="<<s.outputMs<<" presentationMs="<<s.presentationMs<<" syncMs="<<scene_->Statistics().totalMs<<'\n';
    }
}
void PhysicsLab::Stop(Engine& engine){keys_.Reset();scene_.reset();physics_.reset();world_.Clear();bodies_.clear();auto& r=engine.Graphics();r.DestroyMaterial(material_);r.DestroyMaterial(floorMaterial_);r.DestroyMesh(mesh_);}
