#include "pgcpch.h"
#include "SurvivalGame.h"
#include <algorithm>
#include <stdexcept>

namespace PlagCGame
{
void SurvivalGame::Start(PlagC::Engine &engine, const PlagC::EngineConfig &,
                         std::string_view world)
{
    const auto worlds = Worlds();
    if (std::find(worlds.begin(), worlds.end(), world) == worlds.end())
        throw std::invalid_argument("SurvivalGame does not provide world: " + std::string(world));

    activeWorld_ = std::string(world);
    engine.Logger().Write(PlagC::Severity::Info, "GAME",
                          "Starting SurvivalGame world: " + activeWorld_);
}

void SurvivalGame::Frame(PlagC::Engine &, double)
{
    // World/gameplay implementation is intentionally empty until the runtime
    // host and asset mounting path are validated with the existing Default game.
}

void SurvivalGame::Stop(PlagC::Engine &engine)
{
    if (!activeWorld_.empty())
        engine.Logger().Write(PlagC::Severity::Info, "GAME",
                              "Stopping SurvivalGame world: " + activeWorld_);
    activeWorld_.clear();
}
} // namespace PlagCGame
