#include "pgcpch.h"
#include <PlagC/Application.h>
#include "../../Game/Framework/GameRegistry.h"
#include "../../Game/Default/DefaultGame.h"
#include <algorithm>
#include <stdexcept>

namespace
{
class Sandbox final : public PlagC::Application
{
    PlagC::EngineConfig config_;
    PlagCGame::GameRegistry registry_;
    PlagCGame::GamePtr game_;
    std::string selectedGame_;
    std::string selectedWorld_;

    void RegisterGames()
    {
        registry_.Register("Default", "PlagC Default",
                           [] { return std::make_unique<PlagCGame::DefaultGame>(); });
    }

    std::string SelectGame() const
    {
        if (!config_.game.empty() && config_.game != "Default")
            throw std::invalid_argument("only the Default game is available: " + config_.game);
        return "Default";
    }

    std::string SelectWorld(const PlagCGame::IGame &game) const
    {
        if (!config_.world.empty())
        {
            const auto worlds = game.Worlds();
            if (std::find(worlds.begin(), worlds.end(), config_.world) == worlds.end())
                throw std::invalid_argument("game does not provide world: " + config_.world);
            return config_.world;
        }
        return std::string(game.DefaultWorld());
    }

  public:
    explicit Sandbox(const PlagC::EngineConfig &c) : config_(c)
    {
        RegisterGames();
    }

    void OnStart(PlagC::Engine &engine) override
    {
        selectedGame_ = SelectGame();
        game_ = registry_.Create(selectedGame_);
        if (!game_)
            throw std::runtime_error("failed to create game: " + selectedGame_);

        selectedWorld_ = SelectWorld(*game_);
        engine.Logger().Write(PlagC::Severity::Info, "GAME",
                              "Selected game=" + selectedGame_ + " world=" + selectedWorld_);
        game_->Start(engine, config_, selectedWorld_);
    }

    void OnFrame(PlagC::Engine &engine, double dt) override
    {
        if (game_)
            game_->Frame(engine, dt);
    }

    void OnStop(PlagC::Engine &engine) override
    {
        if (game_)
            game_->Stop(engine);
        game_.reset();
    }
};
} // namespace

std::unique_ptr<PlagC::Application> PlagC::CreateApplication(const PlagC::EngineConfig &config)
{
    return std::make_unique<Sandbox>(config);
}
