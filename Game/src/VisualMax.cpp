#include "pgcpch.h"
#include "TPSGame.h"
using namespace PlagC;
namespace Game
{
void TPSGame::InstallVisualMaxMaterials(Engine &e)
{
    auto &r = e.Graphics();
    struct Surface
    {
        const char *prefix;
        Vec3 tint;
        float roughness;
        float metallic;
        float normalScale;
    };
    // Keep the seven material slots used by Facility.cpp, but replace the generated 32x32
    // placeholders with authored CC0 Poly Haven PBR maps.  Diffuse is sRGB; data maps are linear.
    const Surface surfaces[] = {
        {"asphalt_03", {1, 1, 1}, .82f, 0.f, 1.f},
        {"concrete_floor", {.72f, .75f, .78f}, .78f, 0.f, 1.f},
        {"corrugated_iron", {.52f, .58f, .62f}, .48f, .82f, 1.15f},
        {"concrete_floor", {.58f, .64f, .60f}, .70f, 0.f, .85f},
        {"rusty_painted_metal", {.95f, .48f, .24f}, .58f, .62f, 1.1f},
        {"gravel_embedded_concrete", {.42f, .82f, .52f}, .86f, 0.f, 1.2f},
    };

    for (unsigned i = 0; i < std::size(surfaces); ++i)
    {
        const auto &s = surfaces[i];
        std::string base = s.prefix;
        auto albedo = r.LoadTexture(base + "_diff_1k.jpg", true, true);
        auto normal = r.LoadTexture(base + "_normal_1k.jpg", false, true);
        auto rough = r.LoadTexture(base + "_rough_1k.jpg", false, true);
        textures_.push_back(albedo);
        textures_.push_back(normal);
        textures_.push_back(rough);
        MaterialDesc m;
        m.colour = s.tint;
        m.roughness = s.roughness;
        m.metallic = s.metallic;
        m.normalScale = s.normalScale;
        m.baseColourTexture = albedo;
        m.normalTexture = normal;
        m.roughnessTexture = rough;
        r.UpdateMaterial(materials_.at(i), m);
    }

    // Slot 6 is intentionally HDR-bright and untextured: it is used for screens, guide rails,
    // status lamps and the muzzle flash, allowing the filmic bloom pass to respond strongly.
    MaterialDesc emissive;
    emissive.colour = {.025f, .12f, .18f};
    emissive.roughness = .24f;
    emissive.metallic = .12f;
    emissive.emissive = {.35f, 3.8f, 7.5f};
    r.UpdateMaterial(materials_.at(6), emissive);
}
} // namespace Game
