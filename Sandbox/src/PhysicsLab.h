#pragma once
#include <PlagC/Application.h>
#include <PlagC/Physics/PhysicsSystem.h>
#include <PlagC/World/SceneRenderer.h>
void TestPhysicsFoundation();
class PhysicsLab {
public:
    void Start(PlagC::Engine&,unsigned physicsWorkers,unsigned count);
    void Frame(PlagC::Engine&,double dt);
    void Stop(PlagC::Engine&);
private:
    PlagC::World world_;
    std::unique_ptr<PlagC::PhysicsSystem> physics_;
    std::unique_ptr<PlagC::SceneRenderer> scene_;
    PlagC::MeshHandle mesh_;
    PlagC::MaterialHandle material_,floorMaterial_;
    PlagC::EventBus::Connection keys_;
    std::vector<PlagC::Entity> bodies_;
    bool paused_=false,singleStep_=false;
    double elapsed_=0;
};
