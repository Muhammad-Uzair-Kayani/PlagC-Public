#include "pgcpch.h"
#include "UiSubmissionInternal.h"
#include <PlagC/Renderer/Renderer.h>
#include <PlagC/Window.h>
#include <PlagC/Thread.h>
// Warnings in the pinned vendor headers must not weaken checks in engine code.
#ifdef _MSC_VER
#pragma warning(push)
// Vendor headers contain CRT calls, intentional padding/narrowing and
// inconsistent WIN32_LEAN_AND_MEAN definitions in generated GL headers.
#pragma warning(disable : 4005 4244 4324 4996)
#endif
#include <OgreMaterialManager.h>
#include <OgreMaterial.h>
#include <OgreTechnique.h>
#include <OgrePass.h>
#include <OgreTextureUnitState.h>
#include <Compositor/OgreCompositorNodeDef.h>
#include <Compositor/OgreCompositorWorkspaceDef.h>
#include <Compositor/Pass/PassScene/OgreCompositorPassSceneDef.h>
#include <OgreGpuProgramParams.h>
#include <Compositor/Pass/PassQuad/OgreCompositorPassQuadDef.h>
#include <OgreRoot.h>
#include <OgreWorkQueue.h>
#include <OgreImage2.h>
#include <OgreTextureBox.h>
#include <OgrePixelFormatGpuUtils.h>
#include <cstring>
#include <OgreWindow.h>
#include <OgreRenderSystem.h>
#include <OgreSceneManager.h>
#include <OgreRenderQueue.h>
#include <OgreSceneNode.h>
#include <OgreCamera.h>
#include <OgreItem.h>
#include <OgreLight.h>
#include <OgreMeshManager.h>
#include <OgreMeshManager2.h>
#include <OgreMesh2.h>
#include <OgreSubMesh2.h>
#include <OgreArchiveManager.h>
#include <OgreResourceGroupManager.h>
#include <OgreHlmsManager.h>
#include <OgreHlmsPbs.h>
#include <OgreRootLayout.h>
#include <OgreRenderable.h>
#include <OgreHlmsPbsDatablock.h>
#include <OgreHlmsUnlit.h>
#include <OgreHlmsUnlitDatablock.h>
#include <OgreTextureGpu.h>
#include <OgreTextureGpuManager.h>
#include <OgreNULLPlugin.h>
#ifdef PC_OGRE_VULKAN
#include <OgreVulkanPlugin.h>
#endif
#ifndef PC_OGRE_NULL_ONLY
#include <OgreGL3PlusPlugin.h>
#endif
#include <Compositor/OgreCompositorManager2.h>
#include <Animation/OgreSkeletonInstance.h>
#include <Animation/OgreSkeletonAnimation.h>
#include <Vao/OgreVaoManager.h>
#include <Vao/OgreVertexArrayObject.h>
#include <Vao/OgreVertexBufferPacked.h>
#include <Vao/OgreIndexBufferPacked.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif
namespace PlagC
{
namespace
{
std::atomic<std::uint64_t> sequence{1};
std::uint64_t ID()
{
    return sequence.fetch_add(1);
}
Ogre::Vector3 V(Vec3 v)
{
    return {v.x, v.y, v.z};
}
Ogre::ColourValue C(Vec3 v)
{
    return Ogre::ColourValue(v.x, v.y, v.z, 1);
}
bool Finite(Vec3 v)
{
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
void Require(bool ok, const char *text)
{
    if (!ok)
        throw std::invalid_argument(text);
}
template <class Map, class Handle> auto &Get(Map &map, Handle h)
{
    Thread::RequireMain();
    auto i = map.find(h.value);
    if (i == map.end())
        throw std::invalid_argument("invalid or stale renderer handle");
    return i->second;
}
std::string Name(const char *kind, std::uint64_t id)
{
    return std::string("PlagC/") + kind + std::to_string(id);
}
#ifdef PC_OGRE_VULKAN
// The pinned PBS caster layout omits the reflection texture, but its baked
// descriptor set still contains it. Reserve that unused trailing binding.
class VulkanPbs final : public Ogre::HlmsPbs
{
  public:
    using Ogre::HlmsPbs::HlmsPbs;
  protected:
    void calculateHashForPreCaster(Ogre::Renderable* renderable, Ogre::PiecesMap* pieces,
                                   const Ogre::PiecesMap* normalPieces) override
    {
        Ogre::HlmsPbs::calculateHashForPreCaster(renderable, pieces, normalPieces);
        auto* material = static_cast<Ogre::HlmsPbsDatablock*>(renderable->getDatablock());
        setProperty("plagc_shadow_reflection_binding",
                    material->getAlphaTest() != Ogre::CMPF_ALWAYS_PASS &&
                    material->getUseAlphaFromTextures() &&
                    material->getTexture(Ogre::PBSM_REFLECTION) ? 1 : 0);
    }
    void setupRootLayout(Ogre::RootLayout& layout) override
    {
        Ogre::HlmsPbs::setupRootLayout(layout);
        if (getProperty("plagc_shadow_reflection_binding"))
            ++layout.mDescBindingRanges[1][Ogre::DescBindingTypes::Texture].end;
    }
};
#endif
template <class T> T *RegisterHlms(Ogre::Root &root, const std::string &media)
{
    Ogre::String main;
    Ogre::StringVector libs;
    T::getDefaultPaths(main, libs);
    auto &archives = Ogre::ArchiveManager::getSingleton();
    Ogre::ArchiveVec paths;
    for (const auto &lib : libs)
        paths.push_back(archives.load(media + "/" + lib, "FileSystem", true));
    if constexpr (std::is_same_v<T, Ogre::HlmsPbs>)
        paths.push_back(archives.load("Assets/Shaders/Pbs", "FileSystem", true));
    auto* folder = archives.load(media + "/" + main, "FileSystem", true);
    std::unique_ptr<T> result;
#ifdef PC_OGRE_VULKAN
    if constexpr (std::is_same_v<T, Ogre::HlmsPbs>)
        if (root.getRenderSystem()->getName() == "Vulkan Rendering Subsystem")
            result = std::make_unique<VulkanPbs>(folder, &paths);
#endif
    if (!result)
        result = std::make_unique<T>(folder, &paths);
    auto *ptr = result.get();
    root.getHlmsManager()->registerHlms(result.release());
    return ptr;
}
} // namespace
struct Renderer::Impl
{
    // Root is destroyed before its externally owned static plugin.
    std::unique_ptr<Ogre::Plugin> plugin;
    std::unique_ptr<Ogre::Root> root;
    Ogre::Window *output = nullptr;
    Ogre::SceneManager *scene = nullptr;
    Ogre::Camera *camera = nullptr;
    Ogre::SceneNode *cameraNode = nullptr;
    Ogre::HlmsPbs *pbs = nullptr;
    Ogre::HlmsUnlit *unlit = nullptr;
    Ogre::CompositorWorkspace *workspace = nullptr;
    bool nullBackend = false, offscreen = false, paused = false;
    RenderSettings settings;
    EnvironmentDesc environment;
    Ogre::TextureGpu *environmentTexture = nullptr;
    Ogre::MaterialPtr skyMaterial;
    RenderStats stats;
    std::string workspaceName;
    struct Mesh
    {
        Ogre::MeshPtr value;
        bool dynamic = false;
        std::vector<std::uint64_t> users{};
    };
    struct Material
    {
        Ogre::HlmsDatablock *value;
        MaterialModel model;
        bool overlay = false;
        std::array<TextureHandle, 6> textures{};
    };
    struct Object
    {
        Ogre::Item *value;
        Ogre::SceneNode *node;
        MeshHandle mesh;
        MaterialHandle material;
        Ogre::SkeletonAnimation *animation = nullptr;
        float speed = 1;
    };
    struct Light
    {
        Ogre::Light *value;
        Ogre::SceneNode *node;
    };
    struct Target
    {
        Ogre::TextureGpu *value;
        Ogre::CompositorWorkspace *workspace;
        unsigned width, height;
    };
    std::unordered_map<std::uint64_t, Ogre::TextureGpu *> textures;
    std::unordered_map<std::uint64_t, Mesh> meshes;
    std::unordered_map<std::uint64_t, Material> materials;
    std::unordered_map<std::uint64_t, Object> objects;
    std::unordered_map<std::uint64_t, Light> lights;
    std::unordered_map<std::uint64_t, Target> targets;
    ~Impl()
    {
        if (root && scene)
        {
            root->getCompositorManager2()->removeAllWorkspaces();
            workspace = nullptr;
            for (auto &[id, t] : targets)
                root->getRenderSystem()->getTextureGpuManager()->destroyTexture(t.value);
            targets.clear();
            if (skyMaterial)
            {
                const auto name = skyMaterial->getName();
                skyMaterial.reset();
                Ogre::MaterialManager::getSingleton().remove(name);
            }
            root->destroySceneManager(scene);
            scene = nullptr;
            objects.clear();
            lights.clear();
            meshes.clear();
            materials.clear();
        }
        root.reset();
    }
    void Workspaces()
    {
        auto *cm = root->getCompositorManager2();
        if (workspace)
        {
            cm->removeWorkspace(workspace);
            workspace = nullptr;
        }
        for (auto &[id, t] : targets)
        {
            cm->removeWorkspace(t.workspace);
            t.workspace = nullptr;
        }
        if (!workspaceName.empty())
        {
            cm->removeWorkspaceDefinition(workspaceName);
            cm->removeNodeDefinition("AutoGen " +
                                     Ogre::IdString(workspaceName + "/Node").getReleaseText());
        }
        workspaceName = Name("Workspace", ID());
        const auto nodeName = "AutoGen " + Ogre::IdString(workspaceName + "/Node").getReleaseText();
        Ogre::CompositorTargetDef *sceneTarget = nullptr;
        if (settings.hdr && !nullBackend)
        {
            auto *node = cm->addNodeDefinition(nodeName);
            node->addTextureSourceName("WindowRT", 0, Ogre::TextureDefinitionBase::TEXTURE_INPUT);
            node->setNumLocalTextureDefinitions(7);
            auto *texture = node->addTextureDefinition("HDRScene");
            texture->format = Ogre::PFG_RGBA16_FLOAT;
            auto *rtv = node->addRenderTextureView("HDRScene");
            Ogre::RenderTargetViewEntry attachment;
            attachment.textureName = "HDRScene";
            rtv->colourAttachments.push_back(attachment);
            auto *depth = node->addTextureDefinition("HDRDepth");
            depth->format = Ogre::PFG_D32_FLOAT;
            rtv->depthAttachment.textureName = "HDRDepth";
            const char *bloomTargets[]{"BloomHalf", "BloomQuarter", "BloomH", "BloomV",
                                       "BloomFinal"};
            for (unsigned i = 0; i < 5; ++i)
            {
                auto *bloom = node->addTextureDefinition(bloomTargets[i]);
                bloom->format = Ogre::PFG_RGBA16_FLOAT;
                bloom->widthFactor = bloom->heightFactor = (i == 0 || i == 4) ? .5f : .25f;
                auto *view = node->addRenderTextureView(bloomTargets[i]);
                Ogre::RenderTargetViewEntry entry;
                entry.textureName = bloomTargets[i];
                view->colourAttachments.push_back(entry);
            }
            node->setNumTargetPass(7);
            sceneTarget = node->addTargetPass("HDRScene");
            auto *pass =
                static_cast<Ogre::CompositorPassSceneDef *>(sceneTarget->addPass(Ogre::PASS_SCENE));
            pass->setAllClearColours(C(settings.background));
            pass->setAllLoadActions(Ogre::LoadAction::Clear);
            pass->mStoreActionDepth = Ogre::StoreAction::Store;
            pass->mLastRQ = 199;
            pass->mShadowNode = settings.shadows ? Ogre::IdString("PlagCShadow") : Ogre::IdString();
            const char *bloomMaterials[]{"PlagC/BloomExtract", "PlagC/BloomDownsample",
                                         "PlagC/BloomBlurH", "PlagC/BloomBlurV",
                                         "PlagC/BloomUpsample"};
            const char *bloomSources[]{"HDRScene", "BloomHalf", "BloomQuarter", "BloomH",
                                       "BloomHalf"};
            for (unsigned i = 0; i < 5; ++i)
            {
                auto *target = node->addTargetPass(bloomTargets[i]);
                auto *quad =
                    static_cast<Ogre::CompositorPassQuadDef *>(target->addPass(Ogre::PASS_QUAD));
                quad->mMaterialName = bloomMaterials[i];
                quad->addQuadTextureSource(0, bloomSources[i]);
                // Bind every active sampler explicitly, even for modes that use just sourceTexture.
                quad->addQuadTextureSource(1, i == 4 ? "BloomV" : bloomSources[i]);
                quad->setAllLoadActions(Ogre::LoadAction::DontCare);
            }
            Ogre::MaterialManager::getSingleton()
                .getByName("PlagC/BloomExtract")
                ->getTechnique(0)
                ->getPass(0)
                ->getFragmentProgramParameters()
                ->setNamedConstant("threshold", settings.bloomThreshold);
            auto *outputTarget = node->addTargetPass("WindowRT");
            auto *tone =
                static_cast<Ogre::CompositorPassQuadDef *>(outputTarget->addPass(Ogre::PASS_QUAD));
            tone->mMaterialName = "PlagC/ToneMap";
            tone->addQuadTextureSource(0, "HDRScene");
            tone->addQuadTextureSource(1, "BloomFinal");
            tone->addQuadTextureSource(2, "HDRDepth");
            tone->setAllLoadActions(Ogre::LoadAction::DontCare);
            auto material = Ogre::MaterialManager::getSingleton().getByName("PlagC/ToneMap");
            material->getTechnique(0)->getPass(0)->getFragmentProgramParameters()->setNamedConstant(
                "exposure", settings.exposure);
            material->getTechnique(0)->getPass(0)->getFragmentProgramParameters()->setNamedConstant(
                "bloomStrength", settings.bloom ? settings.bloomStrength : 0.f);
            auto fog = material->getTechnique(0)->getPass(0)->getFragmentProgramParameters();
            fog->setNamedConstant("cameraNear", camera->getNearClipDistance());
            fog->setNamedConstant("cameraFar", camera->getFarClipDistance());
            fog->setNamedConstant("fogDensity", settings.fogDensity);
            fog->setNamedConstant("fogColour", V(settings.fogColour));
            auto *overlay = static_cast<Ogre::CompositorPassSceneDef *>(
                outputTarget->addPass(Ogre::PASS_SCENE));
            overlay->mFirstRQ = 199;
            overlay->mLastRQ = 200;
            auto *definition = cm->addWorkspaceDefinition(workspaceName);
            definition->connectExternal(0, nodeName, 0);
        }
        else
        {
            cm->createBasicWorkspaceDef(workspaceName, C(settings.background),
                                        settings.shadows ? Ogre::IdString("PlagCShadow")
                                                         : Ogre::IdString());
            sceneTarget = cm->getNodeDefinitionNonConst(nodeName)->getTargetPass(0);
        }
        if (environmentTexture && environment.skybox && !nullBackend)
        {
            sceneTarget->getCompositorPassesNonConst().front()->mStoreActionDepth =
                Ogre::StoreAction::Store;
            auto *sky =
                static_cast<Ogre::CompositorPassQuadDef *>(sceneTarget->addPass(Ogre::PASS_QUAD));
            sky->mMaterialName = skyMaterial->getName();
            sky->mFrustumCorners = Ogre::CompositorPassQuadDef::CAMERA_DIRECTION;
        }
        // Ogre 3.0 NULL has incomplete window-depth and VAO draw behavior.
        // NULL tests resource/lifecycle APIs; only real GL backends execute draws.
        if (output && !nullBackend)
            workspace = cm->addWorkspace(scene, output->getTexture(), camera, workspaceName, true);
        for (auto &[id, t] : targets)
            t.workspace = cm->addWorkspace(scene, t.value, camera, workspaceName, !nullBackend);
    }
};
RendererBackendCapabilities Renderer::BackendCapabilities()
{
#ifdef PC_OGRE_NULL_ONLY
    return {false, false, true};
#elif defined(PC_OGRE_VULKAN)
    return {true, true, true};
#else
    return {true, false, true};
#endif
}
Renderer::Renderer(Window *window, const RendererConfig &config) : impl_(std::make_unique<Impl>())
{
    Thread::RequireMain();
    auto &p = *impl_;
    backend_ = ResolveRendererBackend(config.backend, BackendCapabilities(), config.nullBackend,
                                      config.offscreen);
    p.nullBackend = backend_ == RendererBackend::Null;
    p.offscreen = config.offscreen;
    Require(p.nullBackend || config.offscreen || (window && window->NativeHandle().value),
            "native renderer needs a native window");
    Require(std::filesystem::is_directory(config.mediaRoot + "/Hlms"),
            "Ogre media missing; initialize submodules and run from project root");
    p.root = std::make_unique<Ogre::Root>(nullptr, "", "", "Logs/Ogre.log", "PlagC");
    std::string systemName;
    switch (backend_)
    {
    case RendererBackend::Null:
        p.plugin = std::make_unique<Ogre::NULLPlugin>();
        systemName = "NULL Rendering Subsystem";
        break;
#ifndef PC_OGRE_NULL_ONLY
    case RendererBackend::OpenGL:
        p.plugin = std::make_unique<Ogre::GL3PlusPlugin>();
        systemName = "OpenGL 3+ Rendering Subsystem";
        break;
#endif
#ifdef PC_OGRE_VULKAN
    case RendererBackend::Vulkan:
        p.plugin = std::make_unique<Ogre::VulkanPlugin>();
        systemName = "Vulkan Rendering Subsystem";
        break;
#endif
    default:
        throw std::runtime_error("selected backend has no Ogre adapter");
    }
    static_cast<Ogre::DefaultWorkQueueBase *>(p.root->getWorkQueue())->setWorkerThreadCount(2);
    p.root->installPlugin(p.plugin.get(), nullptr);
    Ogre::RenderSystem *selected = nullptr;
    for (auto *system : p.root->getAvailableRenderers())
        if (system->getName() == systemName)
            selected = system;
    Require(selected != nullptr, "requested Ogre render system was not registered");
    p.root->setRenderSystem(selected);
    if (config.offscreen)
    {
        if (backend_ == RendererBackend::Vulkan)
            selected->setConfigOption("Interface", "null");
        else
        {
#if defined(__linux__) && !defined(PC_OGRE_NULL_ONLY)
        selected->setConfigOption("Interface", "Headless EGL / PBuffer");
#else
        throw std::invalid_argument("--offscreen requires a native Linux build with EGL");
#endif
        }
    }
    p.root->initialise(false);
    p.root->getRenderSystem()->setMetricsRecordingEnabled(true);
    // Borrowed display/window pair consumed synchronously by Ogre.
    struct X11Handles { void* display; unsigned long window; } x11{};
    Ogre::NameValuePairList options;
    options["vsync"] = "true";
    if (!p.nullBackend && !config.offscreen)
    {
        const auto native = window->NativeHandle();
        if (backend_ == RendererBackend::Vulkan && native.kind == NativeWindow::Kind::X11)
        {
            Require(native.display != nullptr, "Vulkan requires the GLFW X11 display");
            x11 = {native.display, static_cast<unsigned long>(native.value)};
            options["SDL2x11"] = std::to_string(reinterpret_cast<std::uintptr_t>(&x11));
        }
        else
            options["externalWindowHandle"] = std::to_string(native.value);
#ifdef __APPLE__
        options["macAPI"] = "cocoa";
        options["macAPICocoaUseNSView"] = "false";
#endif
    }
    p.output =
        p.root->createRenderWindow("PlagC", window ? unsigned(window->Width()) : 640,
                                   window ? unsigned(window->Height()) : 360, false, &options);
    p.pbs = RegisterHlms<Ogre::HlmsPbs>(*p.root, config.mediaRoot);
    p.unlit = RegisterHlms<Ogre::HlmsUnlit>(*p.root, config.mediaRoot);
    auto &resources = Ogre::ResourceGroupManager::getSingleton();
    for (const auto &dir : {config.assetsRoot + "/Textures", config.assetsRoot + "/Compositors",

                            config.mediaRoot + "/models"})
        resources.addResourceLocation(dir, "FileSystem", "General");
    if (!p.nullBackend)
        resources.addResourceLocation(config.assetsRoot + "/Shaders", "FileSystem", "General");
    resources.initialiseAllResourceGroups(true);
    p.scene = p.root->createSceneManager(Ogre::ST_GENERIC, 1, "PlagCScene");
    // PlagC reserves queues 1..199 for v2 Items. Ogre defaults 100..199 to
    // V1_FAST, which calls the unsupported SubItem::getRenderOperation.
    // Configure the complete public range before any objects (including HUD) render.
    for (unsigned queue = 1; queue < 200; ++queue)
        p.scene->getRenderQueue()->setRenderQueueMode(static_cast<Ogre::uint8>(queue),
                                                      Ogre::RenderQueue::FAST);
    p.scene->setShadowDirectionalLightExtrusionDistance(50);
    p.scene->setShadowFarDistance(50);
    p.scene->setAmbientLight(Ogre::ColourValue(.3f, .3f, .35f), Ogre::ColourValue(.1f, .1f, .12f),
                             Ogre::Vector3::UNIT_Y);
    p.camera = p.scene->createCamera("PlagCCamera");
    p.camera->detachFromParent();
    p.cameraNode = p.scene->getRootSceneNode()->createChildSceneNode();
    p.cameraNode->attachObject(p.camera);
    SetCamera({});
    SetSettings({});
}
Renderer::~Renderer()
{
    Thread::RequireMain();
}
MeshHandle Renderer::CreateMesh(const MeshData &data, bool dynamic)
{
    Thread::RequireMain();
    Require(!data.vertices.empty() && !data.indices.empty() && data.indices.size() % 3 == 0,
            "mesh must contain triangles");
    for (const auto &v : data.vertices)
        Require(Finite(v.position) && Finite(v.normal) && std::isfinite(v.u) && std::isfinite(v.v),
                "nonfinite vertex");
    for (auto i : data.indices)
        Require(i < data.vertices.size(), "mesh index out of range");
    auto &p = *impl_;
    auto id = ID();
    auto &manager = Ogre::MeshManager::getSingleton();
    auto mesh = manager.createManual(Name("Mesh", id), "General");
    auto *vm = p.root->getRenderSystem()->getVaoManager();
    Ogre::VertexBufferPacked *vb = nullptr;
    Ogre::IndexBufferPacked *ib = nullptr;
    Ogre::VertexArrayObject *vao = nullptr;
    try
    {
        Ogre::VertexElement2Vec elements{{Ogre::VET_FLOAT3, Ogre::VES_POSITION},
                                         {Ogre::VET_FLOAT3, Ogre::VES_NORMAL},
                                         {Ogre::VET_FLOAT2, Ogre::VES_TEXTURE_COORDINATES},
                                         {Ogre::VET_FLOAT4, Ogre::VES_TANGENT}};
        vb = vm->createVertexBuffer(elements, data.vertices.size(),
                                    dynamic ? Ogre::BT_DEFAULT : Ogre::BT_IMMUTABLE,
                                    const_cast<Vertex *>(data.vertices.data()), false);
        ib = vm->createIndexBuffer(Ogre::IndexBufferPacked::IT_32BIT, data.indices.size(),
                                   Ogre::BT_IMMUTABLE,
                                   const_cast<std::uint32_t *>(data.indices.data()), false);
        vao = vm->createVertexArrayObject({vb}, ib, Ogre::OT_TRIANGLE_LIST);
        auto *sub = mesh->createSubMesh();
        sub->mVao[Ogre::VpNormal].push_back(vao);
        sub->mVao[Ogre::VpShadow].push_back(vao);
        vao = nullptr;
        vb = nullptr;
        ib = nullptr;
        Ogre::Vector3 lo = V(data.vertices[0].position), hi = lo;
        for (const auto &v : data.vertices)
        {
            lo.makeFloor(V(v.position));
            hi.makeCeil(V(v.position));
        }
        mesh->_setBounds(Ogre::Aabb((lo + hi) * .5f, (hi - lo) * .5f), false);
        mesh->_setBoundingSphereRadius(std::max(lo.length(), hi.length()));
        p.meshes.emplace(id, Impl::Mesh{mesh, dynamic});
    }
    catch (...)
    {
        if (vao)
            vm->destroyVertexArrayObject(vao);
        if (ib)
            vm->destroyIndexBuffer(ib);
        if (vb)
            vm->destroyVertexBuffer(vb);
        manager.remove(mesh->getHandle());
        throw;
    }
    return {id};
}
void Renderer::UpdateMesh(MeshHandle handle, std::span<const Vertex> vertices)
{
    Thread::RequireMain();
    auto &mesh = Get(impl_->meshes, handle);
    auto *buffer = mesh.value->getSubMesh(0)->mVao[Ogre::VpNormal][0]->getVertexBuffers()[0];
    Require(mesh.dynamic && vertices.size() == buffer->getNumElements(),
            "mesh update requires matching dynamic vertex count");
    for (auto &v : vertices)
        Require(Finite(v.position) && Finite(v.normal) && std::isfinite(v.u) && std::isfinite(v.v),
                "nonfinite mesh update");
    buffer->upload(vertices.data(), 0, vertices.size());
    Ogre::Vector3 lo = V(vertices[0].position), hi = lo;
    for (auto &v : vertices)
    {
        lo.makeFloor(V(v.position));
        hi.makeCeil(V(v.position));
    }
    mesh.value->_setBounds(Ogre::Aabb((lo + hi) * .5f, (hi - lo) * .5f), false);
    mesh.value->_setBoundingSphereRadius(std::max(lo.length(), hi.length()));
    for (auto id : mesh.users)
        impl_->objects.at(id).value->setLocalAabb(mesh.value->getAabb());
}
TextureHandle Renderer::CreateTexture(unsigned width, unsigned height,
                                      std::span<const std::uint8_t> rgba, bool srgb, bool mipmaps)
{
    Thread::RequireMain();
    Require(width && height && width <= 8192 && height <= 8192 &&
                rgba.size() == std::uint64_t(width) * height * 4,
            "invalid RGBA texture");
    auto image = std::make_unique<Ogre::Image2>();
    image->createEmptyImage(width, height, 1, Ogre::TextureTypes::Type2D,
                            srgb ? Ogre::PFG_RGBA8_UNORM_SRGB : Ogre::PFG_RGBA8_UNORM);
    auto box = image->getData(0);
    for (unsigned row = 0; row < height; ++row)
        std::memcpy(static_cast<unsigned char *>(box.data) + row * box.bytesPerRow,
                    rgba.data() + std::size_t(row) * width * 4, std::size_t(width) * 4);
    if (mipmaps)
        image->generateMipmaps(true);
    auto id = ID();
    auto *tm = impl_->root->getRenderSystem()->getTextureGpuManager();
    auto *texture =
        tm->createTexture(Name("AssetTexture", id), Ogre::GpuPageOutStrategy::Discard,
                          Ogre::TextureFlags::AutomaticBatching |
                              (srgb ? Ogre::TextureFlags::PrefersLoadingFromFileAsSRGB : 0u),
                          Ogre::TextureTypes::Type2D);
    try
    {
        texture->scheduleTransitionTo(Ogre::GpuResidency::Resident, image.release(), true);
        tm->waitForStreamingCompletion();
        impl_->textures.emplace(id, texture);
    }
    catch (...)
    {
        tm->destroyTexture(texture);
        throw;
    }
    return {id};
}
TextureHandle Renderer::LoadTexture(const std::string &resource, bool srgb, bool mipmaps)
{
    Thread::RequireMain();
    const auto path = std::filesystem::path(resource);
    Require(!path.empty() && !path.is_absolute(), "texture resource must be relative");
    for (const auto &part : path)
        Require(part != "..", "texture resource escapes asset root");
    // Use Ogre's registered image codecs on every platform; upload remains engine-owned.
    Ogre::Image2 image;
    image.load(resource, "General");
    const unsigned width = image.getWidth(), height = image.getHeight();
    Require(image.getTextureType() == Ogre::TextureTypes::Type2D && image.getDepth() == 1 &&
                width && height && width <= 8192 && height <= 8192,
            "unsupported texture dimensions/type");
    std::vector<std::uint8_t> rgba(std::size_t(width) * height * 4);
    Ogre::TextureBox destination(width, height, 1, 1, 4, width * 4,
                                 std::size_t(width) * height * 4);
    destination.data = rgba.data();
    Ogre::PixelFormatGpuUtils::bulkPixelConversion(image.getData(0), image.getPixelFormat(),
                                                   destination, Ogre::PFG_RGBA8_UNORM);
    // A one-channel image is luminance in common PNG/JPEG files. Keep it neutral
    // when used as colour, while preserving the red channel used by data maps.
    if (image.getPixelFormat() == Ogre::PFG_R8_UNORM)
        for (std::size_t i = 0; i < rgba.size(); i += 4)
        {
            rgba[i + 1] = rgba[i + 2] = rgba[i];
            rgba[i + 3] = 255;
        }
    return CreateTexture(width, height, rgba, srgb, mipmaps);
}
void Renderer::DestroyTexture(TextureHandle handle)
{
    Thread::RequireMain();
    auto *texture = Get(impl_->textures, handle);
    for (auto &[id, m] : impl_->materials)
        for (auto h : m.textures)
            Require(h.value != handle.value, "texture still referenced by material");
    impl_->root->getRenderSystem()->getTextureGpuManager()->destroyTexture(texture);
    impl_->textures.erase(handle.value);
}

MeshHandle Renderer::LoadModel(const std::string &resource, bool legacy)
{
    Thread::RequireMain();
    auto id = ID();
    Ogre::MeshPtr mesh;
    if (legacy)
    {
        auto old = Ogre::v1::MeshManager::getSingleton().load(resource, "General");
        mesh = Ogre::MeshManager::getSingleton().createByImportingV1(Name("Model", id), "General",
                                                                     old.get(), true, true, true);
    }
    else
        mesh = Ogre::MeshManager::getSingleton().load(resource, "General");
    impl_->meshes.emplace(id, Impl::Mesh{mesh});
    return {id};
}
void Renderer::DestroyMesh(MeshHandle h)
{
    auto &p = *impl_;
    auto &m = Get(p.meshes, h);
    for (auto &[id, o] : p.objects)
        if (o.mesh.value == h.value)
            throw std::logic_error("mesh still referenced by object");
    Ogre::MeshManager::getSingleton().remove(m.value->getHandle());
    p.meshes.erase(h.value);
}
MaterialHandle Renderer::CreateMaterial(const MaterialDesc &d)
{
    Thread::RequireMain();
    auto &p = *impl_;
    auto id = ID();
    auto name = Name("Material", id);
    Ogre::Hlms *hlms = d.model == MaterialModel::Pbr ? static_cast<Ogre::Hlms *>(p.pbs) : p.unlit;
    Ogre::HlmsMacroblock macro;
    macro.mDepthCheck = !d.overlay;
    macro.mDepthWrite = !d.overlay;
    macro.mPolygonMode = p.settings.wireframe && !d.overlay ? Ogre::PM_WIREFRAME : Ogre::PM_SOLID;
    auto *db = hlms->createDatablock(name, name, macro, Ogre::HlmsBlendblock(), {});
    p.materials.emplace(id, Impl::Material{db, d.model, d.overlay});
    try
    {
        UpdateMaterial({id}, d);
    }
    catch (...)
    {
        p.materials.erase(id);
        hlms->destroyDatablock(name);
        throw;
    }
    return {id};
}
void Renderer::UpdateMaterial(MaterialHandle h, const MaterialDesc &d)
{
    auto &m = Get(impl_->materials, h);
    Require(m.model == d.model && m.overlay == d.overlay, "material model/overlay is immutable");
    Require(Finite(d.colour) && std::isfinite(d.roughness) && std::isfinite(d.metallic) &&
                d.roughness >= 0 && d.roughness <= 1 && d.metallic >= 0 && d.metallic <= 1,
            "invalid material values");
    if (d.baseColourTexture)
        Get(impl_->textures, d.baseColourTexture);
    std::array<TextureHandle, 6> bindings{d.baseColourTexture, d.normalTexture,
                                          d.metallicTexture,   d.roughnessTexture,
                                          d.occlusionTexture,  d.emissiveTexture};
    for (auto h : bindings)
        if (h)
            Get(impl_->textures, h);
    Require(Finite(d.emissive) && std::isfinite(d.normalScale) && d.normalScale >= 0,
            "invalid material factors");
    m.textures = bindings;
    if (d.model == MaterialModel::Pbr)
    {
        auto *b = static_cast<Ogre::HlmsPbsDatablock *>(m.value);
        Require(std::isfinite(d.alphaCutoff) && d.alphaCutoff >= 0 && d.alphaCutoff <= 1,
                "invalid alpha cutoff");
        b->setAlphaTest(d.alphaCutoff > 0 ? Ogre::CMPF_GREATER_EQUAL : Ogre::CMPF_ALWAYS_PASS);
        b->setAlphaTestThreshold(d.alphaCutoff);
        b->setTwoSidedLighting(d.doubleSided);
        b->setWorkflow(Ogre::HlmsPbsDatablock::MetallicWorkflow);
        b->setDiffuse(V(d.colour));
        b->setRoughness(std::max(.02f, d.roughness));
        b->setMetalness(d.metallic);
        b->setEmissive(V(d.emissive));
        b->setNormalMapWeight(d.normalScale);
        Ogre::HlmsSamplerblock sampler;
        sampler.setFiltering(Ogre::TFO_ANISOTROPIC);
        sampler.mMaxAnisotropy = 8;
        sampler.setAddressingMode(Ogre::TAM_WRAP);
        const Ogre::PbsTextureTypes slots[]{Ogre::PBSM_NORMAL, Ogre::PBSM_METALLIC,
                                            Ogre::PBSM_ROUGHNESS, Ogre::PBSM_DETAIL_WEIGHT,
                                            Ogre::PBSM_EMISSIVE};
        for (unsigned i = 0; i < 5; ++i)
            b->setTexture(slots[i],
                          bindings[i + 1] ? Get(impl_->textures, bindings[i + 1]) : nullptr,
                          &sampler);
        if (d.baseColourTexture)
            b->setTexture(Ogre::PBSM_DIFFUSE, Get(impl_->textures, d.baseColourTexture), &sampler);
        else
            b->setTexture(Ogre::PBSM_DIFFUSE, d.colourTexture);
        b->setTexture(Ogre::PBSM_REFLECTION,
                      impl_->environment.reflections ? impl_->environmentTexture : nullptr);
    }
    else
    {
        auto *b = static_cast<Ogre::HlmsUnlitDatablock *>(m.value);
        b->setUseColour(true);
        b->setColour(C(d.colour));
        if (d.baseColourTexture)
            b->setTexture(0, Get(impl_->textures, d.baseColourTexture));
        else
            b->setTexture(0, d.colourTexture);
    }
}
void Renderer::SetEnvironment(const EnvironmentDesc &desc)
{
    Thread::RequireMain();
    auto &p = *impl_;
    if (desc.cubemap == p.environment.cubemap && desc.skybox == p.environment.skybox &&
        desc.reflections == p.environment.reflections)
        return;
    auto *tm = p.root->getRenderSystem()->getTextureGpuManager();
    Ogre::TextureGpu *next = p.environmentTexture;
    const bool replace = desc.cubemap != p.environment.cubemap;
    // Validate and load before touching the live environment. Missing/invalid files
    // fail synchronously rather than silently becoming a streaming placeholder.
    if (replace)
    {
        next = nullptr;
        if (!desc.cubemap.empty())
        {
            auto image = std::make_unique<Ogre::Image2>();
            image->load(desc.cubemap, "General");
            Require(image->getTextureType() == Ogre::TextureTypes::TypeCube,
                    "environment must be a DDS cubemap, not a 2D texture");
            Require(image->getNumMipmaps() > 1,
                    "environment cubemap needs mipmaps for rough reflections");
            next = tm->createTexture(desc.cubemap, Name("Environment", ID()),
                                     Ogre::GpuPageOutStrategy::Discard,
                                     Ogre::TextureFlags::PrefersLoadingFromFileAsSRGB,
                                     Ogre::TextureTypes::TypeCube, "General");
            try
            {
                next->scheduleTransitionTo(Ogre::GpuResidency::Resident, image.release(), true);
                tm->waitForStreamingCompletion();
            }
            catch (...)
            {
                tm->destroyTexture(next);
                throw;
            }
        }
    }
    try
    {
        if (next && !p.nullBackend && !p.skyMaterial)
        {
            auto base = Ogre::MaterialManager::getSingleton().getByName("PlagC/Sky");
            Require(bool(base), "missing PlagC sky material");
            p.skyMaterial = base->clone(Name("Sky", ID()));
        }
    }
    catch (...)
    {
        if (replace && next)
            tm->destroyTexture(next);
        throw;
    }
    auto *previous = p.environmentTexture;
    p.environmentTexture = next;
    p.environment = desc;
    if (p.skyMaterial)
    {
        auto *unit = p.skyMaterial->getTechnique(0)->getPass(0)->getTextureUnitState(0);
        if (next)
            unit->setTexture(next);
        else
            unit->setTextureName("");
    }
    for (auto &[id, material] : p.materials)
        if (material.model == MaterialModel::Pbr)
            static_cast<Ogre::HlmsPbsDatablock *>(material.value)
                ->setTexture(Ogre::PBSM_REFLECTION, desc.reflections ? next : nullptr);
    p.Workspaces();
    if (replace && previous)
        tm->destroyTexture(previous);
}
EnvironmentDesc Renderer::Environment() const
{
    Thread::RequireMain();
    return impl_->environment;
}
void Renderer::DestroyMaterial(MaterialHandle h)
{
    auto &p = *impl_;
    auto &m = Get(p.materials, h);
    for (auto &[id, o] : p.objects)
        if (o.material.value == h.value)
            throw std::logic_error("material still referenced by object");
    m.value->getCreator()->destroyDatablock(m.value->getName());
    p.materials.erase(h.value);
}
ObjectHandle Renderer::CreateObject(MeshHandle mesh, MaterialHandle material, const Transform &t)
{
    auto &p = *impl_;
    auto &m = Get(p.meshes, mesh);
    auto &d = Get(p.materials, material);
    auto id = ID();
    auto *item = p.scene->createItem(m.value, Ogre::SCENE_DYNAMIC);
    Ogre::SceneNode *node = nullptr;
    try
    {
        item->setDatablock(d.value);
        node = p.scene->getRootSceneNode()->createChildSceneNode();
        node->attachObject(item);
        p.objects.emplace(id, Impl::Object{item, node, mesh, material});
        SetTransform({id}, t);
        Get(p.meshes, mesh).users.push_back(id);
    }
    catch (...)
    {
        p.objects.erase(id);
        p.scene->destroyItem(item);
        if (node)
            p.scene->destroySceneNode(node);
        throw;
    }
    return {id};
}
void Renderer::SetTransform(ObjectHandle h, const Transform &t)
{
    auto &o = Get(impl_->objects, h);
    Require(Finite(t.position) && Finite(t.rotationDegrees) && Finite(t.scale) && t.scale.x > 0 &&
                t.scale.y > 0 && t.scale.z > 0,
            "invalid transform");
    o.node->setPosition(V(t.position));
    o.node->setScale(V(t.scale));
    o.node->setOrientation(
        Ogre::Quaternion(Ogre::Degree(t.rotationDegrees.y), Ogre::Vector3::UNIT_Y) *
        Ogre::Quaternion(Ogre::Degree(t.rotationDegrees.x), Ogre::Vector3::UNIT_X) *
        Ogre::Quaternion(Ogre::Degree(t.rotationDegrees.z), Ogre::Vector3::UNIT_Z));
}
void Renderer::SetVisible(ObjectHandle h, bool v)
{
    Get(impl_->objects, h).value->setVisible(v);
}
void Renderer::SetCastShadows(ObjectHandle h, bool v)
{
    Get(impl_->objects, h).value->setCastShadows(v);
}
void Renderer::SetRenderQueue(ObjectHandle h, std::uint8_t q)
{
    Require(q > 0 && q < 200, "queue must be a v2 scene queue 1..199");
    Get(impl_->objects, h).value->setRenderQueueGroup(q);
}
void Renderer::DestroyObject(ObjectHandle h)
{
    auto &p = *impl_;
    auto o = Get(p.objects, h);
    p.scene->destroyItem(o.value);
    p.scene->destroySceneNode(o.node);
    std::erase(Get(p.meshes, o.mesh).users, h.value);
    p.objects.erase(h.value);
}
LightHandle Renderer::CreateLight(const LightDesc &d)
{
    Thread::RequireMain();
    auto &p = *impl_;
    auto id = ID();
    auto *light = p.scene->createLight();
    auto *node = p.scene->getRootSceneNode()->createChildSceneNode();
    node->attachObject(light);
    p.lights.emplace(id, Impl::Light{light, node});
    try
    {
        UpdateLight({id}, d);
    }
    catch (...)
    {
        DestroyLight({id});
        throw;
    }
    return {id};
}
void Renderer::UpdateLight(LightHandle h, const LightDesc &d)
{
    auto &l = Get(impl_->lights, h);
    Require(Finite(d.position) && Finite(d.direction) && V(d.direction).squaredLength() > 0 &&
                Finite(d.colour) && std::isfinite(d.power) && d.power >= 0 &&
                std::isfinite(d.radius) && d.radius > 0 && d.innerAngle > 0 &&
                d.outerAngle > d.innerAngle && d.outerAngle < 180,
            "invalid light");
    l.node->setPosition(V(d.position));
    l.value->setType(d.type == LightType::Directional ? Ogre::Light::LT_DIRECTIONAL
                     : d.type == LightType::Point     ? Ogre::Light::LT_POINT
                                                      : Ogre::Light::LT_SPOTLIGHT);
    l.value->setDirection(V(d.direction).normalisedCopy());
    l.value->setDiffuseColour(C(d.colour));
    l.value->setSpecularColour(C(d.colour));
    l.value->setPowerScale(d.power);
    l.value->setAttenuationBasedOnRadius(d.radius, .01f);
    l.value->setSpotlightRange(Ogre::Degree(d.innerAngle), Ogre::Degree(d.outerAngle));
}
void Renderer::DestroyLight(LightHandle h)
{
    auto &p = *impl_;
    auto l = Get(p.lights, h);
    p.scene->destroyLight(l.value);
    p.scene->destroySceneNode(l.node);
    p.lights.erase(h.value);
}
void Renderer::SetCamera(const CameraDesc &d)
{
    Thread::RequireMain();
    Require(std::isfinite(d.aspect) && d.aspect >= 0 && Finite(d.position) && Finite(d.target) &&
                (V(d.position) - V(d.target)).squaredLength() > 0 && d.fov > 1 && d.fov < 179 &&
                d.nearClip > 0 && d.farClip > d.nearClip,
            "invalid camera");
    auto &p = *impl_;
    p.cameraNode->setPosition(V(d.position));
    p.cameraNode->setFixedYawAxis(true, Ogre::Vector3::UNIT_Y);
    p.cameraNode->lookAt(V(d.target), Ogre::Node::TS_PARENT);
    p.camera->setFOVy(Ogre::Degree(d.fov));
    p.camera->setNearClipDistance(d.nearClip);
    p.camera->setFarClipDistance(d.farClip);
    if (!p.nullBackend && p.settings.hdr)
    {
        auto parameters = Ogre::MaterialManager::getSingleton()
                              .getByName("PlagC/ToneMap")
                              ->getTechnique(0)
                              ->getPass(0)
                              ->getFragmentProgramParameters();
        parameters->setNamedConstant("cameraNear", d.nearClip);
        parameters->setNamedConstant("cameraFar", d.farClip);
    }
    p.camera->setAutoAspectRatio(d.aspect == 0);
    if (d.aspect > 0)
        p.camera->setAspectRatio(d.aspect);
}
RenderSettings Renderer::Settings() const
{
    Thread::RequireMain();
    return impl_->settings;
}
void Renderer::SetVSync(bool enabled)
{
    Thread::RequireMain();
    if (impl_->output)
        impl_->output->setVSync(enabled, 1);
    impl_->settings.vsync = enabled;
}
void Renderer::SetSettings(const RenderSettings &d)
{
    Thread::RequireMain();
    Require(Finite(d.background) && std::isfinite(d.exposure) && d.exposure > 0,
            "invalid render settings");
    Require(std::isfinite(d.bloomStrength) && d.bloomStrength >= 0 &&
                std::isfinite(d.bloomThreshold) && d.bloomThreshold > 0 &&
                std::isfinite(d.fogDensity) && d.fogDensity >= 0 && Finite(d.fogColour),
            "invalid bloom/fog settings");
    auto &p = *impl_;
    p.settings = d;
    for (auto &[id, m] : p.materials)
    {
        auto block = *m.value->getMacroblock();
        block.mPolygonMode = d.wireframe && !m.overlay ? Ogre::PM_WIREFRAME : Ogre::PM_SOLID;
        m.value->setMacroblock(block);
    }
    if (p.output)
        p.output->setVSync(d.vsync, 1);
    p.Workspaces();
}
std::vector<std::string> Renderer::Animations(ObjectHandle h) const
{
    auto &o = Get(impl_->objects, h);
    std::vector<std::string> result;
    if (auto *skeleton = o.value->getSkeletonInstance())
        for (auto &a : skeleton->getAnimations())
            result.push_back(a.getName().getFriendlyText());
    return result;
}
void Renderer::StopAnimation(ObjectHandle h)
{
    auto &o = Get(impl_->objects, h);
    if (o.animation)
        o.animation->setEnabled(false);
    o.animation = nullptr;
}
void Renderer::PlayAnimation(ObjectHandle h, const std::string &name, bool loop, float speed)
{
    auto &o = Get(impl_->objects, h);
    Require(std::isfinite(speed), "nonfinite animation speed");
    auto *skeleton = o.value->getSkeletonInstance();
    Require(skeleton != nullptr, "object has no skeleton");
    for (auto &a : skeleton->getAnimationsNonConst())
        if (a.getName() == Ogre::IdString(name))
        {
            StopAnimation(h);
            o.animation = &a;
            o.speed = speed;
            a.setLoop(loop);
            a.setTime(0);
            a.setEnabled(true);
            return;
        }
    throw std::invalid_argument("animation not found");
}
TargetHandle Renderer::CreateTarget(unsigned w, unsigned h)
{
    Thread::RequireMain();
    Require(w && h && w <= 8192 && h <= 8192, "invalid render target size");
    auto &p = *impl_;
    auto id = ID();
    auto *tm = p.root->getRenderSystem()->getTextureGpuManager();
    auto *t = tm->createTexture(Name("Target", id), Ogre::GpuPageOutStrategy::Discard,
                                Ogre::TextureFlags::RenderToTexture, Ogre::TextureTypes::Type2D);
    try
    {
        t->_setDepthBufferDefaults(1, true, Ogre::PFG_D32_FLOAT);
        t->setResolution(w, h);
        t->setPixelFormat(Ogre::PFG_RGBA8_UNORM_SRGB);
        t->scheduleTransitionTo(Ogre::GpuResidency::Resident);
        auto *ws = p.root->getCompositorManager2()->addWorkspace(p.scene, t, p.camera,
                                                                 p.workspaceName, !p.nullBackend);
        p.targets.emplace(id, Impl::Target{t, ws, w, h});
    }
    catch (...)
    {
        tm->destroyTexture(t);
        throw;
    }
    return {id};
}
void Renderer::ResizeTarget(TargetHandle h, unsigned w, unsigned height)
{
    auto &p = *impl_;
    Get(p.targets, h);
    auto replacement = CreateTarget(w, height);
    auto value = p.targets.at(replacement.value);
    DestroyTarget(h);
    p.targets.emplace(h.value, value);
    p.targets.erase(replacement.value);
}
void Renderer::DestroyTarget(TargetHandle h)
{
    auto &p = *impl_;
    auto t = Get(p.targets, h);
    p.root->getCompositorManager2()->removeWorkspace(t.workspace);
    p.root->getRenderSystem()->getTextureGpuManager()->destroyTexture(t.value);
    p.targets.erase(h.value);
}
void Renderer::CaptureTarget(TargetHandle h, const std::string &path)
{
    auto &t = Get(impl_->targets, h);
    if (impl_->nullBackend)
        throw std::logic_error("NULL renderer cannot capture pixels");
    Ogre::Image2 image;
    image.convertFromTexture(t.value, 0, 0);
    std::ofstream file(path, std::ios::binary);
    file << "P6\n" << image.getWidth() << " " << image.getHeight() << "\n255\n";
    for (unsigned y = 0; y < image.getHeight(); ++y)
        for (unsigned x = 0; x < image.getWidth(); ++x)
        {
            auto c = image.getColourAt(x, y, 0);
            for (float v : {c.r, c.g, c.b})
                file.put(static_cast<char>(std::clamp(v, 0.f, 1.f) * 255.f));
        }
    if (!file)
        throw std::runtime_error("capture write failed");
}
void Renderer::Resize(unsigned w, unsigned h)
{
    Thread::RequireMain();
    auto &p = *impl_;
    p.paused = !w || !h;
    if (p.paused)
        return;
    if (p.output)
    {
        if (p.nullBackend || p.offscreen)
            p.output->requestResolution(w, h);
        else
            p.output->windowMovedOrResized();
    }
}
void Renderer::Render(double dt)
{
    Thread::RequireMain();
    Require(std::isfinite(dt) && dt >= 0, "invalid frame delta");
    auto &p = *impl_;
    if (p.paused)
        return;
    auto start = std::chrono::steady_clock::now();
    for (auto &[id, o] : p.objects)
        if (o.animation)
            o.animation->addTime(float(std::min(dt, .25)) * o.speed);
    Detail::SetOgreUIOutput(this, p.output ? p.output->getTexture() : nullptr);
    if (!p.root->renderOneFrame())
        throw std::runtime_error("Ogre stopped rendering");
    Detail::CheckOgreUIOutput(this);
    const auto &metrics = p.root->getRenderSystem()->getMetrics();
    p.stats.drawCalls = metrics.mDrawCount;
    p.stats.triangles = metrics.mFaceCount;
    p.stats.instances = metrics.mInstanceCount;
    ++p.stats.frames;
    p.stats.cpuMilliseconds =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}
RenderStats Renderer::Statistics() const
{
    Thread::RequireMain();
    auto s = impl_->stats;
    s.objects = impl_->objects.size();
    s.meshes = impl_->meshes.size();
    s.textures = impl_->textures.size();
    s.materials = impl_->materials.size();
    s.lights = impl_->lights.size();
    return s;
}
} // namespace PlagC
