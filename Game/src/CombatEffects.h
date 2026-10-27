#pragma once
#include <PlagC/Renderer/Renderer.h>
#include <array>
namespace Game
{
class CombatEffects
{
    PlagC::Renderer &renderer_;
    PlagC::MeshHandle mesh_;
    PlagC::MaterialHandle sparks_, dust_;
    PlagC::LightHandle muzzle_;
    float muzzleTime_ = 0;
    struct Particle
    {
        PlagC::ObjectHandle object;
        PlagC::Vec3 position, velocity;
        float life = 0, initial = 0, size = 0;
        bool spark = false;
    };
    std::array<Particle, 64> particles_;
    unsigned next_ = 0;

  public:
    explicit CombatEffects(PlagC::Renderer &);
    ~CombatEffects();
    void Muzzle(PlagC::Vec3);
    void Impact(PlagC::Vec3, PlagC::Vec3 normal, bool metal);
    void Update(double dt);
};
} // namespace Game
