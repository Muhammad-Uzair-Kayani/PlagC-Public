#pragma once
#include <PlagC/World/SceneRenderer.h>
#include <PlagC/Jobs.h>
void TestWorld(PlagC::Renderer &, PlagC::JobSystem &);
class WorldDemo
{
  public:
    void Start(PlagC::Renderer &, bool capture);
    void Frame(PlagC::Renderer &, PlagC::JobSystem &, double);
    void Stop(PlagC::Renderer &);
  private:
    PlagC::World world_;
    std::unique_ptr<PlagC::SceneRenderer> scene_;
    std::vector<PlagC::MeshHandle> meshes_;
    std::vector<PlagC::MaterialHandle> materials_;
    std::vector<PlagC::TransformUpdate> animated_;
    PlagC::TargetHandle target_;
    bool capture_ = false;
    unsigned frames_ = 0;
};
