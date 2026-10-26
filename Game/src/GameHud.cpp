#include "pgcpch.h"
#include "GameHud.h"
#include <cctype>
using namespace PlagC;
namespace
{
// Five columns, seven rows. Geometry is rebuilt only when the HUD text changes.
std::array<unsigned char, 7> Glyph(char c)
{
    switch (c)
    {
#define G(ch, a, b, c, d, e, f, g)                                                                 \
    case ch:                                                                                       \
        return {a, b, c, d, e, f, g};
        G('A', 14, 17, 17, 31, 17, 17, 17)
        G('B', 30, 17, 17, 30, 17, 17, 30) G('C', 14, 17, 16, 16, 16, 17, 14) G(
            'D', 30, 17, 17, 17, 17, 17, 30) G('E', 31, 16, 16, 30, 16, 16, 31)
            G('F', 31, 16, 16, 30, 16, 16, 16) G('G', 14, 17, 16, 23, 17, 17, 15) G(
                'H', 17, 17, 17, 31, 17, 17, 17) G('I', 31, 4, 4, 4, 4, 4, 31)
                G('J', 7, 2, 2, 2, 18, 18, 12) G('K', 17, 18, 20, 24, 20, 18, 17)
                    G('L', 16, 16, 16, 16, 16, 16, 31) G('M', 17, 27, 21, 21, 17, 17, 17) G(
                        'N', 17, 25, 21, 19, 17, 17, 17) G('O', 14, 17, 17, 17, 17, 17, 14)
                        G('P', 30, 17, 17, 30, 16, 16, 16) G('Q', 14, 17, 17, 17, 21, 18, 13) G(
                            'R', 30, 17, 17, 30, 20, 18, 17) G('S', 15, 16, 16, 14, 1, 1, 30)
                            G('T', 31, 4, 4, 4, 4, 4, 4) G('U', 17, 17, 17, 17, 17, 17, 14)
                                G('V', 17, 17, 17, 17, 17, 10, 4) G('W', 17, 17, 17, 21, 21, 21, 10)
                                    G('X', 17, 17, 10, 4, 10, 17, 17) G('Y', 17, 17, 10, 4, 4, 4, 4)
                                        G('Z', 31, 1, 2, 4, 8, 16,
                                          31) G('0', 14, 17, 19, 21, 25, 17,
                                                14) G('1', 4, 12, 4, 4, 4, 4, 14)
                                            G('2', 14, 17, 1, 2, 4, 8,
                                              31) G('3', 30, 1, 1, 14, 1, 1,
                                                    30) G('4', 2, 6, 10, 18, 31, 2, 2)
                                                G('5', 31, 16, 16, 30, 1, 1,
                                                  30) G('6', 14, 16, 16, 30, 17, 17,
                                                        14) G('7', 31, 1, 2, 4, 8, 8, 8)
                                                    G('8', 14, 17, 17, 14, 17, 17,
                                                      14) G('9', 14, 17, 17, 15, 1, 1,
                                                            14) G('.', 0, 0, 0, 0, 0, 6, 6)
                                                        G(':', 0, 6, 6, 0, 6, 6,
                                                          0) G('-', 0, 0, 0, 31, 0, 0,
                                                               0) G('/', 1, 1, 2, 4, 8, 16, 16)
                                                            G('=', 0, 0, 31, 0, 31, 0, 0)
                                                                G('(', 2, 4, 8, 8, 8, 4, 2)
                                                                    G(')', 8, 4, 2, 2, 2, 4, 8)
                                                                        G('+', 0, 4, 4, 31, 4, 4, 0)
#undef G
                                                                            default : return {};
    }
}
} // namespace
void GameHud::Update(Renderer &r, const std::string &text)
{
    MeshData mesh;
    unsigned x = 0, y = 0;
    constexpr float pixel = .009f;
    for (unsigned char raw : text)
    {
        if (raw == '\n')
        {
            x = 0;
            y += 9;
            continue;
        }
        auto glyph = Glyph(char(std::toupper(raw)));
        for (unsigned row = 0; row < 7; ++row)
            for (unsigned col = 0; col < 5; ++col)
                if (glyph[row] & (1u << (4 - col)))
                {
                    float px = float(x + col) * pixel, py = -float(y + row) * pixel;
                    auto base = std::uint32_t(mesh.vertices.size());
                    for (auto uv : {std::pair{0.f, 0.f}, {1.f, 0.f}, {1.f, 1.f}, {0.f, 1.f}})
                        mesh.vertices.push_back(
                            {{px + uv.first * pixel, py + uv.second * pixel, 0}, {0, 0, 1}, 0, 0});
                    mesh.indices.insert(mesh.indices.end(),
                                        {base, base + 1, base + 2, base, base + 2, base + 3});
                }
        x += 6;
    }
    if (mesh.indices.empty())
        return;
    if (!material_)
    {
        MaterialDesc d;
        d.model = MaterialModel::Unlit;
        d.colour = {.95f, 1, .95f};
        d.overlay = true;
        material_ = r.CreateMaterial(d);
    }
    if (!cross_)
    {
        MeshData cross;
        for (auto size : {std::pair{.018f, .002f}, std::pair{.002f, .018f}})
        {
            auto base = std::uint32_t(cross.vertices.size());
            for (auto point :
                 {Vec3{-size.first, -size.second, 0}, Vec3{size.first, -size.second, 0},
                  Vec3{size.first, size.second, 0}, Vec3{-size.first, size.second, 0}})
                cross.vertices.push_back({point, {0, 0, 1}, 0, 0});
            cross.indices.insert(cross.indices.end(),
                                 {base, base + 1, base + 2, base, base + 2, base + 3});
        }
        crossMesh_ = r.CreateMesh(cross);
        cross_ = r.CreateObject(crossMesh_, material_);
        r.SetCastShadows(cross_, false);
        r.SetRenderQueue(cross_, 199);
    }
    auto next = r.CreateMesh(mesh);
    auto object = r.CreateObject(next, material_);
    r.SetCastShadows(object, false);
    r.SetRenderQueue(object, 199);
    if (object_)
        r.DestroyObject(object_);
    if (mesh_)
        r.DestroyMesh(mesh_);
    mesh_ = next;
    object_ = object;
}
void GameHud::Place(Renderer &r, Vec3 position, float yaw, float pitch, float aspect, float fov)
{
    if (!object_)
        return;
    constexpr float rad = 3.14159265358979323846f / 180;
    Vec3 f{std::cos(yaw * rad) * std::cos(pitch * rad), std::sin(pitch * rad),
           std::sin(yaw * rad) * std::cos(pitch * rad)},
        right{-std::sin(yaw * rad), 0, std::cos(yaw * rad)},
        up{-std::cos(yaw * rad) * std::sin(pitch * rad), std::cos(pitch * rad),
           -std::sin(yaw * rad) * std::sin(pitch * rad)};
    r.SetTransform(cross_, {{position.x + 2 * f.x, position.y + 2 * f.y, position.z + 2 * f.z},
                            {pitch, -yaw - 90, 0},
                            {1, 1, 1}});
    float half = 2 * std::tan(fov * rad * .5f);
    float left = -.92f * half * aspect, top = .82f * half;
    position = {position.x + 2 * f.x + left * right.x + top * up.x,
                position.y + 2 * f.y + left * right.y + top * up.y,
                position.z + 2 * f.z + left * right.z + top * up.z};
    float scale = std::min(.65f, aspect / 2.6f) * half;
    r.SetTransform(object_, {position, {pitch, -yaw - 90, 0}, {scale, scale, scale}});
}
void GameHud::Clear(Renderer &r)
{
    if (cross_)
        r.DestroyObject(cross_);
    if (crossMesh_)
        r.DestroyMesh(crossMesh_);
    cross_ = {};
    crossMesh_ = {};
    if (object_)
        r.DestroyObject(object_);
    if (mesh_)
        r.DestroyMesh(mesh_);
    if (material_)
        r.DestroyMaterial(material_);
    object_ = {};
    mesh_ = {};
    material_ = {};
}
