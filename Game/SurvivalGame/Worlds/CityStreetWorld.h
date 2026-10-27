#pragma once

#include "../../Framework/GameWorld.h"
#include <PlagC/Assets/AssetScene.h>
#include <PlagC/World/SceneRenderer.h>
#include <memory>

namespace PlagCGame
{
class CityStreetWorld final : public IGameWorld
{
  public:
    std::string_view Name() const override { return "CityStreet"; }
    void Start(PlagC::Engine &engine) override;
    void Frame(PlagC::Engine &engine, double dt) override;
    void Stop(PlagC::Engine &engine) override;

  private:
    std::unique_ptr<PlagC::World> world_;
    std::unique_ptr<PlagC::AssetScene> assetScene_;
    std::unique_ptr<PlagC::SceneRenderer> sceneRenderer_;

    PlagC::Entity environment_;
    PlagC::Entity player_;
    PlagC::Entity jeep_;
    PlagC::Entity grass_;
    PlagC::Entity vegetation_;
    PlagC::Entity camera_;
    PlagC::Entity sun_;
};
} // namespace PlagCGame
