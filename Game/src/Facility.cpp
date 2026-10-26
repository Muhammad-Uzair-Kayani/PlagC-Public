#include "pgcpch.h"
#include "TPSGame.h"
using namespace PlagC;
namespace Game
{
Entity TPSGame::Box(const std::string &name, Vec3 p, Vec3 size, unsigned material, bool collision)
{
    auto e = world_->Create(name);
    world_->SetTransform(e, {p, {}, size});
    world_->SetRenderable(e, {cube_, materials_.at(material)});
    if (collision)
    {
        PhysicsComponent c;
        c.body.motion = MotionType::Static;
        c.body.layer = CollisionLayer::StaticWorld;
        world_->SetPhysics(e, c);
    }
    return e;
}
void TPSGame::Prop(const std::string &name, Vec3 p, AssetHandle<ModelAsset> model, Vec3 half)
{
    auto e = world_->Create(name);
    world_->SetTransform(e, {p, {}, {1, 1, 1}});
    world_->SetModel(e, {model});
    PhysicsComponent c;
    c.body.motion = MotionType::Static;
    c.body.layer = CollisionLayer::StaticWorld;
    c.collider.dimensions = half;
    world_->SetPhysics(e, c);
}
void TPSGame::BuildFacility(Engine &)
{
    Box("Courtyard", {0, -.5f, 3}, {84, 1, 74}, 0);
    Box("West perimeter", {-42, 2, 3}, {1, 5, 74}, 1);
    Box("East perimeter", {42, 2, 3}, {1, 5, 74}, 1);
    Box("South perimeter", {0, 2, 40}, {84, 5, 1}, 1);
    auto room = [&](std::string name, float x, float z, float w, float d, float y = 0) {
        Box(name + " floor", {x, y - .2f, z}, {w, .4f, d}, 0);
        Box(name + " west", {x - w / 2, y + 2.6f, z}, {.5f, 5.2f, d}, 1);
        Box(name + " east", {x + w / 2, y + 2.6f, z}, {.5f, 5.2f, d}, 1);
        for (float side : {-1.f, 1.f})
        {
            for (float s : {-1.f, 1.f})
                Box(name + " portal wall", {x + s * (w / 4 + 1), y + 2.6f, z + side * d / 2},
                    {w / 2 - 2, 5.2f, .5f}, 1);
            Box(name + " lintel", {x, y + 4.4f, z + side * d / 2}, {4, 1.6f, .5f}, 2);
        }
        Box(name + " roof", {x, y + 5.4f, z}, {w + .5f, .4f, d + .5f}, 2);
        auto light = world_->Create(name + " light");
        world_->SetTransform(light, {{x, y + 4.5f, z}, {}, {1, 1, 1}});
        LightDesc l;
        l.type = LightType::Point;
        l.colour = {.65f, .8f, 1};
        l.power = 5;
        l.radius = 22;
        world_->SetLight(light, {l});
    };
    room("Warehouse", -26, -3, 24, 28);
    room("Barracks", 26, -3, 22, 28);
    room("Control", 0, -27, 22, 14);
    room("Workshop", -26, -26, 24, 16);
    room("Generator hall", 26, -26, 22, 16);
    // Side rooms open onto central corridors; every partition has a walking route.
    auto partition = [&](const std::string &name, float x, float z, float width, float depth,
                         float floor) {
        for (float side : {-1.f, 1.f})
        {
            Box(name + " corridor wall", {x + side * 2.5f, floor + 1.5f, z - depth / 4},
                {.25f, 3, depth / 2 - 2}, 1);
            Box(name + " corridor wall", {x + side * 2.5f, floor + 1.5f, z + depth / 4},
                {.25f, 3, depth / 2 - 2}, 1);
            for (float zz : {z - depth / 6, z + depth / 6})
                Box(name + " divider", {x + side * (width / 4 + 1.25f), floor + 1.5f, zz},
                    {width / 2 - 2.5f, 3, .25f}, 1);
        }
    };
    partition("Barracks rooms", 26, -3, 22, 28, 0);
    partition("Storage bays", -26, -3, 24, 28, 0);
    partition("Control rooms", 0, -27, 22, 14, 0);
    for (float x : {-32.f, -20.f})
        Box("Workshop bench", {x, .8f, -26}, {3, 1.6f, 2}, 2);
    for (float x : {20.f, 32.f})
        Prop("Power generator", {x, 0, -26}, generator_, {1, 1, 1});
    for (float x : {-33.f, -19.f})
        for (float z : {-10.f, -3.f, 4.f})
        {
            Box("Shelf", {x, 1, z}, {4, .25f, 2}, 2);
            for (float dx : {-1.8f, 1.8f})
                Box("Shelf leg", {x + dx, .5f, z}, {.15f, 1, 1.5f}, 2);
            Prop("Supply crate", {x, 1.2f, z}, crate_, {.7f, .5f, .5f});
        }
    for (float x : {20.f, 32.f})
        for (float z : {-11.f, -4.f, 3.f})
        {
            Box("Bunk frame", {x, .45f, z}, {2, .2f, 3}, 2);
            Box("Mattress", {x, .7f, z}, {1.8f, .3f, 2.8f}, 3);
        }
    for (float x : {-7.f, 7.f})
    {
        Box("Console", {x, .7f, -27}, {3, 1.4f, 2}, 2);
        Box("Monitor", {x, 1.7f, -27.6f}, {2, 1, .15f}, 6);
    }
    door_ = Box("Control sliding door", {0, 1.5f, -20}, {3.8f, 3, .35f}, 2);
    auto d = world_->Read(door_).physics.value();
    d.body.motion = MotionType::Kinematic;
    d.body.layer = CollisionLayer::Dynamic;
    world_->SetPhysics(door_, d);
    for (float x : {-9.f, 9.f})
        Box("Range cover", {x, .75f, -8}, {6, 1.5f, 1}, 1);
    for (float x : {-8.f, -4.f, 0.f, 4.f, 8.f})
    {
        auto target = Box("Reactive range target", {x, 1.7f, -16}, {.8f, 1.5f, .3f}, 4);
        targets_[target.value] = {};
        Box("Target stand", {x, .45f, -16}, {.2f, .9f, .3f}, 2);
    }
    Box("Tower deck", {0, 6, 6}, {10, .4f, 8}, 2);
    for (float x : {-4.5f, 4.5f})
        for (float z : {2.5f, 9.5f})
            Box("Tower support", {x, 3, z}, {.4f, 6, .4f}, 2);
    for (int i = 0; i < 24; ++i)
        Box("Tower stair", {8, .125f + .25f * i, 19 - .55f * i}, {3, .25f, .6f}, 2);
    Box("Tower west railing", {-4.8f, 6.8f, 6}, {.15f, 1.2f, 8}, 2);
    Box("Tower north railing", {0, 6.8f, 2.2f}, {10, 1.2f, .15f}, 2);
    Box("Tower south railing", {0, 6.8f, 9.8f}, {10, 1.2f, .15f}, 2);
    Box("Tower landing", {5.5f, 6, 6}, {5, .4f, 3}, 2);
    for (int i = 0; i < 20; ++i)
        Box("Tunnel stair", {0, -.2f * (i + 1) - .15f, -34.5f - .55f * i}, {4, .3f, .6f}, 0);
    Box("Tunnel west", {-2.3f, -.2f, -40}, {.5f, 8, 12}, 1);
    Box("Tunnel east", {2.3f, -.2f, -40}, {.5f, 8, 12}, 1);
    room("Lower lab", 0, -54, 24, 18, -4);
    partition("Lab rooms", 0, -54, 24, 18, -4);
    for (float x : {-8.f, 8.f})
        Prop("Generator", {x, -4, -54}, generator_, {1, 1, 1});
    Box("Live electrical floor", {8, -3.95f, -54}, {4, .1f, 5}, 4, false);
    Box("Supply station", {-7, 1, 28}, {2, 2, 1}, 5);
    for (float x : {-15.f, 15.f})
    {
        Box("Canopy", {x, 4, 18}, {12, .3f, 8}, 2);
        for (float z : {14.5f, 21.5f})
            Box("Canopy pillar", {x, 2, z}, {.4f, 4, .4f}, 2);
        Prop("Crate", {x, 0, 26}, crate_, {.7f, .5f, .5f});
    }
    auto sun = world_->Create("Late afternoon sun");
    LightDesc light;
    light.direction = {-.6f, -1, -.4f};
    light.colour = {1, .83f, .63f};
    light.power = 3;
    world_->SetLight(sun, {light});
}
} // namespace Game
