#include "pgcpch.h"
#include <PlagC/Assets/Asset.h>
namespace PlagC
{
namespace
{
void Require(bool v, const char *m)
{
    if (!v)
        throw std::invalid_argument(m);
}
bool Finite(Vec3 v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
bool Finite(Quaternion q)
{
    return std::isfinite(q.x) && std::isfinite(q.y) && std::isfinite(q.z) && std::isfinite(q.w);
}
} // namespace
Matrix4 Multiply(const Matrix4 &a, const Matrix4 &b)
{
    Matrix4 r;
    r.m.fill(0);
    for (int c = 0; c < 4; ++c)
        for (int row = 0; row < 4; ++row)
            for (int k = 0; k < 4; ++k)
                r.m[c * 4 + row] += a.m[k * 4 + row] * b.m[c * 4 + k];
    return r;
}
Matrix4 Matrix(const JointTransform &t)
{
    auto q = t.rotation;
    float x = q.x, y = q.y, z = q.z, w = q.w;
    Matrix4 r;
    r.m = {(1 - 2 * y * y - 2 * z * z) * t.scale.x,
           (2 * x * y + 2 * w * z) * t.scale.x,
           (2 * x * z - 2 * w * y) * t.scale.x,
           0,
           (2 * x * y - 2 * w * z) * t.scale.y,
           (1 - 2 * x * x - 2 * z * z) * t.scale.y,
           (2 * y * z + 2 * w * x) * t.scale.y,
           0,
           (2 * x * z + 2 * w * y) * t.scale.z,
           (2 * y * z - 2 * w * x) * t.scale.z,
           (1 - 2 * x * x - 2 * y * y) * t.scale.z,
           0,
           t.translation.x,
           t.translation.y,
           t.translation.z,
           1};
    return r;
}
Vec3 TransformPoint(const Matrix4 &m, Vec3 p)
{
    return {m.m[0] * p.x + m.m[4] * p.y + m.m[8] * p.z + m.m[12],
            m.m[1] * p.x + m.m[5] * p.y + m.m[9] * p.z + m.m[13],
            m.m[2] * p.x + m.m[6] * p.y + m.m[10] * p.z + m.m[14]};
}
Vec3 TransformNormal(const Matrix4 &m, Vec3 p)
{ // inverse transpose, including nonuniform scales
    float a = m.m[0], b = m.m[4], c = m.m[8], d = m.m[1], e = m.m[5], f = m.m[9], g = m.m[2],
          h = m.m[6], i = m.m[10];
    float det = a * (e * i - f * h) - b * (d * i - f * g) + c * (d * h - e * g);
    Require(std::abs(det) > 1e-12f, "singular normal transform");
    Vec3 n{((e * i - f * h) * p.x + (f * g - d * i) * p.y + (d * h - e * g) * p.z) / det,
           ((c * h - b * i) * p.x + (a * i - c * g) * p.y + (b * g - a * h) * p.z) / det,
           ((b * f - c * e) * p.x + (c * d - a * f) * p.y + (a * e - b * d) * p.z) / det};
    float length = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
    if (length > 1e-8f)
    {
        n.x /= length;
        n.y /= length;
        n.z /= length;
    }
    return n;
}
void Validate(const ModelAsset &a)
{
    Require(!a.meshes.empty() && a.meshes.size() <= 4096, "invalid mesh count");
    Require(!a.materials.empty() && a.materials.size() <= 4096, "invalid material count");
    auto count = a.skeleton.joints.size();
    Require(count > 0 && count <= 1024, "skeleton/node count must be 1..1024");
    std::vector<int> ancestry;
    for (std::size_t i = 0; i < count; ++i)
    {
        auto &n = a.skeleton.joints[i];
        Require(n.parent >= -1 && n.parent < int(i), "invalid/cyclic node parent");
        while (!ancestry.empty() && ancestry.back() != n.parent)
            ancestry.pop_back();
        Require(n.parent < 0 || !ancestry.empty(), "nodes must be in depth-first order");
        ancestry.push_back(int(i));
        Require(Finite(n.local.translation) && Finite(n.local.scale) && Finite(n.local.rotation),
                "nonfinite node transform");
        Require(n.local.scale.x > 0 && n.local.scale.y > 0 && n.local.scale.z > 0,
                "negative/zero node scale unsupported");
        auto q = n.local.rotation;
        float norm = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
        Require(std::abs(norm - 1) < .01f, "nonunit node rotation");
    }
    for (auto &t : a.textures)
        Require(t.width > 0 && t.height > 0 && t.width <= 8192 && t.height <= 8192 &&
                    t.rgba.size() == std::uint64_t(t.width) * t.height * 4,
                "invalid RGBA texture");
    for (auto &m : a.materials)
        Require(Finite(m.colour) && std::isfinite(m.roughness) && m.roughness >= 0 &&
                    m.roughness <= 1 && std::isfinite(m.metallic) && m.metallic >= 0 &&
                    m.metallic <= 1 && m.baseColourTexture >= -1 &&
                    m.baseColourTexture < int(a.textures.size()),
                "invalid material");
    for (auto &m : a.meshes)
    {
        Require(m.node < count && m.material < a.materials.size(), "invalid mesh binding");
        Require(!m.geometry.vertices.empty() && !m.geometry.indices.empty() &&
                    m.geometry.indices.size() % 3 == 0,
                "mesh must contain triangles");
        Require(m.skin.empty() || m.skin.size() == m.geometry.vertices.size(),
                "skin vertex count mismatch");
        Require(m.tangents.empty() || m.tangents.size() == m.geometry.vertices.size(),
                "tangent count mismatch");
        Require(m.skin.empty() || m.inverseBind.size() == count, "inverse bind count mismatch");
        for (auto &v : m.geometry.vertices)
            Require(Finite(v.position) && Finite(v.normal) && std::isfinite(v.u) &&
                        std::isfinite(v.v),
                    "nonfinite vertex");
        for (auto t : m.tangents)
            Require(Finite(t), "nonfinite tangent");
        for (auto idx : m.geometry.indices)
            Require(idx < m.geometry.vertices.size(), "invalid mesh index");
        for (auto &matrix : m.inverseBind)
            for (float v : matrix.m)
                Require(std::isfinite(v), "nonfinite inverse bind");
        for (auto &skin : m.skin)
        {
            float sum = 0;
            for (int k = 0; k < 4; ++k)
            {
                Require(skin.joints[k] < count && std::isfinite(skin.weights[k]) &&
                            skin.weights[k] >= 0,
                        "invalid skin influence");
                sum += skin.weights[k];
            }
            Require(std::abs(sum - 1) < .001f, "skin weights must sum to one");
        }
    }
    for (auto &clip : a.animations)
    {
        Require(std::isfinite(clip.duration) && clip.duration > 0 && clip.tracks.size() == count,
                "invalid clip tracks/duration");
        for (auto &t : clip.tracks)
        {
            auto vectors = [&](const auto &keys, bool scale) {
                float last = -1;
                Require(!keys.empty(), "empty animation channel");
                for (auto &k : keys)
                {
                    Require(std::isfinite(k.time) && k.time >= 0 && k.time <= clip.duration &&
                                k.time > last && Finite(k.value),
                            "invalid animation key");
                    if (scale)
                        Require(k.value.x > 0 && k.value.y > 0 && k.value.z > 0,
                                "nonpositive animated scale");
                    last = k.time;
                }
            };
            vectors(t.translations, false);
            vectors(t.scales, true);
            float last = -1;
            Require(!t.rotations.empty(), "empty rotation channel");
            for (auto &k : t.rotations)
            {
                auto q = k.value;
                float norm = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
                Require(std::isfinite(k.time) && k.time >= 0 && k.time <= clip.duration &&
                            k.time > last && Finite(q) && std::abs(norm - 1) < .01f,
                        "invalid rotation key");
                last = k.time;
            }
        }
    }
}
void Validate(const AudioClip &a)
{
    Require(a.sampleRate >= 8000 && a.sampleRate <= 192000 &&
                (a.channels == 1 || a.channels == 2) && !a.samples.empty() &&
                a.samples.size() % a.channels == 0,
            "invalid PCM audio");
}
} // namespace PlagC
