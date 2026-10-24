#pragma once
#include <PlagC/Application.h>
#include <PlagC/World/SceneRenderer.h>
#include "PhysicsScenarios.h"
#include "PhysicsHud.h"
#include <fstream>
#include <unordered_map>
void TestPhysicsFoundation();
class PhysicsLab {
public:
    void Start(PlagC::Engine&,const PlagC::EngineConfig&);
    void Frame(PlagC::Engine&,double dt);
    void Stop(PlagC::Engine&);
private:
    void Load(PlagC::Engine&);
    void Unload(PlagC::Engine&);
    void Key(PlagC::Engine&,PlagC::KeyCode);
    void Camera(PlagC::Engine&,double);
    void Visuals(PlagC::Renderer&);
    void Hud(PlagC::Engine&,double);
    void Sample(PlagC::Engine&,double,double);
    void FinishBenchmark();
    PlagC::EngineConfig config_;
    PhysicsLabScenes::Options options_;
    PlagC::PhysicsConfig physicsConfig_;
    std::unique_ptr<PhysicsLabScenes::Scene> model_;
    std::unique_ptr<PlagC::SceneRenderer> scene_;
    PlagC::Entity camera_;
    std::vector<PlagC::MeshHandle> meshes_;
    std::vector<PlagC::MaterialHandle> materials_;
    struct CachedMesh {PlagC::ColliderComponent collider;PlagC::MeshHandle mesh;};
    std::vector<CachedMesh> cached_;
    std::unordered_map<std::uint64_t,PlagC::RenderableComponent> bound_;
    std::vector<PlagC::ObjectHandle> marks_;
    PlagC::MeshHandle markMesh_;
    PlagC::EventBus::Connection keys_;
    PhysicsHud hud_;
    bool paused_=false,singleStep_=false,help_=true,colours_=false,looking_=false,benchmark_=false;
    double hudTime_=0,visualTime_=0,loadMs_=0;
    unsigned frame_=0;
    PlagC::Vec3 position_{0,32,65},forward_{0,0,-1};float yaw_=-90,pitch_=-25;
    std::ofstream csv_;
    std::vector<double> frameTimes_,physicsTimes_,syncTimes_;
};
