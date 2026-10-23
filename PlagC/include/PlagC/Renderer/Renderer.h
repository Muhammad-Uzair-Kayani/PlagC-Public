#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
namespace PlagC
{
class Window;
struct Vec3
{
    float x = 0, y = 0, z = 0;
};
struct Transform
{
    Vec3 position, rotationDegrees, scale{1, 1, 1};
};
struct Vertex
{
    Vec3 position, normal;
    float u = 0, v = 0;
};
struct MeshData
{
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
};
enum class Primitive
{
    Cube,
    Plane,
    Sphere,
    Cylinder
};
MeshData MakePrimitive(Primitive);
template <class Tag> struct RenderHandle
{
    std::uint64_t value = 0;
    explicit operator bool() const
    {
        return value != 0;
    }
};
using MeshHandle = RenderHandle<struct MeshTag>;
using MaterialHandle = RenderHandle<struct MaterialTag>;
using ObjectHandle = RenderHandle<struct ObjectTag>;
using LightHandle = RenderHandle<struct LightTag>;
using TargetHandle = RenderHandle<struct TargetTag>;
enum class MaterialModel
{
    Pbr,
    Unlit
};
struct MaterialDesc
{
    MaterialModel model = MaterialModel::Pbr;
    Vec3 colour{.7f, .7f, .7f};
    float roughness = .5f, metallic = 0;
    std::string colourTexture;
};
enum class LightType
{
    Directional,
    Point,
    Spot
};
struct LightDesc
{
    LightType type = LightType::Directional;
    Vec3 position{0, 5, 0}, direction{-1, -1, -1}, colour{1, 1, 1};
    float power = 3.141593f, radius = 20, innerAngle = 25, outerAngle = 45;
};
struct CameraDesc
{
    Vec3 position{8, 6, 10}, target{0, 0, 0};
    float fov = 60, nearClip = .1f, farClip = 1000;
};
struct RenderSettings
{
    bool wireframe = false, vsync = true, shadows = true;
    Vec3 background{.035f, .055f, .09f};
};
// Resource name in Assets/Textures. Empty cubemap disables the environment.
// Supply a mipmapped DDS cubemap; sky and PBR specular reflections are independent.
struct EnvironmentDesc
{
    std::string cubemap;
    bool skybox = true, reflections = true;
};
struct RenderStats
{
    std::uint64_t frames = 0;
    std::size_t objects = 0, meshes = 0, materials = 0, lights = 0;
    double cpuMilliseconds = 0;
};
struct RendererConfig
{
    bool nullBackend = false, offscreen = false;
    std::string mediaRoot = "Vendor/ogre-next/Samples/Media", assetsRoot = "Assets";
};
// Owner-thread API. Handles are instance-specific and never reused.
class Renderer
{
  public:
    Renderer(Window *, const RendererConfig &);
    ~Renderer();
    Renderer(const Renderer &) = delete;
    Renderer &operator=(const Renderer &) = delete;
    MeshHandle CreateMesh(const MeshData &);
    MeshHandle LoadModel(const std::string &, bool legacyMesh = true);
    void DestroyMesh(MeshHandle);
    MaterialHandle CreateMaterial(const MaterialDesc &);
    void UpdateMaterial(MaterialHandle, const MaterialDesc &);
    void DestroyMaterial(MaterialHandle);
    ObjectHandle CreateObject(MeshHandle, MaterialHandle, const Transform & = {});
    void SetTransform(ObjectHandle, const Transform &);
    void SetVisible(ObjectHandle, bool);
    void SetCastShadows(ObjectHandle, bool);
    void SetRenderQueue(ObjectHandle, std::uint8_t);
    void DestroyObject(ObjectHandle);
    LightHandle CreateLight(const LightDesc &);
    void UpdateLight(LightHandle, const LightDesc &);
    void DestroyLight(LightHandle);
    void SetCamera(const CameraDesc &);
    void SetSettings(const RenderSettings &);
    void SetEnvironment(const EnvironmentDesc &);
    EnvironmentDesc Environment() const;
    std::vector<std::string> Animations(ObjectHandle) const;
    void PlayAnimation(ObjectHandle, const std::string &, bool loop = true, float speed = 1);
    void StopAnimation(ObjectHandle);
    TargetHandle CreateTarget(unsigned, unsigned);
    void ResizeTarget(TargetHandle, unsigned, unsigned);
    void DestroyTarget(TargetHandle);
    void CaptureTarget(TargetHandle, const std::string &path);
    void Resize(unsigned, unsigned);
    void Render(double deltaSeconds);
    RenderStats Statistics() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace PlagC
