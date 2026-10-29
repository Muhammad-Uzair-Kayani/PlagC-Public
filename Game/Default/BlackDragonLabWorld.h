#pragma once

#include "../Framework/GameWorld.h"
#include <PlagC/Assets/AssetScene.h>
#include <PlagC/World/SceneRenderer.h>
#include <memory>

namespace PlagCGame
{
class BlackDragonLabWorld final : public IGameWorld
{
  public:
    std::string_view Name() const override { return "BlackDragonLab"; }
    void Start(PlagC::Engine &engine) override;
    void Frame(PlagC::Engine &engine, double dt) override;
    void Stop(PlagC::Engine &engine) override;

  private:
    std::unique_ptr<PlagC::World> world_;
    std::unique_ptr<PlagC::AssetScene> assetScene_;
    std::unique_ptr<PlagC::SceneRenderer> sceneRenderer_;
    PlagC::AssetHandle<PlagC::ModelAsset> characterAsset_;

    PlagC::Entity character_;
    PlagC::Entity camera_;
    PlagC::Entity sun_;
    PlagC::Entity fill_;
    PlagC::Entity ground_;

    PlagC::MeshHandle groundMesh_;
    PlagC::MaterialHandle groundMaterial_;

    PlagC::EventBus::Connection key_;
    PlagC::EventBus::Connection focus_;
    PlagC::EventBus::Connection mouse_;

    PlagC::Vec3 position_{0.0f, 0.0f, 0.0f};
    float characterYaw_ = 0.0f;
    float cameraYaw_ = 0.0f;
    float cameraPitch_ = 8.0f;
    float locomotionSpeed_ = 0.0f;
    float aimWeight_ = 0.0f;
    double upperBodyTime_ = 0.0;
    bool firing_ = false;
    bool previousFire_ = false;
    bool cursorCaptured_ = true;
};
} // namespace PlagCGame
