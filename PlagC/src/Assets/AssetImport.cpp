#include "pgcpch.h"
#include <PlagC/Assets/Asset.h>
#define STB_IMAGE_STATIC
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>

namespace PlagC
{
namespace
{
Vec3 Add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 Sub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 Cross(Vec3 a, Vec3 b)
{
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}
Vec3 Normalize(Vec3 v)
{
    const float length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    if (!(length > 1e-8f) || !std::isfinite(length))
        throw std::runtime_error("cannot generate a normal for degenerate model geometry");
    return {v.x / length, v.y / length, v.z / length};
}

void ScaleModel(ModelAsset &asset, float scale)
{
    if (!std::isfinite(scale) || scale <= 0.0f || scale > 100000.0f)
        throw std::invalid_argument("model import scale must be finite and in (0, 100000]");
    if (std::abs(scale - 1.0f) < 1e-7f)
        return;

    for (auto &node : asset.skeleton.joints)
    {
        node.local.translation.x *= scale;
        node.local.translation.y *= scale;
        node.local.translation.z *= scale;
    }
    for (auto &mesh : asset.meshes)
    {
        for (auto &vertex : mesh.geometry.vertices)
        {
            vertex.position.x *= scale;
            vertex.position.y *= scale;
            vertex.position.z *= scale;
        }
        for (auto &matrix : mesh.inverseBind)
        {
            matrix.m[12] *= scale;
            matrix.m[13] *= scale;
            matrix.m[14] *= scale;
        }
    }
    for (auto &clip : asset.animations)
        for (auto &track : clip.tracks)
            for (auto &key : track.translations)
            {
                key.value.x *= scale;
                key.value.y *= scale;
                key.value.z *= scale;
            }
}

void MirrorZ(ModelAsset &asset)
{
    for (auto &node : asset.skeleton.joints)
    {
        node.local.translation.z = -node.local.translation.z;
        node.local.rotation.x = -node.local.rotation.x;
        node.local.rotation.y = -node.local.rotation.y;
    }
    for (auto &mesh : asset.meshes)
    {
        for (auto &vertex : mesh.geometry.vertices)
        {
            vertex.position.z = -vertex.position.z;
            vertex.normal.z = -vertex.normal.z;
            vertex.tangent.z = -vertex.tangent.z;
            vertex.tangentSign = -vertex.tangentSign;
        }
        for (auto &tangent : mesh.tangents)
            tangent.z = -tangent.z;
        for (std::size_t i = 0; i + 2 < mesh.geometry.indices.size(); i += 3)
            std::swap(mesh.geometry.indices[i + 1], mesh.geometry.indices[i + 2]);
        for (auto &matrix : mesh.inverseBind)
            for (int column = 0; column < 4; ++column)
                for (int row = 0; row < 4; ++row)
                    if ((row == 2) != (column == 2))
                        matrix.m[column * 4 + row] = -matrix.m[column * 4 + row];
    }
    for (auto &clip : asset.animations)
        for (auto &track : clip.tracks)
        {
            for (auto &key : track.translations)
                key.value.z = -key.value.z;
            for (auto &key : track.rotations)
            {
                key.value.x = -key.value.x;
                key.value.y = -key.value.y;
            }
        }
}

void RecalculateSmoothNormals(ModelAsset &asset)
{
    for (auto &mesh : asset.meshes)
    {
        auto &vertices = mesh.geometry.vertices;
        for (auto &vertex : vertices)
            vertex.normal = {};
        for (std::size_t i = 0; i + 2 < mesh.geometry.indices.size(); i += 3)
        {
            const auto ia = mesh.geometry.indices[i];
            const auto ib = mesh.geometry.indices[i + 1];
            const auto ic = mesh.geometry.indices[i + 2];
            const Vec3 edgeA = Sub(vertices[ib].position, vertices[ia].position);
            const Vec3 edgeB = Sub(vertices[ic].position, vertices[ia].position);
            const Vec3 face = Cross(edgeA, edgeB);
            vertices[ia].normal = Add(vertices[ia].normal, face);
            vertices[ib].normal = Add(vertices[ib].normal, face);
            vertices[ic].normal = Add(vertices[ic].normal, face);
        }
        for (auto &vertex : vertices)
            vertex.normal = Normalize(vertex.normal);
    }
}

void StripMaterials(ModelAsset &asset)
{
    asset.textures.clear();
    asset.materials.clear();
    MaterialAsset fallback;
    fallback.name = "Default Imported Material";
    asset.materials.push_back(std::move(fallback));
    for (auto &mesh : asset.meshes)
        mesh.material = 0;
}

} // namespace

ModelAsset ImportModel(const std::filesystem::path &source, const ModelImportOptions &options)
{
    ModelAsset asset = ImportModel(source);
    ScaleModel(asset, options.scale);
    if (options.coordinates == ModelCoordinateImport::MirrorZToRightHanded)
        MirrorZ(asset);
    if (!options.flipUVs)
        for (auto &mesh : asset.meshes)
            for (auto &vertex : mesh.geometry.vertices)
                vertex.v = 1.0f - vertex.v;
    if (options.normals == ModelNormalImport::RecalculateSmooth)
        RecalculateSmoothNormals(asset);
    if (!options.tangents)
        for (auto &mesh : asset.meshes)
        {
            mesh.tangents.clear();
            for (auto &vertex : mesh.geometry.vertices)
            {
                vertex.tangent = {};
                vertex.tangentSign = 1.0f;
            }
        }
    if (!options.materials)
        StripMaterials(asset);
    if (!options.animations)
        asset.animations.clear();

    asset.warnings.push_back("Import settings: scale=" + std::to_string(options.scale) +
                             (options.coordinates == ModelCoordinateImport::MirrorZToRightHanded
                                  ? ", mirrored Z to engine handedness"
                                  : ", right-handed Y-up") +
                             (options.flipUVs ? ", UV V flipped" : ", source UV orientation") +
                             (options.tangents ? ", tangents enabled" : ", tangents stripped") +
                             (options.materials ? ", materials enabled" : ", materials stripped") +
                             (options.animations ? ", animations enabled" : ", animations stripped"));
    Validate(asset);
    return asset;
}

TextureAsset ImportTexture(const std::filesystem::path &source, const TextureImportOptions &options)
{
    const auto full = std::filesystem::canonical(source);
    std::string extension = full.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (extension != ".png" && extension != ".jpg" && extension != ".jpeg")
        throw std::invalid_argument("texture importer supports PNG and JPEG sources");
    if (std::filesystem::file_size(full) > 128ull * 1024ull * 1024ull)
        throw std::runtime_error("texture source exceeds 128 MiB");

    int width = 0, height = 0, channels = 0;
    unsigned char *pixels = stbi_load(full.string().c_str(), &width, &height, &channels, 4);
    if (!pixels)
        throw std::runtime_error(std::string("texture decode failed: ") + stbi_failure_reason());
    std::unique_ptr<unsigned char, decltype(&stbi_image_free)> guard(pixels, &stbi_image_free);
    if (width <= 0 || height <= 0 || width > 8192 || height > 8192)
        throw std::runtime_error("texture dimensions must be in 1..8192");

    TextureAsset texture;
    texture.name = full.filename().string();
    texture.width = static_cast<unsigned>(width);
    texture.height = static_cast<unsigned>(height);
    texture.rgba.assign(pixels, pixels + std::size_t(width) * height * 4u);
    if (options.flipVertical)
    {
        const std::size_t rowBytes = std::size_t(width) * 4u;
        std::vector<std::uint8_t> row(rowBytes);
        for (int y = 0; y < height / 2; ++y)
        {
            auto *top = texture.rgba.data() + std::size_t(y) * rowBytes;
            auto *bottom = texture.rgba.data() + std::size_t(height - 1 - y) * rowBytes;
            std::memcpy(row.data(), top, rowBytes);
            std::memcpy(top, bottom, rowBytes);
            std::memcpy(bottom, row.data(), rowBytes);
        }
    }
    texture.metadata.type = AssetType::Texture;
    Validate(texture);
    return texture;
}

void Validate(const TextureAsset &texture)
{
    if (texture.width == 0 || texture.height == 0 || texture.width > 8192 || texture.height > 8192 ||
        texture.rgba.size() != std::uint64_t(texture.width) * texture.height * 4ull)
        throw std::invalid_argument("invalid RGBA texture asset");
}

} // namespace PlagC
