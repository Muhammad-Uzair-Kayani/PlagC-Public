#pragma once
#include <PlagC/Renderer/Renderer.h>
#include <vector>
class RenderDemo
{
  public:
    void Start(PlagC::Renderer &, bool native);
    void Frame(PlagC::Renderer &, double);
    void Stop(PlagC::Renderer &);
    void ToggleWireframe(PlagC::Renderer &);
    void ToggleShadows(PlagC::Renderer &);

  private:
    std::vector<PlagC::MeshHandle> meshes_;
    std::vector<PlagC::MaterialHandle> materials_;
    std::vector<PlagC::ObjectHandle> objects_;
    std::vector<PlagC::LightHandle> lights_;
    PlagC::TargetHandle target_;
    PlagC::RenderSettings settings_;
    bool native_ = false;
    unsigned frame_ = 0;
    double time_ = 0;
};
