#pragma once

#include "../Framework/Game.h"
#include "../Framework/FpsOverlay.h"
#include "../Framework/GameWorld.h"
#include "../src/TPSGame.h"
#include <memory>

namespace PlagCGame
{
class DefaultGame final : public IGame
{
  public:
    std::string_view Id() const override { return "Default"; }
    std::string_view Name() const override { return "PlagC Default"; }
    std::vector<std::string_view> Worlds() const override
    {
        return {"DefaultScene", "BlackDragonLab"};
    }
    std::string_view DefaultWorld() const override { return "DefaultScene"; }

    void Start(PlagC::Engine &engine, const PlagC::EngineConfig &config,
               std::string_view world) override;
    void Frame(PlagC::Engine &engine, double dt) override;
    void Stop(PlagC::Engine &engine) override;

  private:
    Game::TPSGame game_;
    FpsOverlay fpsOverlay_{};
    std::unique_ptr<IGameWorld> customWorld_;
    bool defaultSceneActive_ = false;
};
} // namespace PlagCGame
