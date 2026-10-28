#pragma once

#include <PlagC/Renderer/Renderer.h>
#include <cstddef>
#include <string>

namespace PlagCGame
{
// Small renderer-agnostic FPS readout. It is implemented with the public PlagC
// renderer API, so OpenGL and Vulkan render exactly the same overlay geometry.
class FpsOverlay
{
  public:
    void Frame(PlagC::Renderer &renderer, const PlagC::CameraDesc &camera, double dt,
               float aspect);
    void Clear(PlagC::Renderer &renderer);

  private:
    void UpdateText(PlagC::Renderer &renderer, const std::string &text);
    void Place(PlagC::Renderer &renderer, const PlagC::CameraDesc &camera, float aspect);

    PlagC::MeshHandle mesh_{};
    PlagC::MaterialHandle material_{};
    PlagC::ObjectHandle object_{};
    std::size_t capacityQuads_ = 0;
    std::string text_;
    double sampleSeconds_ = 0.0;
    unsigned sampleFrames_ = 0;
};
} // namespace PlagCGame
