#include "pgcpch.h"
#include <PlagC/Assets/Asset.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/material.h>
#include <assimp/GltfMaterial.h>
#include <assimp/IOSystem.hpp>
#include <assimp/DefaultIOSystem.h>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_MAX_DIMENSIONS 8192
#include <stb/stb_image.h>
namespace PlagC
{
namespace
{
Vec3 V(aiVector3D v)
{
    return {v.x, v.y, v.z};
}
Matrix4 M(const aiMatrix4x4 &m)
{
    Matrix4 result;
    result.m = {m.a1, m.b1, m.c1, m.d1, m.a2, m.b2, m.c2, m.d2,
                m.a3, m.b3, m.c3, m.d3, m.a4, m.b4, m.c4, m.d4};
    return result;
}
// Track and constrain all Assimp external reads to the source directory.
class ImportIO final : public Assimp::DefaultIOSystem
{
    std::filesystem::path root_;
    std::vector<std::string> &dependencies_;
    std::filesystem::path Safe(const char *file) const
    {
        auto path = std::filesystem::weakly_canonical(file);
        auto relative = path.lexically_relative(root_);
        if (relative.empty() || *relative.begin() == "..")
            throw std::runtime_error("model dependency escapes source directory");
        return path;
    }

  public:
    ImportIO(std::filesystem::path root, std::vector<std::string> &dependencies)
        : root_(std::move(root)), dependencies_(dependencies)
    {
    }
    bool Exists(const char *file) const override
    {
        try
        {
            return std::filesystem::is_regular_file(Safe(file));
        }
        catch (...)
        {
            return false;
        }
    }
    Assimp::IOStream *Open(const char *file, const char *mode = "rb") override
    {
        auto path = Safe(file);
        if (std::string_view(mode).find_first_of("wa+") != std::string_view::npos)
            return nullptr;
        if (std::filesystem::file_size(path) > 128 * 1024 * 1024)
            throw std::runtime_error("source dependency exceeds 128 MiB");
        auto relative = path.lexically_relative(root_).generic_string();
        if (std::find(dependencies_.begin(), dependencies_.end(), relative) == dependencies_.end())
            dependencies_.push_back(relative);
        return Assimp::DefaultIOSystem::Open(path.string().c_str(), mode);
    }
};
TextureAsset Decode(const std::string &name, const std::uint8_t *data, std::size_t size)
{
    if (size > 128 * 1024 * 1024)
        throw std::runtime_error("texture source too large");
    int width = 0, height = 0, channels = 0;
    auto *pixels = stbi_load_from_memory(data, int(size), &width, &height, &channels, 4);
    if (!pixels)
        throw std::runtime_error("texture decode failed: " + name);
    std::unique_ptr<unsigned char, decltype(&stbi_image_free)> guard(pixels, &stbi_image_free);
    TextureAsset t;
    t.name = name;
    t.width = unsigned(width);
    t.height = unsigned(height);
    t.rgba.assign(pixels, pixels + std::size_t(width) * height * 4);
    return t;
}
} // namespace
ModelAsset ImportModel(const std::filesystem::path &source)
{
    auto full = std::filesystem::canonical(source);
    auto ext = full.extension().string();
    if (ext != ".gltf" && ext != ".glb")
        throw std::invalid_argument("this importer profile supports glTF 2.0 / GLB");
    ModelAsset out;
    Assimp::Importer importer;
    importer.SetIOHandler(new ImportIO(full.parent_path(), out.dependencies));
    const auto *scene = importer.ReadFile(
        full.string(), aiProcess_FlipUVs | aiProcess_Triangulate | aiProcess_GenSmoothNormals |
                           aiProcess_CalcTangentSpace | aiProcess_LimitBoneWeights |
                           aiProcess_ValidateDataStructure);
    if (!scene || !scene->mRootNode)
        throw std::runtime_error(std::string("Assimp import failed: ") + importer.GetErrorString());
    std::unordered_map<std::string, unsigned> names;
    std::vector<const aiNode *> nodes;
    std::function<void(const aiNode *, int)> visit = [&](const aiNode *node, int parent) {
        if (nodes.size() >= 1024)
            throw std::runtime_error("model exceeds 1024 nodes");
        unsigned index = unsigned(nodes.size());
        nodes.push_back(node);
        AssetNode n;
        n.name = node->mName.C_Str();
        n.parent = parent;
        aiVector3D scale, pos;
        aiQuaternion rot;
        node->mTransformation.Decompose(scale, rot, pos);
        n.local = {V(pos), V(scale), {rot.x, rot.y, rot.z, rot.w}};
        auto decomposed = Matrix(n.local), original = M(node->mTransformation);
        for (unsigned k = 0; k < 16; ++k)
            if (std::abs(decomposed.m[k] - original.m[k]) > .001f)
                throw std::runtime_error("sheared node transform unsupported");
        // Duplicate names are legal for static nodes, but ambiguous animation/bone binding is
        // rejected below.
        if (!names.emplace(n.name, index).second)
            names[n.name] = ~0u;
        out.skeleton.joints.push_back(n);
        for (unsigned i = 0; i < node->mNumChildren; ++i)
            visit(node->mChildren[i], int(index));
    };
    visit(scene->mRootNode, -1);
    auto nodeFor = [&](const aiString &name) {
        auto it = names.find(name.C_Str());
        if (it == names.end() || it->second == ~0u)
            throw std::runtime_error("missing/ambiguous joint or animation node");
        return it->second;
    };
    std::unordered_map<std::string, int> textures;
    for (unsigned i = 0; i < scene->mNumMaterials; ++i)
    {
        const auto &material = *scene->mMaterials[i];
        MaterialAsset m;
        aiString name;
        material.Get(AI_MATKEY_NAME, name);
        m.name = name.C_Str();
        aiColor4D colour{1, 1, 1, 1};
        if (material.Get(AI_MATKEY_BASE_COLOR, colour) != AI_SUCCESS)
            material.Get(AI_MATKEY_COLOR_DIFFUSE, colour);
        m.colour = {colour.r, colour.g, colour.b};
        material.Get(AI_MATKEY_ROUGHNESS_FACTOR, m.roughness);
        material.Get(AI_MATKEY_METALLIC_FACTOR, m.metallic);
        auto loadTexture = [&](aiTextureType type, int channel = -1) {
            aiString texture;
            unsigned uv = 0;
            bool has = material.GetTexture(type, 0, &texture, nullptr, &uv) == AI_SUCCESS;
            if (uv != 0)
                throw std::runtime_error("material requires unsupported UV set");
            if (has)
            {
                std::string key = texture.C_Str();
                auto found = textures.find(key);
                if (found == textures.end())
                {
                    TextureAsset t;
                    if (auto *embedded = scene->GetEmbeddedTexture(key.c_str()))
                    {
                        if (embedded->mHeight == 0)
                            t = Decode(key,
                                       reinterpret_cast<const std::uint8_t *>(embedded->pcData),
                                       embedded->mWidth);
                        else
                        {
                            t.name = key;
                            t.width = embedded->mWidth;
                            t.height = embedded->mHeight;
                            if (std::uint64_t(t.width) * t.height > 8192ull * 8192)
                                throw std::runtime_error("embedded texture too large");
                            for (std::size_t p = 0; p < std::size_t(t.width) * t.height; ++p)
                            {
                                auto c = embedded->pcData[p];
                                t.rgba.insert(t.rgba.end(), {c.r, c.g, c.b, c.a});
                            }
                        }
                    }
                    else
                    {
                        auto path = std::filesystem::weakly_canonical(full.parent_path() / key);
                        auto relative = path.lexically_relative(full.parent_path());
                        if (relative.empty() || *relative.begin() == "..")
                            throw std::runtime_error("texture escapes source directory");
                        auto size = std::filesystem::file_size(path);
                        if (size > 128 * 1024 * 1024)
                            throw std::runtime_error("texture too large");
                        std::vector<std::uint8_t> data(size);
                        std::ifstream f(path, std::ios::binary);
                        f.read(reinterpret_cast<char *>(data.data()), std::streamsize(size));
                        if (!f)
                            throw std::runtime_error("texture read failed");
                        t = Decode(key, data.data(), data.size());
                        out.dependencies.push_back(relative.generic_string());
                    }
                    int index = int(out.textures.size());
                    out.textures.push_back(std::move(t));
                    found = textures.emplace(key, index).first;
                }
                if (channel < 0)
                    return found->second;
                auto t = out.textures[found->second];
                t.name += "/channel" + std::to_string(channel);
                for (std::size_t pixel = 0; pixel < t.rgba.size(); pixel += 4)
                {
                    auto value = t.rgba[pixel + channel];
                    t.rgba[pixel] = t.rgba[pixel + 1] = t.rgba[pixel + 2] = value;
                    t.rgba[pixel + 3] = 255;
                }
                out.textures.push_back(std::move(t));
                return int(out.textures.size() - 1);
            }
            return -1;
        };
        m.baseColourTexture = loadTexture(aiTextureType_BASE_COLOR);
        if (m.baseColourTexture < 0)
            m.baseColourTexture = loadTexture(aiTextureType_DIFFUSE);
        m.normalTexture = loadTexture(aiTextureType_NORMALS);
        material.Get(AI_MATKEY_GLTF_TEXTURE_SCALE(aiTextureType_NORMALS, 0), m.normalScale);
        m.metallicTexture = loadTexture(aiTextureType_METALNESS, 2);
        m.roughnessTexture = loadTexture(aiTextureType_DIFFUSE_ROUGHNESS, 1);
        m.occlusionTexture = loadTexture(aiTextureType_LIGHTMAP, 0);
        m.emissiveTexture = loadTexture(aiTextureType_EMISSIVE);
        aiColor3D emission{0, 0, 0};
        material.Get(AI_MATKEY_COLOR_EMISSIVE, emission);
        float intensity = 1;
        material.Get(AI_MATKEY_EMISSIVE_INTENSITY, intensity);
        m.emissive = {emission.r * intensity, emission.g * intensity, emission.b * intensity};
        float strength = 1;
        material.Get(AI_MATKEY_GLTF_TEXTURE_STRENGTH(aiTextureType_LIGHTMAP, 0), strength);
        if (m.occlusionTexture >= 0)
        {
            auto &t = out.textures[m.occlusionTexture];
            for (std::size_t pixel = 0; pixel < t.rgba.size(); pixel += 4)
            {
                auto value = std::uint8_t(
                    std::clamp(255.f * (1 - strength) + t.rgba[pixel] * strength, 0.f, 255.f));
                t.rgba[pixel] = t.rgba[pixel + 1] = t.rgba[pixel + 2] = value;
            }
        }
        if (colour.a < .999f)
            out.warnings.push_back("Material alpha is not rendered in this opaque profile: " +
                                   m.name);
        out.materials.push_back(m);
    }
    for (unsigned node = 0; node < nodes.size(); ++node)
        for (unsigned index = 0; index < nodes[node]->mNumMeshes; ++index)
        {
            const auto &src = *scene->mMeshes[nodes[node]->mMeshes[index]];
            if (src.mNumAnimMeshes)
                throw std::runtime_error("morph targets are not supported in this asset profile");
            MeshAsset m;
            m.name = src.mName.C_Str();
            m.node = node;
            m.material = src.mMaterialIndex;
            for (unsigned i = 0; i < src.mNumVertices; ++i)
            {
                auto uv = src.HasTextureCoords(0) ? src.mTextureCoords[0][i] : aiVector3D{};
                m.geometry.vertices.push_back(
                    {V(src.mVertices[i]), V(src.mNormals[i]), uv.x, uv.y});
                if (src.HasTangentsAndBitangents())
                {
                    m.tangents.push_back(V(src.mTangents[i]));
                    auto &v = m.geometry.vertices.back();
                    v.tangent = V(src.mTangents[i]);
                    v.tangentSign = ((src.mNormals[i] ^ src.mTangents[i]) * src.mBitangents[i]) < 0
                                        ? -1.f
                                        : 1.f;
                }
            }
            for (unsigned i = 0; i < src.mNumFaces; ++i)
            {
                auto &face = src.mFaces[i];
                if (face.mNumIndices != 3)
                    throw std::runtime_error("nontriangle primitive unsupported");
                m.geometry.indices.insert(m.geometry.indices.end(), face.mIndices,
                                          face.mIndices + 3);
            }
            if (src.HasBones())
            {
                m.skin.resize(src.mNumVertices);
                m.inverseBind.resize(nodes.size());
                for (unsigned i = 0; i < src.mNumBones; ++i)
                {
                    auto &bone = *src.mBones[i];
                    auto joint = nodeFor(bone.mName);
                    m.inverseBind[joint] = M(bone.mOffsetMatrix);
                    for (unsigned k = 0; k < bone.mNumWeights; ++k)
                    {
                        auto weight = bone.mWeights[k];
                        if (weight.mVertexId >= m.skin.size())
                            throw std::runtime_error("invalid bone vertex");
                        auto &skin = m.skin[weight.mVertexId];
                        bool inserted = false;
                        for (unsigned slot = 0; slot < 4; ++slot)
                            if (skin.weights[slot] == 0)
                            {
                                skin.joints[slot] = std::uint16_t(joint);
                                skin.weights[slot] = weight.mWeight;
                                inserted = true;
                                break;
                            }
                        if (!inserted && weight.mWeight > 0)
                            throw std::runtime_error("more than four bone influences after import");
                    }
                }
                for (auto &skin : m.skin)
                {
                    float sum = 0;
                    for (float w : skin.weights)
                        sum += w;
                    if (sum <= 0)
                    {
                        skin.joints[0] = std::uint16_t(node);
                        skin.weights[0] = 1;
                    }
                    else
                        for (auto &w : skin.weights)
                            w /= sum;
                }
            }
            out.meshes.push_back(std::move(m));
        }
    for (unsigned i = 0; i < scene->mNumAnimations; ++i)
    {
        auto &src = *scene->mAnimations[i];
        if (src.mNumMeshChannels || src.mNumMorphMeshChannels)
            throw std::runtime_error("mesh/morph animation unsupported");
        double ticks = src.mTicksPerSecond > 0 ? src.mTicksPerSecond : 25;
        AnimationClip c;
        c.name = src.mName.C_Str();
        c.duration = float(src.mDuration / ticks);
        c.tracks.resize(nodes.size());
        for (unsigned k = 0; k < nodes.size(); ++k)
        {
            auto &rest = out.skeleton.joints[k].local;
            c.tracks[k].translations = {{0, rest.translation}};
            c.tracks[k].rotations = {{0, rest.rotation}};
            c.tracks[k].scales = {{0, rest.scale}};
        }
        for (unsigned k = 0; k < src.mNumChannels; ++k)
        {
            auto &channel = *src.mChannels[k];
            auto supported = [](auto *keys, unsigned count) {
                for (unsigned key = 0; key < count; ++key)
                    if (keys[key].mInterpolation != aiAnimInterpolation_Linear &&
                        keys[key].mInterpolation != aiAnimInterpolation_Spherical_Linear)
                        throw std::runtime_error("STEP/CUBICSPLINE animation requires baking to "
                                                 "linear keys before import");
            };
            supported(channel.mPositionKeys, channel.mNumPositionKeys);
            supported(channel.mRotationKeys, channel.mNumRotationKeys);
            supported(channel.mScalingKeys, channel.mNumScalingKeys);
            auto &t = c.tracks[nodeFor(channel.mNodeName)];
            if (channel.mNumPositionKeys)
            {
                t.translations.clear();
                for (unsigned j = 0; j < channel.mNumPositionKeys; ++j)
                    t.translations.push_back({float(channel.mPositionKeys[j].mTime / ticks),
                                              V(channel.mPositionKeys[j].mValue)});
            }
            if (channel.mNumScalingKeys)
            {
                t.scales.clear();
                for (unsigned j = 0; j < channel.mNumScalingKeys; ++j)
                    t.scales.push_back({float(channel.mScalingKeys[j].mTime / ticks),
                                        V(channel.mScalingKeys[j].mValue)});
            }
            if (channel.mNumRotationKeys)
            {
                t.rotations.clear();
                for (unsigned j = 0; j < channel.mNumRotationKeys; ++j)
                {
                    auto q = channel.mRotationKeys[j].mValue;
                    t.rotations.push_back(
                        {float(channel.mRotationKeys[j].mTime / ticks), {q.x, q.y, q.z, q.w}});
                }
            }
        }
        out.animations.push_back(std::move(c));
    }
    out.metadata.type = AssetType::Model;
    Validate(out);
    return out;
}
} // namespace PlagC
