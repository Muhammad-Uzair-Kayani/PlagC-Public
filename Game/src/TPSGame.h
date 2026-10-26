#pragma once
#include <PlagC/Application.h>
#include <PlagC/Assets/AssetScene.h>
#include <PlagC/World/SceneRenderer.h>
#include <PlagC/Camera/ThirdPersonCameraController.h>
#include "Combat.h"
#include "GameHud.h"
#include <unordered_map>
namespace Game
{
struct Controls
{
    float forward = 0, right = 0, mouseX = 0, mouseY = 0;
    bool sprint = false, aim = false, fire = false, jump = false;
};
class TPSGame
{
  public:
    void Start(PlagC::Engine &, const PlagC::EngineConfig &);
    void Frame(PlagC::Engine &, double);
    void Tick(PlagC::Engine &, double, const Controls &);
    void Stop(PlagC::Engine &);
    void Reload();
    void Damage(float n) { health_.Damage(n); }
    PlagC::CharacterState Player() const { return physics_->ReadCharacter(character_); }
    const Weapon &Rifle() const { return weapon_; }
    float HitPoints() const { return health_.value; }
    unsigned Hits() const { return hits_; }
    const PlagC::CameraDesc &View() const { return view_; }

  private:
    PlagC::EngineConfig config_;
    std::unique_ptr<PlagC::World> world_;
    std::unique_ptr<PlagC::PhysicsSystem> physics_;
    std::unique_ptr<PlagC::SceneRenderer> scene_;
    std::unique_ptr<PlagC::AssetScene> assets_;
    PlagC::AssetHandle<PlagC::ModelAsset> operative_, crate_, generator_;
    PlagC::CharacterHandle character_;
    PlagC::Entity actor_, door_;
    PlagC::ThirdPersonCameraController camera_;
    PlagC::CameraDesc view_;
    PlagC::MeshHandle cube_;
    std::vector<PlagC::MaterialHandle> materials_;
    std::vector<PlagC::TextureHandle> textures_;
    std::vector<PlagC::AudioSource> sounds_;
    std::unordered_map<std::uint64_t, Health> targets_;
    PlagC::EventBus::Connection key_, focus_, mouse_;
    GameHud hud_;
    Health health_;
    Weapon weapon_;
    unsigned hits_ = 0;
    bool captured_ = false, jump_ = false, restart_ = false, doorOpen_ = false, stats_ = false;
    float heading_ = 180, doorSlide_ = 0, fallPeak_ = 0;
    double deadTime_ = 0, shotTime_ = 0, hudTime_ = 0, footTime_ = 0;
    std::string animation_ = "Idle";
    PlagC::ObjectHandle flash_;
    PlagC::LightHandle muzzleLight_;
    void Capture(PlagC::Engine &, bool);
    void BuildFacility(PlagC::Engine &);
    PlagC::Entity Box(const std::string &, PlagC::Vec3, PlagC::Vec3, unsigned,
                      bool collision = true);
    void Prop(const std::string &, PlagC::Vec3, PlagC::AssetHandle<PlagC::ModelAsset>, PlagC::Vec3);
    void Shoot(PlagC::Engine &);
    void Interact(PlagC::Engine &);
    void Sound(PlagC::Engine &, unsigned, PlagC::Vec3);
};
} // namespace Game
