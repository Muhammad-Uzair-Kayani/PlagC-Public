#pragma once
#include <PlagC/Application.h>
#include <PlagC/World/SceneRenderer.h>
#include <PlagC/Input/Input.h>
#include <PlagC/World/TransformSystem.h>

// Owned snapshots only; workers never access World, Input or Renderer.
unsigned AnimateIntegration(PlagC::JobSystem&, std::vector<PlagC::TransformUpdate>&,
                            float dt, float time, unsigned iterations);
unsigned AnimateTransforms(PlagC::JobSystem&, std::span<PlagC::Transform>, float dt, float time, unsigned iterations);
void TestTransformSystem();
void TestSceneSynchronization(PlagC::Renderer&);
void TestInputAndPacing();
void TestIntegrationWork(PlagC::JobSystem&);
class IntegrationDemo {
public:
    void Start(PlagC::Engine&);
    void Frame(PlagC::Engine&, double);
    void Validate(PlagC::Engine&);
    void Stop(PlagC::Engine&);
private:
    void SetCount(std::size_t);
    void Key(PlagC::Engine&, PlagC::KeyCode);
    void Camera(double);
    PlagC::World world_;
    std::unique_ptr<PlagC::SceneRenderer> scene_;
    std::vector<PlagC::Entity> cubes_;
    PlagC::TransformSystem transforms_;
    PlagC::Entity camera_, lights_[4];
    PlagC::MeshHandle cubeMesh_, groundMesh_;
    PlagC::MaterialHandle cubeMaterial_, groundMaterial_;
    PlagC::EventBus::Connection keys_;
    bool grow_ = true, looking_ = false, profileSync_ = false;
    unsigned iterations_ = 1;
    double elapsed_ = 0, growthTime_ = 0, statsTime_ = 0;
    unsigned statsFrames_ = 0;
    double snapshotMs_ = 0, jobsMs_ = 0, commitMs_ = 0, syncMs_ = 0, renderMs_ = 0;
    PlagC::Vec3 position_{0, 22, 48};
    float yaw_ = -90, pitch_ = -22;
};
