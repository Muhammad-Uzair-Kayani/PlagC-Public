#pragma once
#include <PlagC/Renderer/Renderer.h>
#include <array>
#include <filesystem>
#include <span>
#include <stdexcept>
namespace PlagC
{
struct AssetID
{
    std::uint64_t value = 0;
    explicit operator bool() const
    {
        return value != 0;
    }
    bool operator==(const AssetID &) const = default;
};
enum class AssetType : std::uint32_t
{
    Model = 1,
    Audio = 2
};
struct AssetMetadata
{
    AssetID id;
    AssetType type;
    std::string path;
    std::uint64_t generation = 1, bytes = 0;
};
struct Asset
{
    AssetMetadata metadata;
    virtual ~Asset() = default;
};
// Column-major matrices, right-handed Y-up coordinates, metres, seconds.
struct Matrix4
{
    std::array<float, 16> m{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
};
struct Quaternion
{
    float x = 0, y = 0, z = 0, w = 1;
};
struct JointTransform
{
    Vec3 translation{}, scale{1, 1, 1};
    Quaternion rotation{};
};
Matrix4 Multiply(const Matrix4 &, const Matrix4 &);
Matrix4 Matrix(const JointTransform &);
Vec3 TransformPoint(const Matrix4 &, Vec3);
Vec3 TransformNormal(const Matrix4 &, Vec3);
struct TextureAsset
{
    std::string name;
    unsigned width = 0, height = 0;
    std::vector<std::uint8_t> rgba;
};
struct MaterialAsset
{
    std::string name;
    Vec3 colour{1, 1, 1};
    float roughness = .5f, metallic = 0;
    int baseColourTexture = -1;
};
struct VertexSkin
{
    std::array<std::uint16_t, 4> joints{};
    std::array<float, 4> weights{};
};
struct MeshAsset
{
    std::string name;
    MeshData geometry;
    std::vector<Vec3> tangents;
    std::vector<VertexSkin> skin;
    std::vector<Matrix4> inverseBind;
    unsigned node = 0, material = 0;
};
struct AssetNode
{
    std::string name;
    int parent = -1;
    JointTransform local;
};
struct SkeletonAsset
{
    std::vector<AssetNode> joints;
};
struct VectorKey
{
    float time = 0;
    Vec3 value;
};
struct RotationKey
{
    float time = 0;
    Quaternion value;
};
struct JointTrack
{
    std::vector<VectorKey> translations, scales;
    std::vector<RotationKey> rotations;
};
struct AnimationClip
{
    std::string name;
    float duration = 1;
    std::vector<JointTrack> tracks;
};
struct ModelAsset final : Asset
{
    static constexpr AssetType Type = AssetType::Model;
    std::vector<MeshAsset> meshes;
    std::vector<MaterialAsset> materials;
    std::vector<TextureAsset> textures;
    SkeletonAsset skeleton;
    std::vector<AnimationClip> animations;
    std::vector<std::string> dependencies, warnings;
};
struct AudioClip final : Asset
{
    static constexpr AssetType Type = AssetType::Audio;
    unsigned sampleRate = 0, channels = 0;
    std::vector<std::int16_t> samples;
};
template <class T> class AssetHandle
{
    std::shared_ptr<const T> value_;

  public:
    AssetHandle() = default;
    explicit AssetHandle(std::shared_ptr<const T> value) : value_(std::move(value))
    {
    }
    const T *operator->() const
    {
        if (!value_)
            throw std::logic_error("empty asset handle");
        return value_.get();
    }
    const T &operator*() const
    {
        return *operator->();
    }
    explicit operator bool() const
    {
        return bool(value_);
    }
    const std::shared_ptr<const T> &Shared() const
    {
        return value_;
    }
};
void Validate(const ModelAsset &);
void Validate(const AudioClip &);
// Import/cook are CPU-only. Assimp exists only inside ImportModel's implementation.
ModelAsset ImportModel(const std::filesystem::path &);
AudioClip ImportWave(const std::filesystem::path &);
void SaveAsset(const ModelAsset &, const std::filesystem::path &);
void SaveAsset(const AudioClip &, const std::filesystem::path &);
std::shared_ptr<Asset> ReadAsset(const std::filesystem::path &);
} // namespace PlagC
