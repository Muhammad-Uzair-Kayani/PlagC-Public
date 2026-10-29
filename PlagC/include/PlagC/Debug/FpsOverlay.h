#pragma once

#include <PlagC/Renderer/Renderer.h>
#include <cstddef>
#include <string>

namespace PlagC
{
// Small renderer-agnostic diagnostic readout implemented only with the public
// renderer API. It therefore behaves identically across supported backends.
class FpsOverlay
{
  public:
    void Frame(Renderer &renderer, const CameraDesc &camera, double dt, float aspect);
    void Clear(Renderer &renderer);

  private:
    void UpdateText(Renderer &renderer, const std::string &text);
    void Place(Renderer &renderer, const CameraDesc &camera, float aspect);

    MeshHandle mesh_{};
    MaterialHandle material_{};
    ObjectHandle object_{};
    std::size_t capacityQuads_ = 0;
    std::string text_;
    double sampleSeconds_ = 0.0;
    unsigned sampleFrames_ = 0;
};
} // namespace PlagC
