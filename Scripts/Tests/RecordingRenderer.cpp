// CPU-only test backend. It records state/validates handle lifetimes; it is NOT Ogre
// and cannot measure GPU/render-driver costs or verify pixels.
#include <PlagC/Renderer/Renderer.h>
#include <unordered_map>
#include <filesystem>
#include <stdexcept>
#include <unordered_set>
namespace PlagC
{
namespace
{
std::uint64_t next = 1;
Transform lastTransform;
} // namespace
Transform RecordedLastTransform()
{
    return lastTransform;
}
struct Renderer::Impl
{
    struct Object
    {
        MeshHandle mesh;
        MaterialHandle material;
        Transform transform;
        bool visible = true, shadow = true;
        std::uint8_t queue = 10;
    };
    std::unordered_map<std::uint64_t, MeshData> meshes;
    std::unordered_map<std::uint64_t, MaterialDesc> materials;
    std::unordered_map<std::uint64_t, Object> objects;
    std::unordered_map<std::uint64_t, LightDesc> lights;
    std::unordered_map<std::uint64_t, std::vector<std::uint8_t>> textures;
    std::unordered_set<std::uint64_t> dynamic;
    CameraDesc camera;
    RenderSettings settings;
    EnvironmentDesc environment;
    std::uint64_t frames = 0;
};
RendererBackendCapabilities Renderer::BackendCapabilities()
{
    return {true, false, true}; // Recording test double models the native build's choices.
}
Renderer::Renderer(Window *, const RendererConfig &config) : impl_(std::make_unique<Impl>())
{
    backend_ = ResolveRendererBackend(config.backend, BackendCapabilities(), config.nullBackend,
                                      config.offscreen);
}
Renderer::~Renderer() = default;
MeshHandle Renderer::CreateMesh(const MeshData &d, bool dynamic)
{
    MeshHandle h{next++};
    impl_->meshes.emplace(h.value, d);
    if (dynamic)
        impl_->dynamic.insert(h.value);
    return h;
}
void Renderer::DestroyMesh(MeshHandle h)
{
    for (auto &[id, o] : impl_->objects)
        if (o.mesh.value == h.value)
            throw std::runtime_error("mesh in use");
    if (!impl_->meshes.erase(h.value))
        throw std::runtime_error("stale mesh");
    impl_->dynamic.erase(h.value);
}
MaterialHandle Renderer::CreateMaterial(const MaterialDesc &d)
{
    MaterialHandle h{next++};
    impl_->materials.emplace(h.value, d);
    return h;
}
void Renderer::UpdateMaterial(MaterialHandle handle, const MaterialDesc &desc)
{
    auto it = impl_->materials.find(handle.value);
    if (it == impl_->materials.end())
        throw std::runtime_error("stale material");
    it->second = desc;
}
void Renderer::DestroyMaterial(MaterialHandle h)
{
    for (auto &[id, o] : impl_->objects)
        if (o.material.value == h.value)
            throw std::runtime_error("material in use");
    if (!impl_->materials.erase(h.value))
        throw std::runtime_error("stale material");
}
ObjectHandle Renderer::CreateObject(MeshHandle mesh, MaterialHandle material, const Transform &t)
{
    impl_->meshes.at(mesh.value);
    impl_->materials.at(material.value);
    ObjectHandle h{next++};
    impl_->objects.emplace(h.value, Impl::Object{mesh, material, t});
    return h;
}
void Renderer::DestroyObject(ObjectHandle h)
{
    if (!impl_->objects.erase(h.value))
        throw std::runtime_error("stale object");
}
void Renderer::SetTransform(ObjectHandle h, const Transform &t)
{
    impl_->objects.at(h.value).transform = t;
    lastTransform = t;
}
void Renderer::SetVisible(ObjectHandle h, bool b)
{
    impl_->objects.at(h.value).visible = b;
}
void Renderer::SetCastShadows(ObjectHandle h, bool b)
{
    impl_->objects.at(h.value).shadow = b;
}
void Renderer::SetRenderQueue(ObjectHandle h, std::uint8_t q)
{
    impl_->objects.at(h.value).queue = q;
}
LightHandle Renderer::CreateLight(const LightDesc &d)
{
    LightHandle h{next++};
    impl_->lights.emplace(h.value, d);
    return h;
}
void Renderer::DestroyLight(LightHandle h)
{
    if (!impl_->lights.erase(h.value))
        throw std::runtime_error("stale light");
}
void Renderer::UpdateLight(LightHandle h, const LightDesc &d)
{
    impl_->lights.at(h.value) = d;
}
void Renderer::SetCamera(const CameraDesc &c)
{
    impl_->camera = c;
}
RenderStats Renderer::Statistics() const
{
    RenderStats s;
    s.frames = impl_->frames;
    s.objects = impl_->objects.size();
    s.meshes = impl_->meshes.size();
    s.materials = impl_->materials.size();
    s.lights = impl_->lights.size();
    s.textures = impl_->textures.size();
    return s;
}
void Renderer::SetSettings(const RenderSettings &s)
{
    impl_->settings = s;
}
RenderSettings Renderer::Settings() const
{
    return impl_->settings;
}
void Renderer::SetVSync(bool value)
{
    impl_->settings.vsync = value;
}
void Renderer::SetEnvironment(const EnvironmentDesc &e)
{
    impl_->environment = e;
}
EnvironmentDesc Renderer::Environment() const
{
    return impl_->environment;
}
void Renderer::Render(double)
{
    ++impl_->frames;
}
void Renderer::Resize(unsigned, unsigned)
{
}
void Renderer::UpdateMesh(MeshHandle h, std::span<const Vertex> vertices)
{
    auto &mesh = impl_->meshes.at(h.value);
    if (!impl_->dynamic.contains(h.value) || mesh.vertices.size() != vertices.size())
        throw std::invalid_argument("invalid mesh update");
    mesh.vertices.assign(vertices.begin(), vertices.end());
}
TextureHandle Renderer::CreateTexture(unsigned w, unsigned h, std::span<const std::uint8_t> bytes,
                                      bool, bool)
{
    if (!w || !h || bytes.size() != std::uint64_t(w) * h * 4)
        throw std::invalid_argument("invalid texture");
    TextureHandle handle{next++};
    impl_->textures[handle.value] = {bytes.begin(), bytes.end()};
    return handle;
}
// Recording-only texture stand-in; the native AssetSlice test validates decoding and pixels.
TextureHandle Renderer::LoadTexture(const std::string &resource, bool srgb, bool mipmaps)
{
    if (!std::filesystem::is_regular_file(std::filesystem::path("Assets/Textures") / resource))
        throw std::runtime_error("recording texture resource missing: " + resource);
    const std::uint8_t pixel[] = {255, 255, 255, 255};
    return CreateTexture(1, 1, pixel, srgb, mipmaps);
}
void Renderer::DestroyTexture(TextureHandle h)
{
    for (auto &[id, m] : impl_->materials)
        if (m.baseColourTexture.value == h.value)
            throw std::logic_error("texture in use");
    if (!impl_->textures.erase(h.value))
        throw std::invalid_argument("stale texture");
}

} // namespace PlagC
