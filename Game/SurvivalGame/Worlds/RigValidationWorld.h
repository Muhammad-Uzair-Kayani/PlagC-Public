#pragma once

#include "../../Framework/GameWorld.h"
#include <PlagC/Assets/AssetScene.h>
#include <PlagC/World/SceneRenderer.h>
#include <memory>

namespace PlagCGame
{
class RigValidationWorld final : public IGameWorld
{
  public:
    std::string_view Name() const override { return "RigValidation"; }
    void Start(PlagC::Engine &engine) override;
    void Frame(PlagC::Engine &engine, double dt) override;
    void Stop(PlagC::Engine &engine) override;

  private:
    std::unique_ptr<PlagC::World> world_;
    std::unique_ptr<PlagC::AssetScene> assetScene_;
    std::unique_ptr<PlagC::SceneRenderer> sceneRenderer_;
    PlagC::AssetHandle<PlagC::ModelAsset> jokerAsset_;

    PlagC::Entity joker_;
    PlagC::Entity camera_;
    PlagC::Entity sun_;

    PlagC::EventBus::Connection key_;
    PlagC::EventBus::Connection focus_;
    PlagC::EventBus::Connection mouse_;

    int requestedMode_ = 0; // 0 = bare JokerBase rest skeleton, >=100 = UAL clip index + 100
    int activeMode_ = -1;
    unsigned ualPage_ = 0;
    unsigned ualClipCount_ = 0;
    float cameraYaw_ = 0.0f;
    float cameraPitch_ = 5.0f;
    bool cursorCaptured_ = true;
};
} // namespace PlagCGame
