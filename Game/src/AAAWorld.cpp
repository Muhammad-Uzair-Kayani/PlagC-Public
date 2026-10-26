#include "pgcpch.h"
#include "TPSGame.h"
using namespace PlagC;
namespace Game
{
void TPSGame::LoadAAAWorldAssets(Engine &e)
{
    auto load = [&](const char *path) -> AssetHandle<ModelAsset> {
        const auto disk = std::filesystem::path("Assets/Cooked") / path;
        if (!std::filesystem::exists(disk))
            return {};
        return e.Assets().Load<ModelAsset>(path);
    };
    // These two are drop-in upgrades for props already placed by Facility.cpp.
    if (auto a = load("TPS/Crate.plasset")) crate_ = a;
    if (auto a = load("TPS/Generator.plasset")) generator_ = a;
    securityCamera_ = load("TPS/AAA/SecurityCamera.plasset");
    factoryFacade_ = load("TPS/AAA/FactoryFacade.plasset");
    industrialPipes_ = load("TPS/AAA/IndustrialPipes.plasset");
    steelShelves_ = load("TPS/AAA/SteelShelves.plasset");
    exteriorAircon_ = load("TPS/AAA/ExteriorAircon.plasset");
    utilityBox_ = load("TPS/AAA/UtilityBox.plasset");
    barrel_ = load("TPS/AAA/Barrel.plasset");
    plasticCrate_ = load("TPS/AAA/PlasticCrate.plasset");
    airduct_ = load("TPS/AAA/Airduct.plasset");
}

void TPSGame::DecorateAAAWorld(Engine &)
{
    auto prop = [&](const char *name, Vec3 p, const AssetHandle<ModelAsset> &asset, Vec3 half) {
        if (asset) Prop(name, p, asset, half);
    };
    // Exterior silhouette and security storytelling.
    prop("AAA factory facade west", {-39, 0, -23}, factoryFacade_, {2.5f, 4.5f, 7});
    prop("AAA factory facade east", {39, 0, -23}, factoryFacade_, {2.5f, 4.5f, 7});
    prop("AAA CCTV gate", {-8, 4.7f, 34}, securityCamera_, {.5f, .4f, .5f});
    prop("AAA CCTV tower", {3.8f, 7.2f, 5}, securityCamera_, {.5f, .4f, .5f});
    prop("AAA utility box gate", {-35, 0, 31}, utilityBox_, {.7f, 1.2f, .5f});
    prop("AAA exterior AC west", {-37, 2.1f, -5}, exteriorAircon_, {.7f, .7f, .4f});
    prop("AAA exterior AC control", {10, 2.1f, -27}, exteriorAircon_, {.7f, .7f, .4f});

    // Warehouse/workshop: replace the primitive-only read with dense industrial secondary detail.
    prop("AAA pipe manifold warehouse", {-34, 0, -4}, industrialPipes_, {2.5f, 1.8f, 1});
    prop("AAA pipe manifold generator", {34, 0, -29}, industrialPipes_, {2.5f, 1.8f, 1});
    prop("AAA airduct warehouse", {-26, 3.8f, -9}, airduct_, {3, .8f, .8f});
    prop("AAA airduct workshop", {-26, 3.8f, -27}, airduct_, {3, .8f, .8f});
    for (float z : {-9.f, 1.f, 9.f})
        prop("AAA warehouse shelving", {-32, 0, z}, steelShelves_, {2.2f, 2.2f, .7f});
    for (Vec3 p : {Vec3{-29,0,-22}, Vec3{-23,0,-22}, Vec3{22,0,-21}, Vec3{30,0,-21}})
        prop("AAA industrial barrel", p, barrel_, {.55f, .9f, .55f});
    for (Vec3 p : {Vec3{-20,0,8}, Vec3{-24,0,8}, Vec3{20,0,8}, Vec3{24,0,8}})
        prop("AAA stackable crate", p, plasticCrate_, {.55f, .45f, .45f});

    // Underground lab gets the densest dressing to showcase specular/normal detail in local HDR light.
    prop("AAA lab utility box", {-8, -4, -51}, utilityBox_, {.7f, 1.2f, .5f});
    prop("AAA lab pipe bank", {8, -4, -57}, industrialPipes_, {2.5f, 1.8f, 1});
    prop("AAA lab ceiling duct", {0, -.2f, -57}, airduct_, {3, .8f, .8f});
    prop("AAA lab CCTV", {9, -.1f, -49}, securityCamera_, {.5f, .4f, .5f});
}
} // namespace Game
