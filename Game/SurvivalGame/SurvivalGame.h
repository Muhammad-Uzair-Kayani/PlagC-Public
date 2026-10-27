#pragma once

#include "../Framework/Game.h"

namespace PlagCGame
{
class SurvivalGame final : public IGame
{
  public:
    std::string_view Id() const override { return "SurvivalGame"; }
    std::string_view Name() const override { return "Survival Game"; }
    std::vector<std::string_view> Worlds() const override { return {"CityStreet", "Garage", "Shop"}; }
    std::string_view DefaultWorld() const override { return "CityStreet"; }

    void Start(PlagC::Engine &engine, const PlagC::EngineConfig &config,
               std::string_view world) override;
    void Frame(PlagC::Engine &engine, double dt) override;
    void Stop(PlagC::Engine &engine) override;

  private:
    std::string activeWorld_;
};
} // namespace PlagCGame
