#include "pgcpch.h"
#include <PlagC/Assets/Asset.h>
#include <bit>
#include <cstring>
#include <fstream>
#include <limits>
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
constexpr std::size_t MaxMaterialBytes = 1024 * 1024;
constexpr std::uint32_t MaterialVersion = 1;
constexpr char MaterialMagic[] = "PGCMAT01";

void Require(bool condition, const char* message)
{
    if (!condition)
        throw std::invalid_argument(message);
}

bool Finite(Vec3 value)
{
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

void ValidateTexturePath(const std::string& path)
{
    if (path.empty())
        return;
    Require(path.size() <= 4096, "material texture path is too long");
    Require(path.find('\0') == std::string::npos && path.find('\\') == std::string::npos,
            "material texture path must use forward slashes");
    Require(path.front() != '/' && path.find(':') == std::string::npos,
            "material texture path must be relative to the Assets resource root");
    for (const auto& part : std::filesystem::path(path))
        Require(part != "..", "material texture path traversal is not allowed");
}

std::string LogicalMaterialPath(const std::filesystem::path& input)
{
    std::string path = input.generic_string();
    constexpr std::string_view mounted = "Assets://Cooked/";
    constexpr std::string_view disk = "Assets/Cooked/";
    constexpr std::string_view cooked = "Cooked/";
    if (path.starts_with(mounted)) path.erase(0, mounted.size());
    else if (path.starts_with(disk)) path.erase(0, disk.size());
    else if (path.starts_with(cooked)) path.erase(0, cooked.size());

    Require(!path.empty() && path.front() != '/' && path.find(':') == std::string::npos,
            "material asset path must be relative to Assets/Cooked");
    for (const auto& part : std::filesystem::path(path))
        Require(part != "..", "material asset path traversal is not allowed");
    return path;
}

std::filesystem::path PhysicalMaterialPath(const std::filesystem::path& input)
{
    const std::string path = input.generic_string();
    if (path.starts_with("Assets/Cooked/"))
        return input;
    if (path.starts_with("Assets://Cooked/"))
        return std::filesystem::path("Assets/Cooked") / path.substr(std::string_view("Assets://Cooked/").size());
    if (path.starts_with("Cooked/"))
        return std::filesystem::path("Assets") / input;
    return std::filesystem::path("Assets/Cooked") / input;
}

struct Writer
{
    std::vector<std::uint8_t> bytes;
    void U32(std::uint32_t value)
    {
        for (unsigned i = 0; i < 4; ++i)
            bytes.push_back(static_cast<std::uint8_t>(value >> (i * 8u)));
    }
    void Float(float value) { U32(std::bit_cast<std::uint32_t>(value)); }
    void String(const std::string& value)
    {
        if (value.size() > 131072)
            throw std::invalid_argument("material string is too long");
        U32(static_cast<std::uint32_t>(value.size()));
        bytes.insert(bytes.end(), value.begin(), value.end());
    }
    void Vector(Vec3 value) { Float(value.x); Float(value.y); Float(value.z); }
};

struct Reader
{
    std::span<const std::uint8_t> bytes;
    std::size_t cursor = 0;
    void Need(std::size_t count) const
    {
        if (count > bytes.size() - cursor)
            throw std::runtime_error("truncated material asset");
    }
    std::uint32_t U32()
    {
        Need(4);
        std::uint32_t value = 0;
        for (unsigned i = 0; i < 4; ++i)
            value |= static_cast<std::uint32_t>(bytes[cursor++]) << (i * 8u);
        return value;
    }
    float Float() { return std::bit_cast<float>(U32()); }
    std::string String()
    {
        const auto count = U32();
        if (count > 131072)
            throw std::runtime_error("material string exceeds limit");
        Need(count);
        std::string value(reinterpret_cast<const char*>(bytes.data() + cursor), count);
        cursor += count;
        return value;
    }
    Vec3 Vector() { return {Float(), Float(), Float()}; }
};

void WriteMaterial(Writer& writer, const MaterialAsset& material)
{
    writer.String(material.name);
    writer.Vector(material.colour);
    writer.Float(material.roughness);
    writer.Float(material.metallic);
    writer.String(material.baseColourTexturePath);
    writer.String(material.normalTexturePath);
    writer.String(material.metallicTexturePath);
    writer.String(material.roughnessTexturePath);
    writer.String(material.occlusionTexturePath);
    writer.Vector(material.emissive);
    writer.String(material.emissiveTexturePath);
    writer.Float(material.normalScale);
    writer.Float(material.alphaCutoff);
    writer.U32(material.doubleSided ? 1u : 0u);
}

MaterialAsset ReadMaterial(Reader& reader)
{
    MaterialAsset material;
    material.name = reader.String();
    material.colour = reader.Vector();
    material.roughness = reader.Float();
    material.metallic = reader.Float();
    material.baseColourTexturePath = reader.String();
    material.normalTexturePath = reader.String();
    material.metallicTexturePath = reader.String();
    material.roughnessTexturePath = reader.String();
    material.occlusionTexturePath = reader.String();
    material.emissive = reader.Vector();
    material.emissiveTexturePath = reader.String();
    material.normalScale = reader.Float();
    material.alphaCutoff = reader.Float();
    const auto sided = reader.U32();
    if (sided > 1u)
        throw std::runtime_error("invalid material double-sided flag");
    material.doubleSided = sided != 0;
    return material;
}
} // namespace

