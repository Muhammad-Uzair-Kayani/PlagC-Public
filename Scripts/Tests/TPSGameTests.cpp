// Real Engine/application/StressWorld code with recording graphics and no native OS window.
#include "pgcpch.h"
#include <PlagC/Application.h>
#include <PlagC/Thread.h>
#include <PlagC/Input/Input.h>
#include "../../Game/src/TPSGame.h"
#include <fstream>
namespace PlagC
{
InputState replayInput;
void Window::Poll()
{
    Input::state_ = replayInput;
}
struct Log::Impl
{
};
Log::Log() : impl_(std::make_unique<Impl>())
{
}
Log::~Log() = default;
void Log::Write(Severity, std::string_view, std::string_view)
{
}
struct Window::Impl
{
};
Window::Window(EventBus &, const WindowConfig &) : impl_(std::make_unique<Impl>())
{
}
Window::~Window() = default;
void Window::SetCursorCaptured(bool)
{
}
void Window::SetTitle(const std::string &)
{
}
int Window::Width() const
{
    return 1280;
}
int Window::Height() const
{
    return 720;
}
} // namespace PlagC
using namespace PlagC;
void Check(bool ok, const char *what)
{
    if (!ok)
        throw std::runtime_error(what);
    std::cout << "[PASS] " << what << std::endl;
}
int main()
{
    Thread::InitializeMain();
    try
    {
        Game::Weapon rifle;
        Check(rifle.Fire() && rifle.rounds == 29, "shot consumes ammunition");
        Check(!rifle.Fire(), "fire interval enforced");
        Check(rifle.Reload(), "reload starts");
        Check(!rifle.Fire(), "reload blocks firing");
        rifle.Tick(2.1);
        Check(rifle.rounds == 30 && rifle.reserve == 149, "reload transfers only missing rounds");
        EngineConfig c;
        c.headless = true;
        c.workers = 2;
        c.physicsWorkers = 2;
        c.audioEnabled = false;
        Engine engine(c);
        Game::TPSGame app;
        app.Start(engine, c);
        auto tick = [&](int n, const Game::Controls &input = Game::Controls{}) {
            for (int i = 0; i < n; ++i)
                app.Tick(engine, 1. / 60, input);
        };
        tick(90);
        Check(app.Player().grounded, "character settles on facility floor");
        auto start = app.Player().position;
        Game::Controls walk;
        walk.forward = 1;
        tick(30, walk);
        Check(app.Player().position.z < start.z - 1, "camera-relative walking");
        walk.sprint = true;
        start = app.Player().position;
        tick(30, walk);
        Check(start.z - app.Player().position.z > 2.5f, "sprint increases movement speed");
        Game::Controls jump;
        jump.jump = true;
        tick(1, jump);
        tick(6);
        Check(!app.Player().grounded, "jump leaves floor");
        tick(90);
        Game::Controls aim;
        aim.aim = true;
        tick(30, aim);
        Check(app.View().fov < 49, "aim narrows camera FOV");
        aim.fire = true;
        tick(1, aim);
        Check(app.Rifle().rounds == 29, "gameplay fires rifle");
        app.Reload();
        Check(app.Rifle().reload > 0, "gameplay starts reload");
        tick(140);
        Check(app.Rifle().rounds == 30, "gameplay completes reload");
        app.Damage(100);
        tick(1);
        Check(app.HitPoints() == 0, "damage reaches death state");
        for (int i = 0; i < 3; ++i)
        {
            app.Stop(engine);
            Check(engine.Graphics().Statistics().objects == 0 &&
                      engine.Graphics().Statistics().lights == 0,
                  "shutdown releases every render object and light");
            app.Start(engine, c);
            Check(app.HitPoints() == 100 && app.Rifle().rounds == 30,
                  "redeploy resets player state");
        }
        auto walkTo = [&](float x, float z) {
            for (int i = 0; i < 1800; ++i)
            {
                auto p = app.Player().position;
                float dx = x - p.x, dz = z - p.z;
                float n = std::sqrt(dx * dx + dz * dz);
                if (n < .4f)
                    return;
                Game::Controls c;
                c.right = dx / n;
                c.forward = -dz / n;
                tick(1, c);
            }
            auto p = app.Player().position;
            throw std::runtime_error("blocked route to " + std::to_string(x) + "," +
                                     std::to_string(z) + " at " + std::to_string(p.x) + "," +
                                     std::to_string(p.y) + "," + std::to_string(p.z));
        };
        walkTo(-26, 13);
        walkTo(-26, -3);
        walkTo(-26, -26);
        Check(app.Player().position.z < -25, "warehouse connects to workshop");
        walkTo(-26, 13);
        walkTo(26, 13);
        walkTo(26, -3);
        walkTo(26, -26);
        Check(app.Player().position.x > 25, "barracks connects to generator hall");
        app.Stop(engine);
        app.Start(engine, c);
        tick(60);
        walkTo(8, 21);
        walkTo(8, 6);
        tick(30);
        Check(app.Player().position.y > 6, "tower stairs are climbable");
        walkTo(0, 6);
        app.Stop(engine);
        app.Start(engine, c);
        tick(60);
        walkTo(2.5f, -18);
        walkTo(0, -18);
        Event interact{EventType::KeyPressed};
        interact.key = KeyCode::E;
        engine.Events().Post(interact);
        engine.Events().Pump();
        tick(120);
        walkTo(0, -27);
        walkTo(0, -54);
        Check(app.Player().position.y < -2, "control door and tunnel connect to lower lab");
        app.Stop(engine);
        app.Start(engine, c);
        tick(60);
        walkTo(-4, -6);
        Game::Controls shot;
        shot.aim = true;
        tick(30, shot);
        for (int i = 0; i < 12; ++i)
        {
            auto v = app.View();
            float dx = -4 - v.position.x, dy = 1.7f - v.position.y, dz = -16 - v.position.z;
            float yaw = std::atan2(v.target.z - v.position.z, v.target.x - v.position.x) * 180 /
                        3.14159265f;
            float pitch =
                std::atan2(v.target.y - v.position.y,
                           std::hypot(v.target.x - v.position.x, v.target.z - v.position.z)) *
                180 / 3.14159265f;
            shot.mouseX =
                std::remainder(std::atan2(dz, dx) * 180 / 3.14159265f - yaw, 360.f) / .12f;
            shot.mouseY = -(std::atan2(dy, std::hypot(dx, dz)) * 180 / 3.14159265f - pitch) / .12f;
            tick(1, shot);
        }
        shot.mouseX = shot.mouseY = 0;
        shot.fire = true;
        tick(48, shot);
        Check(app.Hits() >= 4, "muzzle ray damages and disables aimed range target");
        app.Damage(100);
        tick(180);
        app.Frame(engine, 0);
        Check(app.HitPoints() == 100, "death automatically redeploys player");
        app.Stop(engine);
        Check(engine.Graphics().Statistics().textures == 0, "shutdown releases all textures");
        World world;
        auto wall = world.Create("wall");
        world.SetTransform(wall, {{0, 1, 2}, {}, {5, 5, .5f}});
        PhysicsComponent body;
        body.body.motion = MotionType::Static;
        body.body.layer = CollisionLayer::StaticWorld;
        world.SetPhysics(wall, body);
        PhysicsSystem physics(world);
        physics.Synchronize();
        ThirdPersonCameraController camera;
        camera.look.pitch = 0;
        auto view = camera.Update(physics, {0, 1, 0}, false, 1. / 60);
        Check(view.position.z < 1.8f, "camera sphere cast stops before wall");
        std::cout << "TPS gameplay checks passed\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
