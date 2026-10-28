#include "pgcpch.h"
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

    float aspect = 16.0f / 9.0f;
    if (auto *window = engine.MainWindow())
        if (window->Height())
            aspect = static_cast<float>(window->Width()) / static_cast<float>(window->Height());
    fpsOverlay_.Frame(engine.Graphics(), game_.View(), dt, aspect);
}

void DefaultGame::Stop(PlagC::Engine &engine)
{
    fpsOverlay_.Clear(engine.Graphics());
    game_.Stop(engine);
}
} // namespace PlagCGame
