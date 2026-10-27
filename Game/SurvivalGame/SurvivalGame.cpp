#include "pgcpch.h"
#include "SurvivalGame.h"
#include "Worlds/CityStreetWorld.h"
#include "Worlds/JoelRigValidationWorld.h"
#include "Worlds/RigValidationWorld.h"
#include <algorithm>
#include <filesystem>
#include <stdexcept>

namespace PlagCGame
{
namespace
{
std::unique_ptr<IGameWorld> CreateWorld(std::string_view name, const PlagC::EngineConfig &config)
{
    if (name == "CityStreet")
        return std::make_unique<CityStreetWorld>(config);
    if (name == "RigValidation")
        return std::make_unique<RigValidationWorld>();
    if (name == "JoelRigValidation")
        return std::make_unique<JoelRigValidationWorld>();
    return {};
}
} // namespace

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
    const std::filesystem::path manifestPath =
        std::filesystem::path("Game/SurvivalGame/Config") / (activeWorld_ + ".assets.json");

    if (!std::filesystem::is_directory(assetRoot))
        throw std::runtime_error("SurvivalGame asset root is missing: " + assetRoot.string());

    engine.Logger().Write(PlagC::Severity::Info, "GAME",
                          "Starting SurvivalGame world: " + activeWorld_);
    engine.Logger().Write(PlagC::Severity::Info, "ASSET",
                          "Mounting SurvivalGame assets: " + assetRoot.string());

    try
    {
        engine.Assets().Mount("Game", assetRoot);
        gameAssetsMounted_ = true;

        manifest_ = WorldAssetManifest::Load(manifestPath);
        if (manifest_->World() != activeWorld_)
            throw std::runtime_error("manifest world mismatch: expected " + activeWorld_ +
                                     ", got " + manifest_->World());

        manifest_->LoadAll(engine.Assets());
        engine.Logger().Write(PlagC::Severity::Info, "ASSET",
                              "Loaded " + std::to_string(manifest_->Assets().size()) +
                                  " assets for world " + activeWorld_);

        world_ = CreateWorld(activeWorld_, config);
        if (world_)
            world_->Start(engine);
        else
            engine.Logger().Write(PlagC::Severity::Warning, "WORLD",
                                  activeWorld_ + " currently has an asset-only runtime shell");
    }
    catch (...)
    {
        if (world_)
        {
            try { world_->Stop(engine); } catch (...) {}
            world_.reset();
        }
        if (manifest_)
        {
            try { manifest_->UnloadAll(engine.Assets()); } catch (...) {}
            manifest_.reset();
        }
        if (gameAssetsMounted_)
        {
            try { engine.Assets().Unmount("Game"); } catch (...) {}
            gameAssetsMounted_ = false;
        }
        activeWorld_.clear();
        throw;
    }
}

void SurvivalGame::Frame(PlagC::Engine &engine, double dt)
{
    if (world_)
        world_->Frame(engine, dt);
}

void SurvivalGame::Stop(PlagC::Engine &engine)
{
    if (world_)
    {
        world_->Stop(engine);
        world_.reset();
    }

    if (manifest_)
    {
        manifest_->UnloadAll(engine.Assets());
        manifest_.reset();
    }

    if (gameAssetsMounted_)
    {
        engine.Assets().Unmount("Game");
        gameAssetsMounted_ = false;
    }

    if (!activeWorld_.empty())
        engine.Logger().Write(PlagC::Severity::Info, "GAME",
                              "Stopping SurvivalGame world: " + activeWorld_);
    activeWorld_.clear();
}
} // namespace PlagCGame
