#pragma once
#include <PlagC/Renderer/Renderer.h>
class PhysicsHud {
public:
    void Update(PlagC::Renderer&,const std::string&);
    void Place(PlagC::Renderer&,PlagC::Vec3,float yaw,float pitch,float aspect);
    void Clear(PlagC::Renderer&);
private:
    PlagC::MeshHandle mesh_;
    PlagC::MaterialHandle material_;
    PlagC::ObjectHandle object_;
};
