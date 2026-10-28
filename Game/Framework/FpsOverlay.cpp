#include "pgcpch.h"
#include "FpsOverlay.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>

namespace PlagCGame
{
namespace
{
std::array<unsigned char, 7> Glyph(char c)
{
    switch (c)
    {
#define G(ch, a, b, c, d, e, f, g) case ch: return {a, b, c, d, e, f, g};
        G('F', 31, 16, 16, 30, 16, 16, 16)
        G('P', 30, 17, 17, 30, 16, 16, 16)
        G('S', 15, 16, 16, 14, 1, 1, 30)
        G('0', 14, 17, 19, 21, 25, 17, 14)
        G('1', 4, 12, 4, 4, 4, 4, 14)
        G('2', 14, 17, 1, 2, 4, 8, 31)
        G('3', 30, 1, 1, 14, 1, 1, 30)
        G('4', 2, 6, 10, 18, 31, 2, 2)
        G('5', 31, 16, 16, 30, 1, 1, 30)
        G('6', 14, 16, 16, 30, 17, 17, 14)
        G('7', 31, 1, 2, 4, 8, 8, 8)
        G('8', 14, 17, 17, 14, 17, 17, 14)
        G('9', 14, 17, 17, 15, 1, 1, 14)
#undef G
    default:
        return {};
    }
}

bool OverlayEnabled()
{
    static const bool enabled = [] {
        const char *value = std::getenv("PC_FPS_OVERLAY");
        return !value || std::string_view(value) != "0";
    }();
    return enabled;
}

PlagC::Vec3 Add(PlagC::Vec3 a, PlagC::Vec3 b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}
PlagC::Vec3 Sub(PlagC::Vec3 a, PlagC::Vec3 b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}
PlagC::Vec3 Scale(PlagC::Vec3 a, float s)
{
    return {a.x * s, a.y * s, a.z * s};
}
float Length(PlagC::Vec3 v)
{
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}
} // namespace

void FpsOverlay::UpdateText(PlagC::Renderer &renderer, const std::string &text)
{
    if (object_ && text == text_)
        return;

    PlagC::MeshData mesh;
    unsigned x = 0;
    constexpr float pixel = .012f;
    for (char c : text)
    {
        const auto glyph = Glyph(c);
        for (unsigned row = 0; row < 7; ++row)
            for (unsigned col = 0; col < 5; ++col)
                if (glyph[row] & (1u << (4u - col)))
                {
                    const float px = static_cast<float>(x + col) * pixel;
                    const float py = -static_cast<float>(row) * pixel;
                    for (auto uv : {std::pair{0.f, 0.f}, std::pair{1.f, 0.f},
                                    std::pair{1.f, 1.f}, std::pair{0.f, 1.f}})
                        mesh.vertices.push_back(
                            {{px + uv.first * pixel, py + uv.second * pixel, 0}, {0, 0, 1}, 0, 0});
                }
        x += 6;
    }

    if (!material_)
    {
        PlagC::MaterialDesc material;
        material.model = PlagC::MaterialModel::Unlit;
        material.colour = {.35f, 1.0f, .40f};
        material.overlay = true;
        material_ = renderer.CreateMaterial(material);
    }

    const std::size_t quads = mesh.vertices.size() / 4u;
    if (!quads)
        return;

    if (!mesh_ || quads > capacityQuads_)
    {
        capacityQuads_ = std::max(quads + 16u, quads + quads / 4u);
        mesh.vertices.resize(capacityQuads_ * 4u);
        mesh.indices.reserve(capacityQuads_ * 6u);
        for (std::size_t q = 0; q < capacityQuads_; ++q)
        {
            const auto base = static_cast<std::uint32_t>(q * 4u);
            mesh.indices.insert(mesh.indices.end(),
                                {base, base + 1u, base + 2u, base, base + 2u, base + 3u});
        }

        const auto nextMesh = renderer.CreateMesh(mesh, true);
        const auto nextObject = renderer.CreateObject(nextMesh, material_);
        renderer.SetCastShadows(nextObject, false);
        renderer.SetRenderQueue(nextObject, 199);
        if (object_)
            renderer.DestroyObject(object_);
        if (mesh_)
            renderer.DestroyMesh(mesh_);
        mesh_ = nextMesh;
        object_ = nextObject;
    }
    else
    {
        mesh.vertices.resize(capacityQuads_ * 4u);
        renderer.UpdateMesh(mesh_, mesh.vertices);
        renderer.SetVisible(object_, true);
    }
    text_ = text;
}

void FpsOverlay::Place(PlagC::Renderer &renderer, const PlagC::CameraDesc &camera, float aspect)
{
    if (!object_)
        return;

    auto forward = Sub(camera.target, camera.position);
    const float length = Length(forward);
    if (length <= .0001f)
        return;
    forward = Scale(forward, 1.0f / length);

    PlagC::Vec3 right{-forward.z, 0, forward.x};
    const float rightLength = Length(right);
    if (rightLength <= .0001f)
        right = {1, 0, 0};
    else
        right = Scale(right, 1.0f / rightLength);
    const PlagC::Vec3 up{right.y * forward.z - right.z * forward.y,
                         right.z * forward.x - right.x * forward.z,
                         right.x * forward.y - right.y * forward.x};

    constexpr float pi = 3.14159265358979323846f;
    constexpr float pixel = .012f;
    const float yaw = std::atan2(forward.z, forward.x) * 180.0f / pi;
    const float pitch = std::asin(std::clamp(forward.y, -1.0f, 1.0f)) * 180.0f / pi;
    const float halfHeight = 2.0f * std::tan(camera.fov * pi / 360.0f);
    const float scale = std::min(.70f, aspect / 2.4f) * halfHeight;
    const float textWidth = static_cast<float>(std::max<std::size_t>(1u, text_.size())) * 6.0f *
                            pixel * scale;
    const float horizontal = .94f * halfHeight * aspect - textWidth;
    const float top = .90f * halfHeight;
    auto position = Add(camera.position, Scale(forward, 2.0f));
    position = Add(position, Scale(right, horizontal));
    position = Add(position, Scale(up, top));
    renderer.SetTransform(object_, {position, {pitch, -yaw - 90.0f, 0}, {scale, scale, scale}});
}

void FpsOverlay::Frame(PlagC::Renderer &renderer, const PlagC::CameraDesc &camera, double dt,
                       float aspect)
{
    if (!OverlayEnabled())
        return;
    if (!std::isfinite(dt) || dt < 0)
        return;

    sampleSeconds_ += dt;
    ++sampleFrames_;
    // A quarter-second window is responsive but avoids a rapidly flickering number and
    // limits dynamic-buffer uploads to four per second.
    if (sampleSeconds_ >= .25)
    {
        const auto fps = static_cast<unsigned>(std::lround(sampleFrames_ / sampleSeconds_));
        UpdateText(renderer, "FPS " + std::to_string(fps));
        sampleSeconds_ = 0.0;
        sampleFrames_ = 0;
    }
    Place(renderer, camera, aspect > 0 ? aspect : 16.0f / 9.0f);
}

void FpsOverlay::Clear(PlagC::Renderer &renderer)
{
    if (object_)
        renderer.DestroyObject(object_);
    if (mesh_)
        renderer.DestroyMesh(mesh_);
    if (material_)
        renderer.DestroyMaterial(material_);
    object_ = {};
    mesh_ = {};
    material_ = {};
    capacityQuads_ = 0;
    text_.clear();
    sampleSeconds_ = 0.0;
    sampleFrames_ = 0;
}
} // namespace PlagCGame
