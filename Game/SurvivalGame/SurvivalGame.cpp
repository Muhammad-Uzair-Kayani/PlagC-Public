#include "pgcpch.h"
#include "SurvivalGame.h"
#include <algorithm>
#include <filesystem>
#include <stdexcept>

namespace PlagCGame
{
void SurvivalGame::Start(PlagC::Engine &engine, const PlagC::EngineConfig &config,
                         std::string_view world)
{
    const auto worlds = Worlds();
    if (std::find(worlds.begin(), worlds.end(), world) == worlds.end())
        throw std::invalid_argument("SurvivalGame does not provide world: " + std::string(world));

    activeWorld_ = std::string(world);

    const std::filesystem::path assetRoot = config.gameAssetRoot.empty()
                                                ? std::filesystem::path("GameAssets/SurvivalGame/Runtime")
                                                : std::filesystem::path(config.gameAssetRoot);
    if (!engine.Assets().HasMount("Game"))
        engine.Assets().Mount("Game", assetRoot);

    engine.Logger().Write(PlagC::Severity::Info, "GAME",
                          "Starting SurvivalGame world: " + activeWorld_);
    engine.Logger().Write(PlagC::Severity::Info, "ASSETS",
                          "Mounted Game:// at " + engine.Assets().MountedRoot("Game").generic_string());
    engine.Logger().Write(PlagC::Severity::Info, "ASSETS",
                          "World manifest: Game/SurvivalGame/Config/" + activeWorld_ + ".assets.json");
}

void SurvivalGame::Frame(PlagC::Engine &, double)
{
    // Asset manifest parsing/loading and concrete world creation are the next layer.
}

void SurvivalGame::Stop(PlagC::Engine &engine)
{
    if (!activeWorld_.empty())
        engine.Logger().Write(PlagC::Severity::Info, "GAME",
                              "Stopping SurvivalGame world: " + activeWorld_);
    activeWorld_.clear();
}
} // namespace PlagCGame
