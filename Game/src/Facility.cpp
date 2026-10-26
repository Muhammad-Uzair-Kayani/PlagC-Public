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
    // OUTPOST ZERO doubles as PlagC's renderer acceptance scene.  Each zone deliberately
    // stresses a different part of the public renderer API instead of reaching into Ogre.
    auto pointLight = [&](const std::string &name, Vec3 p, Vec3 colour, float power, float radius) {
        auto e = world_->Create(name);
        world_->SetTransform(e, {p, {}, {1, 1, 1}});
        LightDesc l;
        l.type = LightType::Point;
        l.position = p;
        l.colour = colour;
        l.power = power;
        l.radius = radius;
        world_->SetLight(e, {l});
        return e;
    };
    auto spotLight = [&](const std::string &name, Vec3 p, Vec3 direction, Vec3 colour,
                         float power, float radius, float inner = 22, float outer = 38) {
        auto e = world_->Create(name);
        world_->SetTransform(e, {p, {}, {1, 1, 1}});
        LightDesc l;
        l.type = LightType::Spot;
        l.position = p;
        l.direction = direction;
        l.colour = colour;
        l.power = power;
        l.radius = radius;
        l.innerAngle = inner;
        l.outerAngle = outer;
        world_->SetLight(e, {l});
        return e;
    };
    auto emissiveStrip = [&](const std::string &name, Vec3 p, Vec3 size) {
        return Box(name, p, size, 6, false);
    };

    // Exterior: broad late-afternoon key light, environment reflections and long shadow silhouettes.
    Box("Courtyard", {0, -.5f, 3}, {84, 1, 74}, 0);
    Box("West perimeter", {-42, 2, 3}, {1, 5, 74}, 1);
    Box("East perimeter", {42, 2, 3}, {1, 5, 74}, 1);
    Box("South perimeter", {0, 2, 40}, {84, 5, 1}, 1);
    for (float x : {-36.f, -18.f, 0.f, 18.f, 36.f})
    {
        Box("Courtyard bollard", {x, .75f, 32}, {.35f, 1.5f, .35f}, 2);
        emissiveStrip("Bollard lamp", {x, 1.45f, 32}, {.22f, .12f, .22f});
        pointLight("Bollard pool light", {x, 1.6f, 32}, {.18f, .55f, 1.f}, 2.2f, 8.f);
    }

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
    };
    room("Warehouse", -26, -3, 24, 28);
    room("Barracks", 26, -3, 22, 28);
    room("Control", 0, -27, 22, 14);
    room("Workshop", -26, -26, 24, 16);
    room("Generator hall", 26, -26, 22, 16);

    // Give every interior a distinct colour script so PBR roughness/metalness can be judged
    // under mixed lighting rather than a single flat ambient condition.
    pointLight("Warehouse cold fill", {-26, 4.3f, -3}, {.48f, .68f, 1.f}, 5.5f, 22.f);
    spotLight("Warehouse inspection spot", {-26, 4.8f, 4}, {0, -1, -.2f}, {.55f, .78f, 1.f}, 8.f, 24.f);
    pointLight("Barracks warm fill", {26, 4.2f, -3}, {1.f, .58f, .3f}, 4.2f, 20.f);
    spotLight("Barracks ceiling spot", {26, 4.8f, -8}, {0, -1, .15f}, {1.f, .72f, .45f}, 6.f, 18.f);
    pointLight("Control cyan fill", {0, 3.8f, -27}, {.12f, .72f, 1.f}, 4.8f, 17.f);
    pointLight("Workshop neutral fill", {-26, 4.2f, -26}, {.72f, .82f, 1.f}, 4.5f, 18.f);
    pointLight("Generator amber key", {26, 3.8f, -26}, {1.f, .36f, .08f}, 6.5f, 20.f);
    spotLight("Generator service spot", {26, 4.8f, -30}, {0, -1, .25f}, {1.f, .52f, .16f}, 9.f, 20.f);

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
    {
        Prop("Power generator", {x, 0, -26}, generator_, {1, 1, 1});
        emissiveStrip("Generator status bar", {x, 1.7f, -24.8f}, {1.2f, .12f, .08f});
        pointLight("Generator local glow", {x, 1.8f, -24.5f}, {.08f, .65f, 1.f}, 3.5f, 7.f);
    }
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

    // Control room: concentrated emissive surfaces against a dark/metallic shell.
    for (float x : {-7.f, 7.f})
    {
        Box("Console", {x, .7f, -27}, {3, 1.4f, 2}, 2);
        Box("Monitor", {x, 1.7f, -27.6f}, {2, 1, .15f}, 6);
        emissiveStrip("Console light rail", {x, 1.15f, -25.95f}, {2.5f, .08f, .08f});
        pointLight("Console screen spill", {x, 1.8f, -26.5f}, {.08f, .62f, 1.f}, 2.8f, 6.f);
    }
    door_ = Box("Control sliding door", {0, 1.5f, -20}, {3.8f, 3, .35f}, 2);
    auto d = world_->Read(door_).physics.value();
    d.body.motion = MotionType::Kinematic;
    d.body.layer = CollisionLayer::Dynamic;
    world_->SetPhysics(door_, d);
    emissiveStrip("Control door header", {0, 3.25f, -20}, {3.9f, .12f, .12f});

    // Firing range: alternating key/fill makes target silhouettes and shadow edges obvious.
    for (float x : {-9.f, 9.f})
        Box("Range cover", {x, .75f, -8}, {6, 1.5f, 1}, 1);
    for (float x : {-8.f, -4.f, 0.f, 4.f, 8.f})
    {
        auto target = Box("Reactive range target", {x, 1.7f, -16}, {.8f, 1.5f, .3f}, 4);
        targets_[target.value] = {};
        Box("Target stand", {x, .45f, -16}, {.2f, .9f, .3f}, 2);
        spotLight("Range target spot", {x, 4.5f, -12}, {0, -.7f, -.7f}, {1.f, .82f, .55f}, 4.5f, 12.f, 18, 30);
    }

    // Tower: intentionally thin geometry for high-contrast cascaded-shadow inspection.
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
    spotLight("Tower searchlight", {0, 8.2f, 6}, {.15f, -.5f, -.85f}, {.72f, .86f, 1.f}, 12.f, 55.f, 14, 26);

    // Tunnel/lab: darkest zone, dominated by local HDR-scale coloured lights and emissives.
    for (int i = 0; i < 20; ++i)
        Box("Tunnel stair", {0, -.2f * (i + 1) - .15f, -34.5f - .55f * i}, {4, .3f, .6f}, 0);
    Box("Tunnel west", {-2.3f, -.2f, -40}, {.5f, 8, 12}, 1);
    Box("Tunnel east", {2.3f, -.2f, -40}, {.5f, 8, 12}, 1);
    for (float z : {-37.f, -42.f, -47.f})
    {
        emissiveStrip("Tunnel guide strip", {0, 2.4f, z}, {2.8f, .08f, .08f});
        pointLight("Tunnel cyan pool", {0, 2.1f, z}, {.05f, .35f, 1.f}, 3.2f, 9.f);
    }
    room("Lower lab", 0, -54, 24, 18, -4);
    partition("Lab rooms", 0, -54, 24, 18, -4);
    pointLight("Lower lab blue key", {-7, -.2f, -54}, {.08f, .3f, 1.f}, 7.5f, 18.f);
    pointLight("Lower lab hazard red", {8, -.5f, -54}, {1.f, .06f, .02f}, 8.5f, 16.f);
    spotLight("Lower lab examination spot", {0, .8f, -54}, {0, -1, 0}, {.55f, .78f, 1.f}, 10.f, 16.f, 16, 28);
    for (float x : {-8.f, 8.f})
    {
        Prop("Generator", {x, -4, -54}, generator_, {1, 1, 1});
        emissiveStrip("Lab generator strip", {x, -2.3f, -52.8f}, {1.2f, .1f, .08f});
    }
    Box("Live electrical floor", {8, -3.95f, -54}, {4, .1f, 5}, 4, false);
    emissiveStrip("Electrical warning rail", {8, -3.65f, -56.4f}, {4, .12f, .12f});

    Box("Supply station", {-7, 1, 28}, {2, 2, 1}, 5);
    emissiveStrip("Supply station display", {-7, 1.4f, 28.55f}, {1.2f, .6f, .08f});
    pointLight("Supply station glow", {-7, 1.7f, 29}, {.12f, 1.f, .38f}, 2.8f, 7.f);
    for (float x : {-15.f, 15.f})
    {
        Box("Canopy", {x, 4, 18}, {12, .3f, 8}, 2);
        for (float z : {14.5f, 21.5f})
            Box("Canopy pillar", {x, 2, z}, {.4f, 4, .4f}, 2);
        Prop("Crate", {x, 0, 26}, crate_, {.7f, .5f, .5f});
        spotLight("Canopy downlight", {x, 3.7f, 18}, {0, -1, 0}, {1.f, .72f, .42f}, 5.f, 13.f);
    }

    auto sun = world_->Create("Late afternoon sun");
    LightDesc light;
    light.type = LightType::Directional;
    light.direction = {-.6f, -1, -.4f};
    light.colour = {1, .78f, .55f};
    light.power = 3.8f;
    world_->SetLight(sun, {light});
}
} // namespace Game
