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
    explicit operator bool() const { return value != 0; }
    bool operator==(const AssetID &) const = default;
};
enum class AssetType : std::uint32_t
{
    Model = 1,
    Audio = 2,
    Texture = 3
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
struct Quaternion { float x = 0, y = 0, z = 0, w = 1; };
struct JointTransform
{
    Vec3 translation{}, scale{1, 1, 1};
    Quaternion rotation{};
};
Matrix4 Multiply(const Matrix4 &, const Matrix4 &);
Matrix4 Matrix(const JointTransform &);
Vec3 TransformPoint(const Matrix4 &, Vec3);
Vec3 TransformNormal(const Matrix4 &, Vec3);
struct TextureAsset final : Asset
{
    static constexpr AssetType Type = AssetType::Texture;
    std::string name;
    unsigned width = 0, height = 0;
    std::vector<std::uint8_t> rgba;
};
// Renderer-neutral PBS material value. Imported ModelAsset materials use the integer texture
// indices; standalone authored .pgmaterial assets use the path fields and assetPath identity.
struct MaterialAsset
{
    std::string name = "Material";
    std::string assetPath;
    Vec3 colour{1, 1, 1};
    float roughness = .5f, metallic = 0;
    int baseColourTexture = -1;
    int normalTexture = -1, metallicTexture = -1, roughnessTexture = -1, occlusionTexture = -1,
        emissiveTexture = -1;
    std::string baseColourTexturePath;
    std::string normalTexturePath;
    std::string metallicTexturePath;
    std::string roughnessTexturePath;
    std::string occlusionTexturePath;
    Vec3 emissive{};
    std::string emissiveTexturePath;
    float normalScale = 1;
    float alphaCutoff = 0;
    bool doubleSided = false;
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
struct SkeletonAsset { std::vector<AssetNode> joints; };
struct VectorKey { float time = 0; Vec3 value; };
struct RotationKey { float time = 0; Quaternion value; };
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

enum class ModelNormalImport : std::uint8_t { PreserveOrGenerate, RecalculateSmooth };
enum class ModelCoordinateImport : std::uint8_t { RightHandedYUp, MirrorZToRightHanded };
struct ModelImportOptions
{
    float scale = 1.0f;
    ModelCoordinateImport coordinates = ModelCoordinateImport::RightHandedYUp;
    bool flipUVs = true;
    ModelNormalImport normals = ModelNormalImport::PreserveOrGenerate;
    bool tangents = true;
    bool materials = true;
    bool animations = true;
};
struct TextureImportOptions { bool flipVertical = false; };

template <class T> class AssetHandle
{
    std::shared_ptr<const T> value_;
  public:
    AssetHandle() = default;
    explicit AssetHandle(std::shared_ptr<const T> value) : value_(std::move(value)) {}
    const T *operator->() const
    {
        if (!value_) throw std::logic_error("empty asset handle");
        return value_.get();
    }
    const T &operator*() const { return *operator->(); }
    explicit operator bool() const { return bool(value_); }
    const std::shared_ptr<const T> &Shared() const { return value_; }
};
void Validate(const ModelAsset &);
void Validate(const AudioClip &);
void Validate(const TextureAsset &);
void Validate(const MaterialAsset &);
// Import/cook are CPU-only. Assimp exists only inside ImportModel's implementation.
ModelAsset ImportModel(const std::filesystem::path &);
ModelAsset ImportModel(const std::filesystem::path &, const ModelImportOptions &);
TextureAsset ImportTexture(const std::filesystem::path &, const TextureImportOptions & = {});
AudioClip ImportWave(const std::filesystem::path &);
void SaveAsset(const ModelAsset &, const std::filesystem::path &);
void SaveAsset(const AudioClip &, const std::filesystem::path &);
void SaveAsset(const TextureAsset &, const std::filesystem::path &);
std::shared_ptr<Asset> ReadAsset(const std::filesystem::path &);

// Standalone authored material persistence. Paths are logical Assets/Cooked-relative identities;
// texture paths are resource names relative to the Assets resource root.
void SaveMaterialAsset(const MaterialAsset &, const std::filesystem::path &);
AssetHandle<MaterialAsset> LoadMaterialAsset(const std::filesystem::path &);
} // namespace PlagC