void Validate(const MaterialAsset& material)
{
    Require(!material.name.empty() && material.name.size() <= 4096, "material name is invalid");
    Require(Finite(material.colour) && material.colour.x >= 0 && material.colour.y >= 0 && material.colour.z >= 0,
            "material base colour is invalid");
    Require(std::isfinite(material.roughness) && material.roughness >= 0 && material.roughness <= 1,
            "material roughness is invalid");
    Require(std::isfinite(material.metallic) && material.metallic >= 0 && material.metallic <= 1,
            "material metallic value is invalid");
    Require(Finite(material.emissive) && material.emissive.x >= 0 && material.emissive.y >= 0 && material.emissive.z >= 0,
            "material emissive value is invalid");
    Require(std::isfinite(material.normalScale) && material.normalScale >= 0,
            "material normal scale is invalid");
    Require(std::isfinite(material.alphaCutoff) && material.alphaCutoff >= 0 && material.alphaCutoff <= 1,
            "material alpha cutoff is invalid");
    ValidateTexturePath(material.baseColourTexturePath);
    ValidateTexturePath(material.normalTexturePath);
    ValidateTexturePath(material.metallicTexturePath);
    ValidateTexturePath(material.roughnessTexturePath);
    ValidateTexturePath(material.occlusionTexturePath);
    ValidateTexturePath(material.emissiveTexturePath);
}

void SaveMaterialAsset(const MaterialAsset& material, const std::filesystem::path& path)
{
    Validate(material);
    const auto physical = PhysicalMaterialPath(path);
    Writer writer;
    writer.bytes.insert(writer.bytes.end(), std::begin(MaterialMagic), std::end(MaterialMagic) - 1);
    writer.U32(MaterialVersion);
    WriteMaterial(writer, material);
    if (writer.bytes.size() > MaxMaterialBytes)
        throw std::runtime_error("material asset exceeds size limit");

    if (physical.has_parent_path())
        std::filesystem::create_directories(physical.parent_path());
    auto temporary = physical;
    temporary += ".tmp";
    try
    {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        stream.write(reinterpret_cast<const char*>(writer.bytes.data()), static_cast<std::streamsize>(writer.bytes.size()));
        stream.close();
        if (!stream)
            throw std::runtime_error("material asset write failed");
#ifdef _WIN32
        if (!MoveFileExW(temporary.c_str(), physical.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("material asset atomic replacement failed");
#else
        std::filesystem::rename(temporary, physical);
#endif
    }
    catch (...)
    {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw;
    }
}

AssetHandle<MaterialAsset> LoadMaterialAsset(const std::filesystem::path& path)
{
    const auto physical = PhysicalMaterialPath(path);
    const auto size = std::filesystem::file_size(physical);
    if (size < 12 || size > MaxMaterialBytes)
        throw std::runtime_error("invalid material asset size");
    std::vector<std::uint8_t> bytes(size);
    std::ifstream stream(physical, std::ios::binary);
    stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!stream)
        throw std::runtime_error("material asset read failed");
    if (std::memcmp(bytes.data(), MaterialMagic, 8) != 0)
        throw std::runtime_error("invalid material asset magic");

    Reader reader{bytes, 8};
    if (reader.U32() != MaterialVersion)
        throw std::runtime_error("unsupported material asset version");
    auto material = std::make_shared<MaterialAsset>(ReadMaterial(reader));
    if (reader.cursor != reader.bytes.size())
        throw std::runtime_error("unexpected material asset trailing bytes");
    material->assetPath = LogicalMaterialPath(path);
    Validate(*material);
    return AssetHandle<MaterialAsset>(std::move(material));
}

} // namespace PlagC
