#pragma once

#include "../Framework/GameWorld.h"
#include <PlagC/Assets/AssetScene.h>
#include <PlagC/World/SceneRenderer.h>
#include <memory>

namespace PlagCGame
{
class CityScaleLabWorld final : public IGameWorld
{
  public:
    std::string_view Name() const override { return "CityScaleLab"; }
    void Start(PlagC::Engine &engine) override;
    void Frame(PlagC::Engine &engine, double dt) override;
    void Stop(PlagC::Engine &engine) override;

  private:
    void ApplyCityTransform();
    void ResetCamera();

    std::unique_ptr<PlagC::World> world_;
    std::unique_ptr<PlagC::AssetScene> assetScene_;
    std::unique_ptr<PlagC::SceneRenderer> sceneRenderer_;
    PlagC::AssetHandle<PlagC::ModelAsset> cityAsset_;
    PlagC::AssetHandle<PlagC::ModelAsset> characterAsset_;

    PlagC::Entity city_;
    PlagC::Entity referenceCharacter_;
    PlagC::Entity camera_;
    PlagC::Entity sun_;
    PlagC::Entity fill_;

    PlagC::EventBus::Connection key_;
    PlagC::EventBus::Connection focus_;
    PlagC::EventBus::Connection mouse_;

    // Baked City.plasset is already in metres at the calibrated 10x source conversion.
    float cityScale_ = 1.0f;
    float cityYOffset_ = 0.0f;
    PlagC::Vec3 referencePosition_{-5.85f, 0.0f, 0.59f};
    PlagC::Vec3 cameraPosition_{};
    float cameraYaw_ = 0.0f;
    float cameraPitch_ = 8.0f;
    bool cursorCaptured_ = true;
};
} // namespace PlagCGame
