#include "pgcpch.h"
#include "TPSGame.h"
using namespace PlagC;
namespace Game
{
void TPSGame::BuildSettlement(Engine &e)
{
    auto material = [&](Vec3 colour, float roughness, float metallic = 0.f) {
        MaterialDesc d;
        d.colour = colour;
        d.roughness = roughness;
        d.metallic = metallic;
        unsigned index = unsigned(materials_.size());
        materials_.push_back(e.Graphics().CreateMaterial(d));
        return index;
    };
    unsigned plaster = material({.54f, .46f, .32f}, .9f),
             wood = material({.16f, .10f, .065f}, .86f);
    unsigned roof = material({.12f, .17f, .18f}, .55f, .25f),
             glass = material({.16f, .25f, .28f}, .16f, .4f);
    auto tilted = [&](const char *name, Vec3 pos, Vec3 size, Vec3 rotation, unsigned m) {
        auto entity = Box(name, pos, size, m);
        auto t = world_->ReadTransform(entity);
        t.rotationDegrees = rotation;
        world_->SetTransform(entity, t);
        return entity;
    };
    MeshData gable;
    gable.vertices = {{{-6, 0, 0}, {0, 0, 1}, 0, 0},      {{6, 0, 0}, {0, 0, 1}, 1, 0},
                      {{0, 2.1f, 0}, {0, 0, 1}, .5f, 1},  {{-6, 0, 0}, {0, 0, -1}, 0, 0},
                      {{0, 2.1f, 0}, {0, 0, -1}, .5f, 1}, {{6, 0, 0}, {0, 0, -1}, 1, 0}};
    gable.indices = {0, 1, 2, 3, 4, 5};
    auto gableMesh = e.Graphics().CreateMesh(gable);
    exteriorMeshes_.push_back(gableMesh);
    for (float x : {-68.f, 66.f})
        for (float z : {59.f, 88.f, 117.f})
        {
            float y = LandscapeSurface::Height(x, z);
            Box("Settlement foundation", {x, y - .3f, z}, {14, .6f, 17}, 1);
            Box("Settlement timber floor", {x, y + .08f, z}, {12, .16f, 14}, wood);
            for (float side : {-1.f, 1.f})
            {
                // Both end walls have real walk-through doors opening onto cross-paths.
                for (float part : {-1.f, 1.f})
                    Box("House entrance wall", {x + side * 6, y + 1.6f, z + part * 4.1f},
                        {.35f, 3.2f, 5.8f}, plaster);
                Box("House door lintel", {x + side * 6, y + 2.9f, z}, {.42f, .6f, 2.4f}, wood);
                Box("House door frame", {x + side * 6, y + 1.15f, z - 1.25f}, {.5f, 2.3f, .16f},
                    wood);
                Box("House door frame", {x + side * 6, y + 1.15f, z + 1.25f}, {.5f, 2.3f, .16f},
                    wood);
                auto end = world_->Create("House roof gable");
                world_->SetTransform(end, {{x, y + 3.2f, z + side * 7}, {}, {1, 1, 1}});
                world_->SetRenderable(end, {gableMesh, materials_[plaster]});
                Box("House window sill wall", {x, y + .55f, z + side * 7}, {12, 1.1f, .35f},
                    plaster);
                Box("House window header", {x, y + 2.9f, z + side * 7}, {12, .6f, .35f}, plaster);
                for (float part : {-1.f, 0.f, 1.f})
                    Box("House window pier", {x + part * 5.2f, y + 1.85f, z + side * 7},
                        {1.6f, 1.5f, .35f}, plaster);
                for (float part : {-1.f, 1.f})
                {
                    Box("House window pane", {x + part * 2.6f, y + 1.85f, z + side * 7},
                        {3.5f, 1.4f, .08f}, glass);
                    Box("House window mullion", {x + part * 2.6f, y + 1.85f, z + side * 7.05f},
                        {.10f, 1.4f, .16f}, wood);
                    Box("House window crossbar", {x + part * 2.6f, y + 1.85f, z + side * 7.05f},
                        {3.5f, .10f, .16f}, wood);
                }
                tilted("House pitched roof", {x + side * 3.25f, y + 4.1f, z}, {7.25f, .22f, 16},
                       {0, 0, -side * 20}, roof);
                Box("House porch step", {x + side * 7, y + .12f, z}, {2, .24f, 4}, 1);
            }
            Box("House roof ridge", {x, y + 5.35f, z}, {.25f, .2f, 16}, wood);
            Box("House chimney", {x + 3, y + 4.9f, z - 3}, {.8f, 2, .8f}, 1);
            Box("House table", {x + 2, y + .8f, z + 3}, {2, 1.6f, 1}, wood);
            Box("House bench", {x + 2, y + .4f, z + 1.8f}, {2, .8f, .5f}, wood);
            Prop("Settlement supply crate", {x - 3, y + .16f, z - 3}, crate_, {.6f, .5f, .5f});
            auto lamp = world_->Create("House warm porch light");
            world_->SetTransform(lamp, {{x + 6.5f, y + 2.6f, z}, {}, {1, 1, 1}});
            LightDesc light;
            light.type = LightType::Point;
            light.colour = {1, .67f, .30f};
            light.power = 2.5f;
            light.radius = 9;
            world_->SetLight(lamp, {light});
        }
    for (float x : {-6.2f, 6.2f})
    {
        Box("Gate masonry pillar", {x, 2.7f, 40}, {.8f, 5.4f, 1.2f}, 1);
        Box("Gate brass cap", {x, 5.5f, 40}, {1, .2f, 1.4f}, roof);
    }
    for (float z = 46; z <= 146; z += 20)
    {
        float x = -7 + 5 * std::sin((z - 45) * .022f), y = LandscapeSurface::Height(x, z);
        Box("Road timber bollard", {x, y + .6f, z}, {.2f, 1.2f, .2f}, wood);
        Box("Road reflective cap", {x, y + 1.05f, z}, {.24f, .1f, .24f}, 6, false);
    }
}
} // namespace Game
