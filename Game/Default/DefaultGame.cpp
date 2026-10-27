#include "DefaultGame.h"
#include <stdexcept>

namespace PlagCGame
{
void DefaultGame::Start(PlagC::Engine &engine, const PlagC::EngineConfig &config,
                        std::string_view world)
{
    if (world != DefaultWorld())
        throw std::invalid_argument("Default game does not provide world: " + std::string(world));
    game_.Start(engine, config);
}

void DefaultGame::Frame(PlagC::Engine &engine, double dt)
{
    game_.Frame(engine, dt);
}

void DefaultGame::Stop(PlagC::Engine &engine)
{
    game_.Stop(engine);
}
} // namespace PlagCGame
