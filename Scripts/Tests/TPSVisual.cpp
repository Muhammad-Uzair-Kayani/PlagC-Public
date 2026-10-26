#include "pgcpch.h"
#include <PlagC/Application.h>
#include <PlagC/Thread.h>
#include "../../Game/src/TPSGame.h"
int main()
{
    PlagC::Thread::InitializeMain();
    try
    {
        PlagC::EngineConfig config;
        config.offscreen = true;
        config.workers = 2;
        config.audioEnabled = false;
        PlagC::Engine engine(config);
        Game::TPSGame game;
        game.Start(engine, config);
        auto target = engine.Graphics().CreateTarget(1280, 720);
        for (int i = 0; i < 180; ++i)
        {
            Game::Controls c;
            if (i == 75)
                c.mouseX = 1500;
            if (i >= 126)
                c.aim = true;
            if (i == 145)
                c.fire = true;
            game.Tick(engine, 1. / 60, c);
            engine.Graphics().Render(1. / 60);
            if (i == 60 || i == 120 || i == 150)
                engine.Graphics().CaptureTarget(target, "Logs/TPS-" + std::to_string(i) + ".ppm");
        }
        engine.Graphics().ResizeTarget(target, 960, 540);
        engine.Graphics().Render(1. / 60);
        engine.Graphics().CaptureTarget(target, "Logs/TPS-resize.ppm");
        engine.Graphics().DestroyTarget(target);
        game.Stop(engine);
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
