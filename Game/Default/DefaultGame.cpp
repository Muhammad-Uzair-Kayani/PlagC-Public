#include "pgcpch.h"
#include "DefaultGame.h"
#include "BlackDragonLabWorld.h"
#include <stdexcept>

namespace PlagCGame
{
void DefaultGame::Start(PlagC::Engine &engine, const PlagC::EngineConfig &config,
                        std::string_view world)
{
    if (world == "DefaultScene")
    {
        defaultSceneActive_ = true;
        game_.Start(engine, config);
        return;
    }
    if (world == "BlackDragonLab")
    {
        defaultSceneActive_ = false;
        customWorld_ = std::make_unique<BlackDragonLabWorld>();
        customWorld_->Start(engine);
        return;
    }
    throw std::invalid_argument("Default game does not provide world: " + std::string(world));
}

void DefaultGame::Frame(PlagC::Engine &engine, double dt)
{
    if (customWorld_)
    {
        customWorld_->Frame(engine, dt);
        return;
    }
    if (!defaultSceneActive_)
        throw std::logic_error("DefaultGame frame before Start");

    game_.Frame(engine, dt);
    float aspect = 16.0f / 9.0f;
    if (auto *window = engine.MainWindow())
        if (window->Height())
            aspect = static_cast<float>(window->Width()) / static_cast<float>(window->Height());
    fpsOverlay_.Frame(engine.Graphics(), game_.View(), dt, aspect);
}

void DefaultGame::Stop(PlagC::Engine &engine)
{
    if (customWorld_)
    {
        customWorld_->Stop(engine);
        customWorld_.reset();
        return;
    }
    if (defaultSceneActive_)
    {
        fpsOverlay_.Clear(engine.Graphics());
        game_.Stop(engine);
        defaultSceneActive_ = false;
    }
}
} // namespace PlagCGame
