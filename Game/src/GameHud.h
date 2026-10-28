#pragma once
#include <PlagC/Renderer/Renderer.h>
#include <cstddef>
#include <string>
class GameHud
{
  public:
    void Update(PlagC::Renderer &, const std::string &);
    void Place(PlagC::Renderer &, PlagC::Vec3, float yaw, float pitch, float aspect, float fov);
    void Clear(PlagC::Renderer &);

  private:
    PlagC::MeshHandle mesh_;
    PlagC::MaterialHandle material_;
    PlagC::ObjectHandle object_;
    PlagC::MeshHandle crossMesh_;
    PlagC::ObjectHandle cross_;
    std::string text_;
    std::size_t capacityQuads_ = 0;
};
