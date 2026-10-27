// Native rendering/collision acceptance for the populated world and isolated bloom/combat effects.
#include "pgcpch.h"
#include <PlagC/Application.h>
#include <PlagC/Thread.h>
#include "../../Game/src/TPSGame.h"
#include "../../Game/src/Landscape.h"
#include "../../Game/src/CombatEffects.h"
#include <dlfcn.h>
#include <iostream>
using namespace PlagC;
void Check(bool ok, const char *message)
{
    if (!ok)
        throw std::runtime_error(message);
    std::cout << "[PASS] " << message << '\n';
}
int main()
{
    Thread::InitializeMain();
    try
    {
        EngineConfig config;
        config.offscreen = true;
        config.audioEnabled = false;
        config.workers = 2;
        Engine engine(config);
        auto &r = engine.Graphics();
        auto target = r.CreateTarget(1280, 720);
        Game::TPSGame game;
        game.Start(engine, config);
        for (unsigned i = 0; i < 75; ++i)
        {
            game.Tick(engine, 1. / 60, {});
            r.Render(1. / 60);
        }
        auto populated = r.Statistics();
        Check(populated.objects > 500, "populated scene creates terrain and vegetation");
        Check(populated.instances > populated.drawCalls,
              "shared meshes use Ogre automatic instancing/multi-draw batching");
        r.CaptureTarget(target, "Logs/Valley-gameplay.ppm");
        r.SetCamera({{110, 85, 170}, {0, 0, 25}, 60, .1f, 600});
        r.Render(1. / 60);
        r.CaptureTarget(target, "Logs/Valley-overview.ppm");
        std::cout << "POPULATED objects=" << populated.objects << " draws=" << populated.drawCalls
                  << " triangles=" << populated.triangles << " instances=" << populated.instances
                  << '\n';
        // Cross the new gate into the road network; preserve original compound gameplay.
        for (unsigned i = 0; i < 40; ++i)
        {
            Game::Controls c;
            c.right = 1;
            game.Tick(engine, 1. / 60, c);
        }
        for (unsigned i = 0; i < 360; ++i)
        {
            Game::Controls c;
            c.forward = -1;
            c.sprint = true;
            game.Tick(engine, 1. / 60, c);
        }
        Check(game.Player().position.z > 48,
              "player can leave compound through gate onto terrain road");
        Check(game.Player().grounded, "player remains grounded outside compound");
        {
            Game::Controls c;
            c.aim = true;
            c.mouseX = 750;
            game.Tick(engine, 1. / 60, c);
        }
        for (unsigned i = 0; i < 20; ++i)
        {
            Game::Controls c;
            c.aim = true;
            game.Tick(engine, 1. / 60, c);
            r.Render(1. / 60);
        }
        r.CaptureTarget(target, "Logs/Valley-settlement.ppm");
        game.Stop(engine);
        Check(r.Statistics().objects == 0 && r.Statistics().lights == 0,
              "world and effects release all objects and lights");
        Check(r.Statistics().meshes == 0 && r.Statistics().materials == 0 &&
                  r.Statistics().textures == 0,
              "world releases geometry, materials and textures");
        // Same sampled triangles are sent to Jolt; rays and character grounding must agree.
        {
            World world;
            PhysicsConfig pc;
            pc.workers = 0;
            PhysicsSystem physics(world, pc);
            auto tile = Game::LandscapeSurface::Tile(80, 80, 1);
            auto g = std::make_shared<ColliderGeometry>();
            for (unsigned i = 0; i < 33 * 33; ++i)
                g->vertices.push_back(tile.vertices[i].position);
            for (std::size_t i = 0; i < tile.indices.size(); i += 3)
                if (tile.indices[i] < 33 * 33 && tile.indices[i + 1] < 33 * 33 &&
                    tile.indices[i + 2] < 33 * 33)
                    g->indices.insert(g->indices.end(),
                                      {tile.indices[i], tile.indices[i + 1], tile.indices[i + 2]});
            auto floor = world.Create("Shared surface");
            PhysicsComponent p;
            p.body.motion = MotionType::Static;
            p.body.layer = CollisionLayer::StaticWorld;
            p.collider.type = ColliderType::Mesh;
            p.collider.geometry = g;
            world.SetPhysics(floor, p);
            physics.Synchronize();
            auto hit = physics.Raycast({96, 100, 96}, {0, -150, 0});
            Check(hit && std::abs(hit->position.y - Game::LandscapeSurface::Height(96, 96)) < .03f,
                  "Jolt collision agrees with sampled terrain");
            auto character =
                physics.CreateCharacter({{96, Game::LandscapeSurface::Height(96, 96) + 3, 96}});
            for (unsigned i = 0; i < 180; ++i)
            {
                physics.SetCharacterInput(character, {}, false);
                physics.Advance(1. / 60);
            }
            Check(physics.ReadCharacter(character).grounded, "character grounds on uneven terrain");
            auto low = Game::LandscapeSurface::Tile(80, 80, 4);
            Check(low.indices.size() < tile.indices.size() / 5,
                  "terrain LOD reduces triangle work");
        }
        // Independent VFX scene, without the environment/character/HUD masking output.
        r.SetEnvironment({});
        auto settings = r.Settings();
        settings.hdr = true;
        settings.fogDensity = 0;
        settings.bloom = false;
        settings.shadows = false;
        settings.background = {0, 0, 0};
        r.SetSettings(settings);
        auto mesh = r.CreateMesh(MakePrimitive(Primitive::Cube));
        MaterialDesc m;
        m.colour = {0, 0, 0};
        m.emissive = {12, 4, 1};
        auto mat = r.CreateMaterial(m);
        auto glow = r.CreateObject(mesh, mat, {{0, 1, 0}, {}, {.18f, .18f, .18f}});
        r.SetCamera({{0, 1, 5}, {0, 1, 0}, 60, .05f, 30});
        r.ResizeTarget(target, 256, 256);
        for (int i = 0; i < 3; ++i)
            r.Render(1. / 60);
        r.CaptureTarget(target, "Logs/Bloom-off.ppm");
        settings.bloom = true;
        r.SetSettings(settings);
        for (int i = 0; i < 3; ++i)
            r.Render(1. / 60);
        r.CaptureTarget(target, "Logs/Bloom-on.ppm");
        r.ResizeTarget(target, 320, 180);
        r.Render(1. / 60);
        r.CaptureTarget(target, "Logs/Bloom-resize.ppm");
        r.DestroyObject(glow);
        r.DestroyMesh(mesh);
        r.DestroyMaterial(mat);
        {
            Game::CombatEffects effects(r);
            auto count = r.Statistics();
            effects.Muzzle({0, 1, 0});
            effects.Impact({0, 1, 0}, {0, 1, 0}, true);
            effects.Impact({1, 1, 0}, {0, 1, 0}, false);
            for (unsigned i = 0; i < 30; ++i)
                effects.Update(1. / 30);
            Check(r.Statistics().objects == count.objects, "impact particle pool remains bounded");
        }
        Check(r.Statistics().objects == 0 && r.Statistics().lights == 0,
              "isolated combat effects release resources");
        auto error = reinterpret_cast<unsigned (*)()>(dlsym(RTLD_DEFAULT, "glGetError"));
        Check(error && error() == 0, "native GL reports no error after bloom resize and effects");
        r.DestroyTarget(target);
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
