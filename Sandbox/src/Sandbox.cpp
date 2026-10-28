#include "pgcpch.h"
#include <PlagC/Application.h>
#include "../../Game/Framework/GameRegistry.h"
#include "../../Game/Default/DefaultGame.h"
#include "../../Game/SurvivalGame/SurvivalGame.h"
#include "RendererBenchmarkGame.h"
#include <algorithm>
#include <iostream>
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
        registry_.Register("SurvivalGame", "Survival Game",
                           [] { return std::make_unique<PlagCGame::SurvivalGame>(); });
        registry_.Register("RendererBenchmark", "Renderer Backend Benchmark",
                           [] { return std::make_unique<PlagCGame::RendererBenchmarkGame>(); });
    }

    static bool NonInteractive(const PlagC::EngineConfig &c)
    {
        return c.headless || c.nullPlatform || c.offscreen;
    }

    std::string SelectGame() const
    {
        if (!config_.game.empty())
        {
            if (!registry_.Contains(config_.game))
                throw std::invalid_argument("unknown game: " + config_.game);
            return config_.game;
        }

        if (NonInteractive(config_))
            return "Default";

        const auto entries = registry_.Entries();
        std::cout << "\n=== PlagC Game Runtime ===\n";
        for (std::size_t i = 0; i < entries.size(); ++i)
            std::cout << '[' << (i + 1) << "] " << entries[i].second
                      << " (" << entries[i].first << ")\n";
        std::cout << "Select game [Enter = Default]: ";

        std::string input;
        std::getline(std::cin, input);
        if (input.empty())
            return "Default";

        try
        {
            const auto index = std::stoul(input);
            if (index >= 1 && index <= entries.size())
                return entries[index - 1].first;
        }
        catch (...)
        {
        }

        if (registry_.Contains(input))
            return input;
        throw std::invalid_argument("invalid game selection: " + input);
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

        if (NonInteractive(config_))
            return std::string(game.DefaultWorld());

        const auto worlds = game.Worlds();
        if (worlds.size() <= 1)
            return std::string(game.DefaultWorld());

        std::cout << "\nWorlds for " << game.Name() << ":\n";
        for (std::size_t i = 0; i < worlds.size(); ++i)
            std::cout << '[' << (i + 1) << "] " << worlds[i]
                      << (worlds[i] == game.DefaultWorld() ? " [default]" : "") << '\n';
        std::cout << "Select world [Enter = " << game.DefaultWorld() << "]: ";

        std::string input;
        std::getline(std::cin, input);
        if (input.empty())
            return std::string(game.DefaultWorld());

        try
        {
            const auto index = std::stoul(input);
            if (index >= 1 && index <= worlds.size())
                return std::string(worlds[index - 1]);
        }
        catch (...)
        {
        }

        if (std::find(worlds.begin(), worlds.end(), input) != worlds.end())
            return input;
        throw std::invalid_argument("invalid world selection: " + input);
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
