#pragma once

#include "../Framework/Game.h"
#include "../Framework/GameWorld.h"
#include "../Framework/WorldAssetManifest.h"
#include <optional>

namespace PlagCGame
{
class SurvivalGame final : public IGame
{
  public:
    std::string_view Id() const override { return "SurvivalGame"; }
    std::string_view Name() const override { return "Survival Game"; }
    std::vector<std::string_view> Worlds() const override { return {"CityStreet", "Garage", "Shop", "RigValidation"}; }
    std::string_view DefaultWorld() const override { return "CityStreet"; }

    void Start(PlagC::Engine &engine, const PlagC::EngineConfig &config,
               std::string_view world) override;
    void Frame(PlagC::Engine &engine, double dt) override;
    void Stop(PlagC::Engine &engine) override;

  private:
    std::string activeWorld_;
    std::optional<WorldAssetManifest> manifest_;
    std::unique_ptr<IGameWorld> world_;
    bool gameAssetsMounted_ = false;
};
} // namespace PlagCGame
