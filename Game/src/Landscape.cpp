#include "pgcpch.h"
#include "Landscape.h"
using namespace PlagC;
namespace Game
{
namespace
{
constexpr float Pi = 3.14159265359f;
float Smooth(float a, float b, float x)
{
    float t = std::clamp((x - a) / (b - a), 0.f, 1.f);
    return t * t * (3 - 2 * t);
}
std::uint32_t Hash(std::uint32_t n)
{
    n ^= n >> 16;
    n *= 0x7feb352du;
    n ^= n >> 15;
    n *= 0x846ca68bu;
    return n ^ (n >> 16);
}
float Random(std::uint32_t &seed)
{
    seed = Hash(seed + 0x9e3779b9u);
    return float(seed & 0xffffff) / 16777215.f;
}
float Noise(float x, float z)
{
    int ix = int(std::floor(x)), iz = int(std::floor(z));
    float fx = Smooth(0, 1, x - float(ix)), fz = Smooth(0, 1, z - float(iz));
    auto h = [](int a, int b) {
        return float(Hash(std::uint32_t(a) * 73856093u ^ std::uint32_t(b) * 19349663u) & 65535) /
               65535.f;
    };
    return std::lerp(std::lerp(h(ix, iz), h(ix + 1, iz), fx),
                     std::lerp(h(ix, iz + 1), h(ix + 1, iz + 1), fx), fz);
}
Vec3 Normal(Vec3 v)
{
    float l = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    return l > .0001f ? Vec3{v.x / l, v.y / l, v.z / l} : Vec3{0, 1, 0};
}
void Triangle(MeshData &mesh, Vec3 a, Vec3 b, Vec3 c, float uvScale = 1)
{
    Vec3 u{b.x - a.x, b.y - a.y, b.z - a.z}, v{c.x - a.x, c.y - a.y, c.z - a.z};
    Vec3 n = Normal({u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x});
    Vec3 tangent = Normal({n.y, -n.x, 0});
    unsigned base = unsigned(mesh.vertices.size());
    for (auto p : {a, b, c})
        mesh.vertices.push_back({p, n, p.x * uvScale, p.z * uvScale, tangent});
    mesh.indices.insert(mesh.indices.end(), {base, base + 1, base + 2});
}
// Crossed branch cards with cutout needle texture. Near/far meshes share the same material.
MeshData Pine(unsigned sides, unsigned layers)
{
    MeshData mesh;
    for (unsigned layer = 0; layer < layers; ++layer)
        for (unsigned side = 0; side < sides; ++side)
        {
            float y = .8f + float(layer) * 6 / float(layers);
            float a = float(side) * 2 * Pi / float(sides) + float(layer) * .63f;
            float length = 2.6f * std::pow(1 - float(layer) / float(layers + 1), .8f);
            Vec3 axis{std::cos(a), .15f, std::sin(a)};
            for (unsigned cross = 0; cross < 2; ++cross)
            {
                Vec3 wide =
                    cross ? Vec3{0, .34f * length, 0}
                          : Vec3{-std::sin(a) * .36f * length, 0, std::cos(a) * .36f * length};
                Vec3 n = cross ? Vec3{-std::sin(a), 0, std::cos(a)} : Vec3{0, 1, 0};
                unsigned base = unsigned(mesh.vertices.size());
                for (unsigned end = 0; end < 2; ++end)
                    for (float sign : {-1.f, 1.f})
                    {
                        Vec3 p{axis.x * length * float(end) + wide.x * sign,
                               y + axis.y * length * float(end) + wide.y * sign,
                               axis.z * length * float(end) + wide.z * sign};
                        mesh.vertices.push_back({p, n, (sign + 1) * .5f, float(end), axis});
                    }
                mesh.indices.insert(mesh.indices.end(),
                                    {base, base + 2, base + 1, base + 1, base + 2, base + 3});
            }
        }
    return mesh;
}
MeshData Rock(unsigned rings, unsigned sides)
{
    MeshData mesh;
    auto p = [&](unsigned ring, unsigned side) {
        float a = float(side) * 2 * Pi / float(sides), b = float(ring) * Pi / float(rings);
        Vec3 v{std::sin(b) * std::cos(a), std::cos(b), std::sin(b) * std::sin(a)};
        float r = .85f + .22f * Noise(v.x * 3 + 9, v.z * 3 + v.y * 2);
        return Vec3{v.x * r * 1.4f, v.y * r * .75f + .4f, v.z * r};
    };
    for (unsigned j = 0; j < rings; ++j)
        for (unsigned i = 0; i < sides; ++i)
        {
            if (j)
                Triangle(mesh, p(j, i), p(j, i + 1), p(j + 1, i), .25f);
            if (j + 1 < rings)
                Triangle(mesh, p(j, i + 1), p(j + 1, i + 1), p(j + 1, i), .25f);
        }
    for (auto &v : mesh.vertices)
    {
        v.normal = Normal(
            {v.position.x / (1.4f * 1.4f), (v.position.y - .4f) / (.75f * .75f), v.position.z});
        v.tangent = Normal({v.normal.y, -v.normal.x, 0});
    }
    return mesh;
}
} // namespace
bool LandscapeSurface::Facility(float x, float z)
{
    return std::abs(x) < 44 && z > -63 && z < 42;
}
bool LandscapeSurface::Buildable(float x, float z)
{
    for (float hx : {-68.f, 66.f})
        for (float hz : {59.f, 88.f, 117.f})
            if (std::abs(x - hx) < 12 && std::abs(z - hz) < 11)
                return true;
    return false;
}
float LandscapeSurface::RoadDistance(float x, float z)
{
    float center = 5 * std::sin(std::max(0.f, z - 45) * .022f);
    float d = std::abs(x - center);
    for (float branch : {60.f, 90.f, 120.f})
        if (std::abs(x) < 80)
            d = std::min(d, std::abs(z - branch));
    return d;
}
float LandscapeSurface::Height(float x, float z)
{
    float outside = std::max(std::abs(x) - 44, std::max(-63 - z, z - 42));
    float ridge = std::pow(std::abs(Noise(x * .008f + 33, z * .008f + 15) - .5f) * 2, 1.4f);
    float h = 1.5f + 15 * Noise(x * .018f, z * .018f) + 24 * ridge;
    h += 2.2f * Noise(x * .09f, z * .09f);
    h += 36 * Smooth(145, 245, std::max(std::abs(x), std::abs(z))) * ridge;
    h *= Smooth(0, 35, outside);
    // Facility and settlement are deliberately graded pads; their blend avoids cliffs.
    for (float hx : {-68.f, 66.f})
        for (float hz : {59.f, 88.f, 117.f})
        {
            float d = std::max(std::abs(x - hx) - 11, std::abs(z - hz) - 10);
            h *= Smooth(0, 12, d);
        }
    if (z > 40 && z < 155)
        h *= .28f + .72f * Smooth(4, 16, RoadDistance(x, z));
    return h;
}
MeshData LandscapeSurface::Tile(float x, float z, unsigned stride)
{
    MeshData mesh;
    constexpr unsigned size = 32;
    if (!stride || size % stride || !std::isfinite(x) || !std::isfinite(z))
        throw std::invalid_argument("invalid landscape tile parameters");
    for (unsigned j = 0; j <= size; j += stride)
        for (unsigned i = 0; i <= size; i += stride)
        {
            float wx = x + float(i), wz = z + float(j), h = Height(wx, wz);
            auto n = Normal({Height(wx - .5f, wz) - Height(wx + .5f, wz), 1,
                             Height(wx, wz - .5f) - Height(wx, wz + .5f)});
            mesh.vertices.push_back({{wx, h, wz}, n, wx * .13f, wz * .13f, Normal({n.y, -n.x, 0})});
        }
    unsigned count = size / stride + 1;
    for (unsigned j = 0; j + 1 < count; ++j)
        for (unsigned i = 0; i + 1 < count; ++i)
        {
            float wx = x + (float(i) + .5f) * float(stride),
                  wz = z + (float(j) + .5f) * float(stride);
            // Keep the existing underground tunnel/lab open. All pads inside this hole have floors.
            if ((std::abs(wx) < 42 && wz > -34 && wz < 40) ||
                (std::abs(wx) < 13 && wz >= -64 && wz <= -44) ||
                (std::abs(wx) < 3 && wz >= -46 && wz <= -34))
                continue;
            unsigned a = j * count + i, b = a + 1, c = a + count, d = c + 1;
            mesh.indices.insert(mesh.indices.end(), {a, c, b, b, c, d});
        }
    // Lower skirts conceal cracks where neighboring chunks select different LOD levels.
    for (unsigned edge = 0; edge < 4; ++edge)
        for (unsigned i = 0; i + 1 < count; ++i)
        {
            unsigned a = edge == 0   ? i
                         : edge == 1 ? (count - 1) * count + i
                         : edge == 2 ? i * count
                                     : i * count + count - 1;
            unsigned b = a + (edge < 2 ? 1 : count);
            auto va = mesh.vertices[a], vb = mesh.vertices[b];
            unsigned c = unsigned(mesh.vertices.size());
            va.position.y -= 3;
            vb.position.y -= 3;
            mesh.vertices.push_back(va);
            mesh.vertices.push_back(vb);
            mesh.indices.insert(mesh.indices.end(), {a, b, c, b, c + 1, c, c, b, a, c, c + 1, b});
        }
    return mesh;
}
MeshHandle Landscape::Mesh(MeshData data)
{
    auto mesh = renderer_.CreateMesh(data);
    meshes_.push_back(mesh);
    return mesh;
}
MaterialHandle Landscape::Material(Vec3 colour, float roughness)
{
    MaterialDesc d;
    d.colour = colour;
    d.roughness = roughness;
    auto m = renderer_.CreateMaterial(d);
    materials_.push_back(m);
    return m;
}
void Landscape::DetailObject(const Transform &t, std::array<MeshHandle, 3> meshes,
                             MaterialHandle material, float radius, float distance, bool shadow)
{
    details_.push_back({t, meshes, material, {}, radius, distance, 3, shadow, t.position});
}
Landscape::~Landscape()
{
    for (auto &d : details_)
        if (d.object)
            renderer_.DestroyObject(d.object);
    for (auto m : meshes_)
        renderer_.DestroyMesh(m);
    for (auto m : materials_)
        renderer_.DestroyMaterial(m);
    for (auto t : textures_)
        renderer_.DestroyTexture(t);
}
void Landscape::Build(World &world)
{
    auto ground = Material({.28f, .34f, .16f}), stone = Material({.30f, .33f, .29f});
    auto pine = Material({.075f, .18f, .095f}), bark = Material({.16f, .105f, .065f});
    auto grass = Material({.24f, .32f, .11f}), road = Material({.24f, .20f, .145f});
    std::vector<std::uint8_t> needles(128 * 256 * 4);
    auto line = [&](int ax, int ay, int bx, int by, float brightness) {
        int count = std::max(std::abs(bx - ax), std::abs(by - ay));
        for (int i = 0; i <= count; ++i)
        {
            float t = count ? float(i) / float(count) : 0;
            int x = int(std::lerp(float(ax), float(bx), t)),
                y = int(std::lerp(float(ay), float(by), t));
            if (x < 0 || x >= 128 || y < 0 || y >= 256)
                continue;
            auto offset = std::size_t(y * 128 + x) * 4;
            needles[offset] = std::uint8_t(100 * brightness);
            needles[offset + 1] = std::uint8_t(155 * brightness);
            needles[offset + 2] = std::uint8_t(65 * brightness);
            needles[offset + 3] = 255;
        }
    };
    line(64, 4, 64, 252, .7f);
    for (int y = 12; y < 249; y += 3)
        for (int side : {-1, 1})
        {
            float taper = std::sin(float(y) / 256 * Pi) * .85f + .1f;
            int width = int(54 * taper);
            line(64, y, 64 + side * width, y + 9 + (y % 11), .65f + float(y % 17) * .035f);
            for (int branch = 1; branch < 5; ++branch)
            {
                int x = 64 + side * width * branch / 5;
                line(x, y + branch * 2, x - side * 8, y + branch * 2 + 12, .75f);
            }
        }
    auto needleTexture = renderer_.CreateTexture(128, 256, needles, true, true);
    textures_.push_back(needleTexture);
    MaterialDesc foliage;
    foliage.colour = {1, 1, 1};
    foliage.roughness = .95f;
    foliage.baseColourTexture = needleTexture;
    foliage.emissive = {.012f, .026f, .005f};
    foliage.alphaCutoff = .15f;
    foliage.doubleSided = true;
    renderer_.UpdateMaterial(pine, foliage);

    // Reuse authored grass/rock maps when supplied by the mandatory landscape build.
#ifdef PC_TPS_VISUAL_MAX
    auto texture = [&](const char *name, bool srgb) {
        auto t = renderer_.LoadTexture(name, srgb, true);
        textures_.push_back(t);
        return t;
    };
    MaterialDesc d;
    d.colour = {.64f, .7f, .50f};
    d.roughness = .95f;
    d.baseColourTexture = texture("Landscape_Grass_Color.jpg", true);
    d.normalTexture = texture("Landscape_Grass_Normal.jpg", false);
    d.roughnessTexture = texture("Landscape_Grass_Roughness.jpg", false);
    renderer_.UpdateMaterial(ground, d);
    d.colour = {.55f, .51f, .44f};
    d.normalTexture = {};
    d.roughnessTexture = {};
    d.baseColourTexture = texture("gravel_embedded_concrete_diff_1k.jpg", true);
    renderer_.UpdateMaterial(stone, d);
#endif
    for (int z = -256; z < 256; z += 32)
        for (int x = -256; x < 256; x += 32)
        {
            auto high = LandscapeSurface::Tile(float(x), float(z), 1);
            if (high.indices.empty())
                continue;
            auto geometry = std::make_shared<ColliderGeometry>();
            // Same triangles as the highest-detail surface; omit visual skirts from collision.
            const unsigned surfaceVertices = 33 * 33;
            for (unsigned i = 0; i < surfaceVertices; ++i)
                geometry->vertices.push_back(high.vertices[i].position);
            for (std::size_t i = 0; i < high.indices.size(); i += 3)
                if (high.indices[i] < surfaceVertices && high.indices[i + 1] < surfaceVertices &&
                    high.indices[i + 2] < surfaceVertices)
                    geometry->indices.insert(
                        geometry->indices.end(),
                        {high.indices[i], high.indices[i + 1], high.indices[i + 2]});
            if (geometry->indices.empty())
                continue;
            if (!geometry->indices.empty())
            {
                auto e = world.Create("Landscape collision tile");
                PhysicsComponent p;
                p.body.motion = MotionType::Static;
                p.body.layer = CollisionLayer::StaticWorld;
                p.collider.type = ColliderType::Mesh;
                p.collider.geometry = geometry;
                p.collider.material.friction = .8f;
                world.SetPhysics(e, p);
            }
            auto a = Mesh(std::move(high)), b = Mesh(LandscapeSurface::Tile(float(x), float(z), 2));
            auto c = Mesh(LandscapeSurface::Tile(float(x), float(z), 4));
            DetailObject({}, {a, b, c}, ground, 30, 650);
            details_.back().center = {float(x) + 16, 0, float(z) + 16};
        }
    // Dirt road follows exactly the sampled terrain; widened shoulders blend into grass.
    MeshData paths;
    for (int z = 42; z < 162; ++z)
        for (int x = -82; x < 82; ++x)
        {
            float wx = float(x) + .5f, wz = float(z) + .5f;
            float center = 5 * std::sin(std::max(0.f, wz - 45) * .022f);
            bool lane = std::abs(wx - center) < 4;
            for (float cross : {60.f, 90.f, 120.f})
                lane = lane || (std::abs(wz - cross) < 2.6f && std::abs(wx) < 80);
            if (!lane)
                continue;
            Vec3 p{float(x), LandscapeSurface::Height(float(x), float(z)) + .035f, float(z)};
            Vec3 q{float(x + 1), LandscapeSurface::Height(float(x + 1), float(z)) + .035f,
                   float(z)};
            Vec3 r{float(x), LandscapeSurface::Height(float(x), float(z + 1)) + .035f,
                   float(z + 1)};
            Vec3 t{float(x + 1), LandscapeSurface::Height(float(x + 1), float(z + 1)) + .035f,
                   float(z + 1)};
            Triangle(paths, p, r, q, .25f);
            Triangle(paths, q, r, t, .25f);
        }
    auto pathMesh = Mesh(std::move(paths));
    DetailObject({}, {pathMesh, pathMesh, pathMesh}, road, 100, 350, false);
    std::array<MeshHandle, 3> trees{Mesh(Pine(12, 16)), Mesh(Pine(8, 10)), Mesh(Pine(6, 6))};
    std::array<MeshHandle, 3> rocks{Mesh(Rock(20, 28)), Mesh(Rock(10, 16)), Mesh(Rock(6, 10))};
    auto trunk = Mesh(MakePrimitive(Primitive::Cylinder));
    std::uint32_t seed = 0x4f555450;
    for (unsigned i = 0; i < 1500; ++i)
    {
        float x = -180 + Random(seed) * 360, z = -180 + Random(seed) * 360;
        if (LandscapeSurface::Facility(x, z) || LandscapeSurface::Buildable(x, z) ||
            (z > 38 && z < 166 && LandscapeSurface::RoadDistance(x, z) < 8))
            continue;
        float slope =
            std::hypot(LandscapeSurface::Height(x + 1, z) - LandscapeSurface::Height(x - 1, z),
                       LandscapeSurface::Height(x, z + 1) - LandscapeSurface::Height(x, z - 1)) *
            .5f;
        bool tree = i % 3 != 0 && slope < .7f;
        float s = tree ? .8f + Random(seed) * .9f : 1.1f + Random(seed) * 3.2f;
        float y = LandscapeSurface::Height(x, z), yaw = Random(seed) * 360;
        if (tree)
        {
            DetailObject({{x, y, z}, {0, yaw, 0}, {s, s, s}}, trees, pine, 8 * s, 250);
            DetailObject({{x, y + 2 * s, z}, {}, {.32f * s, 4 * s, .32f * s}},
                         {trunk, trunk, trunk}, bark, 5 * s, 180);
            auto e = world.Create("Pine trunk collision");
            world.SetTransform(e, {{x, y + 2 * s, z}, {}, {1, 1, 1}});
            PhysicsComponent p;
            p.body.motion = MotionType::Static;
            p.body.layer = CollisionLayer::StaticWorld;
            p.collider.type = ColliderType::Cylinder;
            p.collider.dimensions = {.22f * s, 2 * s, .22f * s};
            world.SetPhysics(e, p);
        }
        else
        {
            DetailObject({{x, y - .2f, z}, {0, yaw, 0}, {s, s, s}}, rocks, stone, 2 * s, 220);
            if (s > 2)
            {
                auto e = world.Create("Boulder collision");
                world.SetTransform(e, {{x, y + .35f * s, z}, {}, {1, 1, 1}});
                PhysicsComponent p;
                p.body.motion = MotionType::Static;
                p.body.layer = CollisionLayer::StaticWorld;
                p.collider.type = ColliderType::Sphere;
                p.collider.dimensions = {.65f * s, .65f * s, .65f * s};
                world.SetPhysics(e, p);
            }
        }
    }
    // Merge thousands of grass blades into spatial patches; no per-blade objects/transforms.
    for (int z = -160; z < 160; z += 20)
        for (int x = -160; x < 160; x += 20)
        {
            MeshData blades;
            for (unsigned i = 0; i < 90; ++i)
            {
                float wx = float(x) + Random(seed) * 20, wz = float(z) + Random(seed) * 20;
                if (LandscapeSurface::Facility(wx, wz) || LandscapeSurface::Buildable(wx, wz) ||
                    (wz > 38 && wz < 166 && LandscapeSurface::RoadDistance(wx, wz) < 6))
                    continue;
                float y = LandscapeSurface::Height(wx, wz), h = .25f + Random(seed) * .5f;
                for (unsigned j = 0; j < 3; ++j)
                {
                    float a = Random(seed) * 2 * Pi, dx = std::cos(a) * .08f,
                          dz = std::sin(a) * .08f;
                    Vec3 p{wx - dx, y, wz - dz}, q{wx + dx, y, wz + dz},
                        r{wx + dx * 2, y + h, wz + dz * 2};
                    Triangle(blades, p, r, q);
                    Triangle(blades, q, r, p);
                }
            }
            if (blades.indices.empty())
                continue;
            auto m = Mesh(std::move(blades));
            DetailObject({}, {m, m, m}, grass, 15, 70, false);
            details_.back().center = {float(x) + 10, 0, float(z) + 10};
        }
}
void Landscape::Update(const CameraDesc &camera, double dt)
{
    updateTime_ -= float(dt);
    if (updateTime_ > 0)
        return;
    updateTime_ = .12f;
    const float detailScale = quality_ == 0 ? .7f : quality_ == 1 ? 1.f : 1.35f;
    for (auto &d : details_)
    {
        Vec3 center = d.center;
        float distance =
            std::hypot(center.x - camera.position.x, center.z - camera.position.z) / detailScale;
        unsigned lod = distance < 55 ? 0 : distance < 115 ? 1 : 2;
        // Hysteresis prevents repeated resource swaps around a detail boundary.
        if (d.lod == 0 && distance < 61)
            lod = 0;
        if (d.lod == 1 && distance > 49 && distance < 121)
            lod = 1;
        if (d.lod == 2 && distance > 109)
            lod = 2;
        float margin = d.lod == 3 ? 0 : 10;
        if (distance > d.distance + margin)
            lod = 3;
        if (d.lod == lod)
            continue;
        if (d.object)
        {
            renderer_.DestroyObject(d.object);
            d.object = {};
        }
        d.lod = lod;
        if (lod < 3)
        {
            d.object = renderer_.CreateObject(d.meshes[lod], d.material, d.transform);
            renderer_.SetCastShadows(d.object, d.shadow && lod < 2);
        }
    }
}
} // namespace Game
