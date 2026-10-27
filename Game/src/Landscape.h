#pragma once
#include <PlagC/World/World.h>
#include <array>
namespace Game
{
// One deterministic height source for rendering, collision and placement.
struct LandscapeSurface
{
    static float Height(float x, float z);
    static float RoadDistance(float x, float z);
    static bool Facility(float x, float z);
    static bool Buildable(float x, float z);
    static PlagC::MeshData Tile(float x, float z, unsigned stride);
};
class Landscape
{
    PlagC::Renderer &renderer_;
    std::vector<PlagC::MeshHandle> meshes_;
    std::vector<PlagC::MaterialHandle> materials_;
    std::vector<PlagC::TextureHandle> textures_;
    struct Detail
    {
        PlagC::Transform transform;
        std::array<PlagC::MeshHandle, 3> meshes;
        PlagC::MaterialHandle material;
        PlagC::ObjectHandle object;
        float radius = 0, distance = 0;
        unsigned lod = 3;
        bool shadow = true;
        PlagC::Vec3 center;
    };
    std::vector<Detail> details_;
    float updateTime_ = 0;
    unsigned quality_ = 1;
    PlagC::MeshHandle Mesh(PlagC::MeshData);
    PlagC::MaterialHandle Material(PlagC::Vec3, float = .9f);
    void DetailObject(const PlagC::Transform &, std::array<PlagC::MeshHandle, 3>,
                      PlagC::MaterialHandle, float radius, float distance, bool shadows = true);

  public:
    explicit Landscape(PlagC::Renderer &r) : renderer_(r)
    {
    }
    ~Landscape();
    void Build(PlagC::World &);
    void Update(const PlagC::CameraDesc &, double dt);
    void CycleQuality()
    {
        quality_ = (quality_ + 1) % 3;
        updateTime_ = 0;
    }
    const char *QualityName() const
    {
        return quality_ == 0 ? "LOW" : quality_ == 1 ? "BALANCED" : "HIGH";
    }
};
} // namespace Game
