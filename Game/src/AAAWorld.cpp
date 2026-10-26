#include "pgcpch.h"
#include "TPSGame.h"
using namespace PlagC;
namespace Game
{
namespace
{
// The provider's JPEG glTF fence loses wire opacity. Use its authored metal posts
// with real opaque wire geometry, rather than render an unsupported alpha card.
MeshData ChainlinkPanel()
{
    MeshData mesh;
    constexpr float pi = 3.14159265359f;
    auto wire = [&](Vec3 a, Vec3 b) {
        Vec3 d{b.x - a.x, b.y - a.y, 0};
        float length = std::sqrt(d.x * d.x + d.y * d.y);
        if (length < .01f)
            return;
        d = {d.x / length, d.y / length, 0};
        Vec3 side{-d.y, d.x, 0};
        unsigned base = unsigned(mesh.vertices.size());
        for (int end = 0; end < 2; ++end)
            for (unsigned i = 0; i < 6; ++i)
            {
                float angle = 2 * pi * float(i) / 6;
                Vec3 n{side.x * std::sin(angle), side.y * std::sin(angle), std::cos(angle)};
                auto p = end ? b : a;
                p = {p.x + n.x * .008f, p.y + n.y * .008f, p.z + n.z * .008f};
                Vec3 tangent{side.x * std::cos(angle), side.y * std::cos(angle), -std::sin(angle)};
                mesh.vertices.push_back({p, n, float(i) / 6, float(end), tangent, -1});
            }
        for (unsigned i = 0; i < 6; ++i)
        {
            auto next = (i + 1) % 6;
            mesh.indices.insert(mesh.indices.end(), {base + i, base + 6 + i, base + next,
                                                     base + next, base + 6 + i, base + 6 + next});
        }
    };
    for (float intercept = -4; intercept <= 7; intercept += .22f)
    {
        float lo = std::max(-3.f, .3f - intercept), hi = std::min(3.f, 2.7f - intercept);
        if (lo < hi)
            wire({lo, lo + intercept, 0}, {hi, hi + intercept, 0});
        lo = std::max(-3.f, intercept - 2.7f);
        hi = std::min(3.f, intercept - .3f);
        if (lo < hi)
            wire({lo, -lo + intercept, 0}, {hi, -hi + intercept, 0});
    }
    return mesh;
}
} // namespace
void TPSGame::LoadAAAWorldAssets(Engine &e)
{
    auto L = [&](const char *p) { return e.Assets().Load<ModelAsset>(p); };
    if (auto a = L("TPS/AAA/Crate.plasset"))
        crate_ = a;
    if (auto a = L("TPS/AAA/Generator.plasset"))
        generator_ = a;
    securityCamera_ = L("TPS/AAA/SecurityCamera.plasset");
    securityCamera02_ = L("TPS/AAA/SecurityCamera02.plasset");
    factoryFacade_ = L("TPS/AAA/FactoryFacade.plasset");
    industrialPipes_ = L("TPS/AAA/IndustrialPipes.plasset");
    modularPipes_ = L("TPS/AAA/ModularPipes.plasset");
    steelShelves_ = L("TPS/AAA/SteelShelves.plasset");
    exteriorAircon_ = L("TPS/AAA/ExteriorAircon.plasset");
    utilityBox_ = L("TPS/AAA/UtilityBox.plasset");
    barrel_ = L("TPS/AAA/Barrel.plasset");
    plasticCrate_ = L("TPS/AAA/PlasticCrate.plasset");
    airduct_ = L("TPS/AAA/Airduct.plasset");
    circularAirduct_ = L("TPS/AAA/CircularAirduct.plasset");
    electricCables_ = L("TPS/AAA/ElectricCables.plasset");
    chainlinkFence_ = L("TPS/AAA/ChainlinkFence.plasset");
    roadBarrier_ = L("TPS/AAA/RoadBarrier.plasset");
    jerrycan_ = L("TPS/AAA/Jerrycan.plasset");
    wallLamp_ = L("TPS/AAA/WallLamp.plasset");
    cagedSconce_ = L("TPS/AAA/CagedSconce.plasset");
    fluorescentLights_ = L("TPS/AAA/FluorescentLights.plasset");
    fireAlarm_ = L("TPS/AAA/FireAlarm.plasset");
    toolbox_ = L("TPS/AAA/Toolbox.plasset");
    handTruck_ = L("TPS/AAA/HandTruck.plasset");
}
void TPSGame::DecorateAAAWorld(Engine &e)
{
    auto P = [&](const char *n, Vec3 p, const AssetHandle<ModelAsset> &a, Vec3 h) {
        if (a)
            Prop(n, p, a, h);
    };
    auto light = [&](Vec3 p, Vec3 c, float power, float radius) {
        LightDesc l;
        l.type = LightType::Point;
        l.position = p;
        l.colour = c;
        l.power = power;
        l.radius = radius;
        auto entity = world_->Create("Industrial practical light");
        world_->SetTransform(entity, {p});
        world_->SetLight(entity, {l});
    };
    // Exterior: break the box silhouette with authored facade, perimeter security, service
    // infrastructure and foreground clutter.
    P("facade west", {-39, 0, -23}, factoryFacade_, {2.5f, 4.5f, 7});
    P("facade east", {39, 0, -23}, factoryFacade_, {2.5f, 4.5f, 7});
    fenceWire_ = e.Graphics().CreateMesh(ChainlinkPanel());
    for (float x : {-31.f, -20.f, 20.f, 31.f})
    {
        for (float side : {-1.f, 1.f})
            P("perimeter fence post", {x + side * 3, 0, 36}, chainlinkFence_, {.15f, 1.5f, .15f});
        for (float y : {.3f, 2.7f})
            Box("fence rail", {x, y, 36}, {6, .06f, .06f}, 2, false);
        auto entity = world_->Create("chainlink wire panel");
        world_->SetTransform(entity, {{x, 0, 36}});
        world_->SetRenderable(entity, {fenceWire_, materials_[2], true, false});
        auto barrier = world_->Create("fence collision proxy");
        world_->SetTransform(barrier, {{x, 1.5f, 36}});
        PhysicsComponent physics;
        physics.body.motion = MotionType::Static;
        physics.body.layer = CollisionLayer::StaticWorld;
        physics.collider.dimensions = {3, 1.5f, .04f};
        world_->SetPhysics(barrier, physics);
    }
    for (float x : {-28.f, -14.f, 14.f, 28.f})
        P("road barrier", {x, 0, 29}, roadBarrier_, {1.6f, .7f, .5f});
    P("gate CCTV", {-8, 3.8f, 39.7f}, securityCamera_, {.5f, .4f, .5f});
    P("tower CCTV", {3.8f, 7.2f, 5}, securityCamera02_, {.5f, .4f, .5f});
    P("gate utility", {-35, 0, 31}, utilityBox_, {.7f, 1.2f, .5f});
    for (Vec3 p : {Vec3{-37, 2.1f, -5}, Vec3{10, 2.1f, -27}, Vec3{36, 2.1f, -8}})
        P("exterior AC", p, exteriorAircon_, {.7f, .7f, .4f});
    // Warehouse: ceiling services, shelves, logistics clutter and practical fixtures create several
    // depth layers.
    P("warehouse pipe manifold", {-34, 0, -4}, industrialPipes_, {2.5f, 1.8f, 1});
    P("warehouse duct", {-26, 3.8f, -9}, airduct_, {3, .8f, .8f});
    P("warehouse circular duct", {-22, 4.1f, 1}, circularAirduct_, {3, .8f, .8f});
    P("warehouse cable tray", {-29, 4.2f, 7}, electricCables_, {3, .3f, .5f});
    for (float z : {-9.f, 1.f, 9.f})
        P("warehouse shelving", {-32, 0, z}, steelShelves_, {2.2f, 2.2f, .7f});
    for (Vec3 p : {Vec3{-29, 0, -22}, Vec3{-23, 0, -22}, Vec3{22, 0, -21}, Vec3{30, 0, -21},
                   Vec3{-31, 0, 7}})
        P("industrial barrel", p, barrel_, {.55f, .9f, .55f});
    for (Vec3 p :
         {Vec3{-20, 0, 8}, Vec3{-24, 0, 8}, Vec3{20, 0, 8}, Vec3{24, 0, 8}, Vec3{-18, 0, -4}})
        P("stackable crate", p, plasticCrate_, {.55f, .45f, .45f});
    P("warehouse hand truck", {-27, 0, 11}, handTruck_, {.8f, 1.4f, .6f});
    P("warehouse toolbox", {-20, 0, -8}, toolbox_, {.8f, .5f, .5f});
    P("warehouse fire alarm", {-18, 2.1f, -11}, fireAlarm_, {.25f, .35f, .15f});
    // Generator/workshop: mechanically dense hero area.
    P("generator pipe bank", {34, 0, -29}, industrialPipes_, {2.5f, 1.8f, 1});
    P("generator secondary pipes", {28, 0, -31}, modularPipes_, {2.5f, 1.8f, 1});
    P("workshop duct", {-26, 3.8f, -27}, airduct_, {3, .8f, .8f});
    P("workshop cables", {-22, 3.7f, -29}, electricCables_, {3, .3f, .5f});
    P("workshop toolbox", {-31, 0, -28}, toolbox_, {.8f, .5f, .5f});
    P("generator jerrycan", {31, 0, -24}, jerrycan_, {.4f, .6f, .3f});
    // Control room: security hardware and believable practical lighting.
    P("control CCTV", {9, 3.3f, -18}, securityCamera02_, {.5f, .4f, .5f});
    P("control alarm", {13, 2.0f, -21}, fireAlarm_, {.25f, .35f, .15f});
    P("control fluorescent", {5, 3.7f, -22}, fluorescentLights_, {1.8f, .25f, .5f});
    // Lower lab: maximum normal/specular response under contrasting cyan/red practicals.
    P("lab utility", {-8, -4, -51}, utilityBox_, {.7f, 1.2f, .5f});
    P("lab pipe bank", {8, -4, -57}, industrialPipes_, {2.5f, 1.8f, 1});
    P("lab ceiling duct", {0, -.2f, -57}, airduct_, {3, .8f, .8f});
    P("lab cables", {-5, -.3f, -57}, electricCables_, {3, .3f, .5f});
    P("lab CCTV", {9, -.1f, -49}, securityCamera_, {.5f, .4f, .5f});
    P("lab caged lamp", {-8, -.1f, -55}, cagedSconce_, {.4f, .5f, .3f});
    P("lab toolbox", {6, -4, -52}, toolbox_, {.8f, .5f, .5f});
    // Practical point sources reinforce the actual fixture meshes and give HDR/bloom readable
    // highlights.
    for (Vec3 p : {Vec3{-29, 3.6f, -7}, Vec3{-29, 3.6f, 6}, Vec3{28, 3.6f, -27}})
        light(p, {.72f, .84f, 1.f}, 13, 8);
    light({5, 3.4f, -22}, {.45f, .8f, 1.f}, 12, 7);
    light({-8, -.2f, -55}, {.2f, .75f, 1.f}, 14, 7);
    light({8, -.2f, -55}, {1.f, .12f, .05f}, 8, 5);
}
} // namespace Game
