#include "pgcpch.h"
#include <PlagC/Assets/Asset.h>
#include <bit>
#include <cstring>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
namespace PlagC
{
namespace
{
constexpr std::size_t MaxBytes = 128 * 1024 * 1024;
struct Writer
{
    std::vector<std::uint8_t> data;
    void U(std::uint32_t v)
    {
        for (int i = 0; i < 4; ++i)
            data.push_back(std::uint8_t(v >> (8 * i)));
    }
    void F(float v)
    {
        U(std::bit_cast<std::uint32_t>(v));
    }
    void S(const std::string &s)
    {
        if (s.size() > 131072)
            throw std::runtime_error("asset string too large");
        U(std::uint32_t(s.size()));
        data.insert(data.end(), s.begin(), s.end());
    }
    template <class T, class Fn> void V(const std::vector<T> &values, Fn fn)
    {
        if (values.size() > MaxBytes / 4)
            throw std::runtime_error("asset array too large");
        U(std::uint32_t(values.size()));
        for (auto &v : values)
            fn(v);
    }
    void Vec(Vec3 v)
    {
        F(v.x);
        F(v.y);
        F(v.z);
    }
    void Q(Quaternion q)
    {
        F(q.x);
        F(q.y);
        F(q.z);
        F(q.w);
    }
    void Mat(const Matrix4 &m)
    {
        for (auto v : m.m)
            F(v);
    }
};
struct Reader
{
    std::span<const std::uint8_t> data;
    std::size_t cursor = 0;
    void Need(std::size_t n)
    {
        if (n > data.size() - cursor)
            throw std::runtime_error("truncated cooked asset");
    }
    std::uint32_t U()
    {
        Need(4);
        std::uint32_t v = 0;
        for (int i = 0; i < 4; ++i)
            v |= std::uint32_t(data[cursor++]) << (8 * i);
        return v;
    }
    float F()
    {
        return std::bit_cast<float>(U());
    }
    std::string S()
    {
        auto n = U();
        if (n > 131072)
            throw std::runtime_error("asset string limit");
        Need(n);
        std::string s(reinterpret_cast<const char *>(data.data() + cursor), n);
        cursor += n;
        return s;
    }
    template <class T, class Fn> void V(std::vector<T> &values, Fn fn)
    {
        auto n = U();
        if (n > 4194304 || n > (data.size() - cursor) / 4)
            throw std::runtime_error("asset array limit");
        values.reserve(n);
        for (unsigned i = 0; i < n; ++i)
            values.push_back(fn());
    }
    Vec3 Vec()
    {
        Vec3 v;
        v.x = F();
        v.y = F();
        v.z = F();
        return v;
    }
    Quaternion Q()
    {
        Quaternion q;
        q.x = F();
        q.y = F();
        q.z = F();
        q.w = F();
        return q;
    }
    Matrix4 Mat()
    {
        Matrix4 m;
        for (auto &v : m.m)
            v = F();
        return m;
    }
};
std::uint64_t Hash(std::span<const std::uint8_t> bytes)
{
    std::uint64_t h = 14695981039346656037ull;
    for (auto b : bytes)
    {
        h ^= b;
        h *= 1099511628211ull;
    }
    return h;
}
void WriteFile(Writer &payload, AssetType type, const std::filesystem::path &path)
{
    if (payload.data.size() > MaxBytes)
        throw std::runtime_error("cooked asset exceeds 128 MiB");
    Writer file;
    file.data = {'P', 'L', 'A', 'G', 'C', 'A', 'S', 'T'};
    file.U(2);
    file.U(unsigned(type));
    file.U(unsigned(payload.data.size()));
    auto hash = Hash(payload.data);
    file.U(unsigned(hash));
    file.U(unsigned(hash >> 32));
    file.data.insert(file.data.end(), payload.data.begin(), payload.data.end());
    if (path.has_parent_path())
        std::filesystem::create_directories(path.parent_path());
    static std::atomic<std::uint64_t> serial{0};
    auto temp = path;
    temp += ".tmp-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
            "-" + std::to_string(++serial);
    try
    {
        std::ofstream out(temp, std::ios::binary);
        out.write(reinterpret_cast<const char *>(file.data.data()),
                  std::streamsize(file.data.size()));
        out.close();
        if (!out)
            throw std::runtime_error("asset write failed");
#ifdef _WIN32
        if (!MoveFileExW(temp.c_str(), path.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("asset atomic replacement failed");
#else
        std::filesystem::rename(temp, path);
#endif
    }
    catch (...)
    {
        std::error_code ec;
        std::filesystem::remove(temp, ec);
        throw;
    }
}
} // namespace
void SaveAsset(const ModelAsset &a, const std::filesystem::path &path)
{
    Validate(a);
    Writer w;
    w.V(a.skeleton.joints, [&](const auto &n) {
        w.S(n.name);
        w.U(std::uint32_t(n.parent));
        w.Vec(n.local.translation);
        w.Q(n.local.rotation);
        w.Vec(n.local.scale);
    });
    w.V(a.materials, [&](const auto &m) {
        w.S(m.name);
        w.Vec(m.colour);
        w.F(m.roughness);
        w.F(m.metallic);
        w.U(std::uint32_t(m.baseColourTexture));
        for (auto index : {m.normalTexture, m.metallicTexture, m.roughnessTexture,
                           m.occlusionTexture, m.emissiveTexture})
            w.U(std::uint32_t(index));
        w.Vec(m.emissive);
        w.F(m.normalScale);
    });
    w.V(a.textures, [&](const auto &t) {
        w.S(t.name);
        w.U(t.width);
        w.U(t.height);
        w.U(unsigned(t.rgba.size()));
        w.data.insert(w.data.end(), t.rgba.begin(), t.rgba.end());
    });
    w.V(a.meshes, [&](const auto &m) {
        w.S(m.name);
        w.U(m.node);
        w.U(m.material);
        w.V(m.geometry.vertices, [&](auto v) {
            w.Vec(v.position);
            w.Vec(v.normal);
            w.F(v.u);
            w.F(v.v);
            w.Vec(v.tangent);
            w.F(v.tangentSign);
        });
        w.V(m.geometry.indices, [&](auto v) { w.U(v); });
        w.V(m.tangents, [&](auto v) { w.Vec(v); });
        w.V(m.skin, [&](auto v) {
            for (auto j : v.joints)
                w.U(j);
            for (auto x : v.weights)
                w.F(x);
        });
        w.V(m.inverseBind, [&](auto v) { w.Mat(v); });
    });
    w.V(a.animations, [&](const auto &c) {
        w.S(c.name);
        w.F(c.duration);
        w.V(c.tracks, [&](const auto &t) {
            auto vec = [&](auto v) {
                w.F(v.time);
                w.Vec(v.value);
            };
            w.V(t.translations, vec);
            w.V(t.rotations, [&](auto v) {
                w.F(v.time);
                w.Q(v.value);
            });
            w.V(t.scales, vec);
        });
    });
    w.V(a.dependencies, [&](auto &s) { w.S(s); });
    w.V(a.warnings, [&](auto &s) { w.S(s); });
    WriteFile(w, AssetType::Model, path);
}
void SaveAsset(const AudioClip &a, const std::filesystem::path &path)
{
    Validate(a);
    Writer w;
    w.U(a.sampleRate);
    w.U(a.channels);
    w.U(unsigned(a.samples.size()));
    for (auto s : a.samples)
    {
        w.data.push_back(std::uint8_t(s));
        w.data.push_back(std::uint8_t(std::uint16_t(s) >> 8));
    }
    WriteFile(w, AssetType::Audio, path);
}
void SaveAsset(const TextureAsset &a, const std::filesystem::path &path)
{
    Validate(a);
    Writer w;
    w.S(a.name);
    w.U(a.width);
    w.U(a.height);
    w.U(unsigned(a.rgba.size()));
    w.data.insert(w.data.end(), a.rgba.begin(), a.rgba.end());
    WriteFile(w, AssetType::Texture, path);
}
std::shared_ptr<Asset> ReadAsset(const std::filesystem::path &path)
{
    auto size = std::filesystem::file_size(path);
    if (size < 28 || size > MaxBytes + 28)
        throw std::runtime_error("invalid cooked asset size");
    std::vector<std::uint8_t> bytes(size);
    std::ifstream file(path, std::ios::binary);
    file.read(reinterpret_cast<char *>(bytes.data()), std::streamsize(size));
    if (!file)
        throw std::runtime_error("asset read failed");
    if (std::memcmp(bytes.data(), "PLAGCAST", 8))
        throw std::runtime_error("invalid cooked asset magic");
    Reader r{bytes, 8};
    auto version = r.U();
    if (version != 1 && version != 2)
        throw std::runtime_error("unsupported cooked asset version");
    auto type = AssetType(r.U());
    auto n = r.U();
    std::uint64_t hash = r.U();
    hash |= std::uint64_t(r.U()) << 32;
    if (n != size - 28 || Hash(std::span(bytes).subspan(28)) != hash)
        throw std::runtime_error("cooked asset checksum/length mismatch");
    std::shared_ptr<Asset> result;
    if (type == AssetType::Model)
    {
        auto a = std::make_shared<ModelAsset>();
        r.V(a->skeleton.joints, [&] {
            AssetNode v;
            v.name = r.S();
            v.parent = std::bit_cast<std::int32_t>(r.U());
            v.local.translation = r.Vec();
            v.local.rotation = r.Q();
            v.local.scale = r.Vec();
            return v;
        });
        r.V(a->materials, [&] {
            MaterialAsset v;
            v.name = r.S();
            v.colour = r.Vec();
            v.roughness = r.F();
            v.metallic = r.F();
            v.baseColourTexture = std::bit_cast<std::int32_t>(r.U());
            if (version >= 2)
            {
                for (auto ptr : {&v.normalTexture, &v.metallicTexture, &v.roughnessTexture,
                                 &v.occlusionTexture, &v.emissiveTexture})
                    *ptr = std::bit_cast<std::int32_t>(r.U());
                v.emissive = r.Vec();
                v.normalScale = r.F();
            }
            return v;
        });
        r.V(a->textures, [&] {
            TextureAsset v;
            v.name = r.S();
            v.width = r.U();
            v.height = r.U();
            auto count = r.U();
            r.Need(count);
            v.rgba.assign(r.data.begin() + r.cursor, r.data.begin() + r.cursor + count);
            r.cursor += count;
            return v;
        });
        r.V(a->meshes, [&] {
            MeshAsset m;
            m.name = r.S();
            m.node = r.U();
            m.material = r.U();
            r.V(m.geometry.vertices, [&] {
                Vertex v;
                v.position = r.Vec();
                v.normal = r.Vec();
                v.u = r.F();
                v.v = r.F();
                if (version >= 2)
                {
                    v.tangent = r.Vec();
                    v.tangentSign = r.F();
                }
                return v;
            });
            r.V(m.geometry.indices, [&] { return r.U(); });
            r.V(m.tangents, [&] { return r.Vec(); });
            r.V(m.skin, [&] {
                VertexSkin v;
                for (auto &j : v.joints)
                {
                    auto value = r.U();
                    if (value > 65535)
                        throw std::runtime_error("joint index overflow");
                    j = std::uint16_t(value);
                }
                for (auto &x : v.weights)
                    x = r.F();
                return v;
            });
            r.V(m.inverseBind, [&] { return r.Mat(); });
            return m;
        });
        r.V(a->animations, [&] {
            AnimationClip c;
            c.name = r.S();
            c.duration = r.F();
            r.V(c.tracks, [&] {
                JointTrack t;
                auto vec = [&] {
                    VectorKey v;
                    v.time = r.F();
                    v.value = r.Vec();
                    return v;
                };
                r.V(t.translations, vec);
                r.V(t.rotations, [&] {
                    RotationKey v;
                    v.time = r.F();
                    v.value = r.Q();
                    return v;
                });
                r.V(t.scales, vec);
                return t;
            });
            return c;
        });
        r.V(a->dependencies, [&] { return r.S(); });
        r.V(a->warnings, [&] { return r.S(); });
        Validate(*a);
        result = a;
    }
    else if (type == AssetType::Audio)
    {
        auto a = std::make_shared<AudioClip>();
        a->sampleRate = r.U();
        a->channels = r.U();
        auto count = r.U();
        r.Need(std::uint64_t(count) * 2);
        a->samples.resize(count);
        for (auto &s : a->samples)
        {
            s = std::bit_cast<std::int16_t>(
                std::uint16_t(std::uint16_t(r.data[r.cursor]) |
                              std::uint16_t(std::uint16_t(r.data[r.cursor + 1]) << 8)));
            r.cursor += 2;
        }
        Validate(*a);
        result = a;
    }
    else if (type == AssetType::Texture)
    {
        auto a = std::make_shared<TextureAsset>();
        a->name = r.S();
        a->width = r.U();
        a->height = r.U();
        const auto count = r.U();
        r.Need(count);
        a->rgba.assign(r.data.begin() + r.cursor, r.data.begin() + r.cursor + count);
        r.cursor += count;
        Validate(*a);
        result = a;
    }
    else
        throw std::runtime_error("unknown cooked asset type");
    if (r.cursor != r.data.size())
        throw std::runtime_error("unexpected cooked asset trailing bytes");
    result->metadata.type = type;
    result->metadata.bytes = size;
    return result;
}
} // namespace PlagC