#include "pgcpch.h"
#include "UiSubmissionInternal.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4244 4267 4324 4996)
#endif
#include <CommandBuffer/OgreCbDrawCall.h>
#include <Compositor/OgreCompositorManager2.h>
#include <Compositor/OgreCompositorWorkspace.h>
#include <Compositor/OgreCompositorWorkspaceListener.h>
#include <OgreGpuProgramParams.h>
#include <OgreHardwareBufferManager.h>
#include <OgreHardwareIndexBuffer.h>
#include <OgreHardwareVertexBuffer.h>
#include <OgreHighLevelGpuProgramManager.h>
#include <OgreHlmsManager.h>
#include <OgreIdString.h>
#include <OgreMaterialManager.h>
#include <OgreMatrix4.h>
#include <OgrePass.h>
#include <OgrePsoCacheHelper.h>
#include <OgreRenderPassDescriptor.h>
#include <OgreRenderSystem.h>
#include <OgreResourceGroupManager.h>
#include <OgreRoot.h>
#include <OgreTechnique.h>
#include <OgreTextureGpu.h>
#include <OgreTextureGpuManager.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <unordered_set>

namespace PlagC::Detail {
namespace {

constexpr const char* UiMaterialName = "PlagC/EditorUI/Material";
constexpr const char* UiVertexProgramGL = "PlagC/EditorUI/VP/GL150";
constexpr const char* UiPixelProgramGL = "PlagC/EditorUI/FP/GL150";
constexpr const char* UiVertexProgramVK = "PlagC/EditorUI/VP/Vulkan";
constexpr const char* UiPixelProgramVK = "PlagC/EditorUI/FP/Vulkan";

const char* VertexShaderGL = R"GLSL(#version 150
uniform mat4 ProjectionMatrix;
in vec2 vertex;
in vec2 uv0;
in vec4 colour;
out vec2 Texcoord;
out vec4 col;
void main()
{
    gl_Position = ProjectionMatrix * vec4(vertex.xy, 0.0, 1.0);
    Texcoord = uv0;
    col = colour;
}
)GLSL";

const char* PixelShaderGL = R"GLSL(#version 150
in vec2 Texcoord;
in vec4 col;
uniform sampler2D sampler0;
out vec4 out_col;
void main()
{
    out_col = col * texture(sampler0, Texcoord);
}
)GLSL";

const char* VertexShaderVK = R"GLSL(
vulkan( layout( ogre_P0 ) uniform Params { )
    uniform mat4 ProjectionMatrix;
vulkan( }; )
vulkan_layout( OGRE_POSITION ) in vec2 vertex;
vulkan_layout( OGRE_TEXCOORD0 ) in vec2 Texcoord;
vulkan_layout( OGRE_DIFFUSE ) in vec4 colour;
vulkan_layout( location = 1 )
out block
{
    vec2 uv0;
    vec4 col;
} outVs;
void main()
{
    gl_Position = ProjectionMatrix * vec4(vertex.xy, 0.0, 1.0);
    outVs.uv0 = Texcoord;
    outVs.col = colour;
}
)GLSL";

const char* PixelShaderVK = R"GLSL(
vulkan_layout( ogre_t0 ) uniform texture2D sampler0;
vulkan( layout( ogre_s0 ) uniform sampler texSampler );
vulkan_layout( location = 0 )
out vec4 out_col;
vulkan_layout( location = 1 )
in block
{
    vec2 uv0;
    vec4 col;
} inPs;
void main()
{
    out_col = inPs.col * texture(vkSampler2D(sampler0, texSampler), inPs.uv0);
}
)GLSL";

std::size_t GrowCapacity(std::size_t current, std::size_t required)
{
    std::size_t capacity = std::max<std::size_t>(current, 256u);
    while (capacity < required)
    {
        if (capacity > std::numeric_limits<std::size_t>::max() / 2u)
            return required;
        capacity *= 2u;
    }
    return capacity;
}

class UiBuffers final
{
public:
    UiBuffers()
    {
        auto* manager = Ogre::v1::HardwareBufferManager::getSingletonPtr();
        if (!manager)
            throw std::runtime_error("Ogre hardware buffer manager is unavailable for editor UI");

        operation_.vertexData = OGRE_NEW Ogre::v1::VertexData(manager);
        operation_.indexData = OGRE_NEW Ogre::v1::IndexData();
        operation_.operationType = Ogre::OperationType::OT_TRIANGLE_LIST;
        operation_.useIndexes = true;
        operation_.useGlobalInstancingVertexBufferIsAvailable = false;

        auto* declaration = operation_.vertexData->vertexDeclaration;
        std::size_t offset = 0;
        declaration->addElement(0, offset, Ogre::VET_FLOAT2, Ogre::VES_POSITION);
        offset += Ogre::v1::VertexElement::getTypeSize(Ogre::VET_FLOAT2);
        declaration->addElement(0, offset, Ogre::VET_FLOAT2, Ogre::VES_TEXTURE_COORDINATES, 0);
        offset += Ogre::v1::VertexElement::getTypeSize(Ogre::VET_FLOAT2);
        declaration->addElement(0, offset, Ogre::VET_COLOUR, Ogre::VES_DIFFUSE);

        if (offset + Ogre::v1::VertexElement::getTypeSize(Ogre::VET_COLOUR) != sizeof(UiVertex))
            throw std::logic_error("editor UI vertex layout does not match Ogre declaration");
    }

    ~UiBuffers()
    {
        OGRE_DELETE operation_.vertexData;
        OGRE_DELETE operation_.indexData;
    }

    UiBuffers(const UiBuffers&) = delete;
    UiBuffers& operator=(const UiBuffers&) = delete;

    void Upload(const UiDrawData& frame)
    {
        if (frame.vertices.empty() || frame.indices.empty())
            return;

        auto* manager = Ogre::v1::HardwareBufferManager::getSingletonPtr();
        if (!manager)
            throw std::runtime_error("Ogre hardware buffer manager disappeared during editor UI upload");

        auto* binding = operation_.vertexData->vertexBufferBinding;
        if (vertexCapacity_ < frame.vertices.size())
        {
            vertexCapacity_ = GrowCapacity(vertexCapacity_, frame.vertices.size());
            binding->setBinding(
                0,
                manager->createVertexBuffer(sizeof(UiVertex), vertexCapacity_,
                    Ogre::v1::HardwareBuffer::HBU_DYNAMIC_WRITE_ONLY_DISCARDABLE));
        }

        if (indexCapacity_ < frame.indices.size())
        {
            indexCapacity_ = GrowCapacity(indexCapacity_, frame.indices.size());
            operation_.indexData->indexBuffer = manager->createIndexBuffer(
                Ogre::v1::HardwareIndexBuffer::IT_32BIT, indexCapacity_,
                Ogre::v1::HardwareBuffer::HBU_DYNAMIC_WRITE_ONLY_DISCARDABLE);
        }

        auto vertexBuffer = binding->getBuffer(0);
        void* vertices = vertexBuffer->lock(Ogre::v1::HardwareBuffer::HBL_DISCARD);
        std::memcpy(vertices, frame.vertices.data(), frame.vertices.size() * sizeof(UiVertex));
        vertexBuffer->unlock();

        void* indices = operation_.indexData->indexBuffer->lock(Ogre::v1::HardwareBuffer::HBL_DISCARD);
        std::memcpy(indices, frame.indices.data(), frame.indices.size() * sizeof(std::uint32_t));
        operation_.indexData->indexBuffer->unlock();

        operation_.vertexData->vertexStart = 0;
        operation_.vertexData->vertexCount = frame.vertices.size();
        operation_.indexData->indexStart = 0;
        operation_.indexData->indexCount = frame.indices.size();
    }

    Ogre::v1::RenderOperation& Operation() noexcept { return operation_; }

private:
    Ogre::v1::RenderOperation operation_{};
    std::size_t vertexCapacity_ = 0;
    std::size_t indexCapacity_ = 0;
};

class OgreUiBackend final : public Ogre::CompositorWorkspaceListener
{
public:
    void Register(const Renderer* renderer)
    {
        if (!renderer || renderer->Backend() == RendererBackend::Null)
            return;
        if (!renderers_.insert(renderer).second)
            return;

        if (renderers_.size() == 1u)
        {
            auto* root = Ogre::Root::getSingletonPtr();
            if (!root || !root->getRenderSystem() || !root->getCompositorManager2())
            {
                renderers_.erase(renderer);
                throw std::logic_error("Ogre is not ready for editor UI registration");
            }
            compositorManager_ = root->getCompositorManager2();
            compositorManager_->addListener(this);
        }
    }

    void Unregister(const Renderer* renderer) noexcept
    {
        renderers_.erase(renderer);
        if (renderers_.empty())
            Shutdown();
    }

    void workspacePosUpdate(Ogre::CompositorWorkspace* workspace) override
    {
        if (!workspace || renderers_.empty())
            return;

        Ogre::TextureGpu* target = workspace->getFinalTarget();
        if (!target)
            return;
        const Ogre::String targetName = target->getNameStr();
        if (targetName.rfind("PlagC/Target", 0) == 0)
            return; // Editor scene targets are sampled by the UI; never draw the UI into itself.

        const Renderer* renderer = nullptr;
        const UiDrawData* frame = nullptr;
        for (const Renderer* candidate : renderers_)
        {
            const UiDrawData* pending = PendingUIFrame(candidate);
            if (!pending || pending->commands.empty())
                continue;

            const auto width = static_cast<unsigned>(std::lround(pending->width));
            const auto height = static_cast<unsigned>(std::lround(pending->height));
            if (width == target->getWidth() && height == target->getHeight())
            {
                renderer = candidate;
                frame = pending;
                break;
            }
        }
        if (!renderer || !frame)
            return;

        auto* root = Ogre::Root::getSingletonPtr();
        if (!root || !root->getCompositorManager2())
            return;
        const std::size_t frameNumber = root->getCompositorManager2()->getFrameCount();
        if (lastRenderedFrame_ == frameNumber && lastTarget_ == target)
            return;

        EnsureResources(renderer->Backend());
        Render(*frame, target, renderer->Backend());
        lastRenderedFrame_ = frameNumber;
        lastTarget_ = target;
    }

private:
    void Shutdown() noexcept
    {
        try
        {
            if (compositorManager_)
                compositorManager_->removeListener(this);

            auto* root = Ogre::Root::getSingletonPtr();
            auto* renderSystem = root ? root->getRenderSystem() : nullptr;
            if (renderSystem && renderPass_)
                renderSystem->destroyRenderPassDescriptor(renderPass_);
            renderPass_ = nullptr;
            renderTarget_ = nullptr;

            buffers_.reset();
            psoCache_.reset();

            if (root && samplerblock_)
                root->getHlmsManager()->destroySamplerblock(samplerblock_);
            samplerblock_ = nullptr;
            pass_ = nullptr;
            material_.setNull();
            backend_ = RendererBackend::Auto;
            lastTarget_ = nullptr;
            lastRenderedFrame_ = std::numeric_limits<std::size_t>::max();
            compositorManager_ = nullptr;
        }
        catch (...)
        {
            compositorManager_ = nullptr;
            renderPass_ = nullptr;
            renderTarget_ = nullptr;
            samplerblock_ = nullptr;
            pass_ = nullptr;
            buffers_.reset();
            psoCache_.reset();
        }
    }

    void EnsureResources(RendererBackend backend)
    {
        if (backend != RendererBackend::OpenGL && backend != RendererBackend::Vulkan)
            throw std::logic_error("editor UI requires an Ogre OpenGL or Vulkan backend");

        if (backend_ != RendererBackend::Auto && backend_ != backend)
            throw std::logic_error("editor UI backend cannot switch graphics API at runtime");
        backend_ = backend;

        auto* root = Ogre::Root::getSingletonPtr();
        auto* renderSystem = root ? root->getRenderSystem() : nullptr;
        if (!root || !renderSystem)
            throw std::logic_error("Ogre render system is unavailable for editor UI");

        if (!psoCache_)
            psoCache_ = std::make_unique<Ogre::PsoCacheHelper>(renderSystem);
        if (!buffers_)
            buffers_ = std::make_unique<UiBuffers>();
        if (!samplerblock_)
        {
            Ogre::HlmsSamplerblock sampler;
            samplerblock_ = root->getHlmsManager()->getSamplerblock(sampler);
        }
        if (!pass_)
            CreateMaterial(backend);
    }

    void CreateMaterial(RendererBackend backend)
    {
        auto& programs = Ogre::HighLevelGpuProgramManager::getSingleton();
        const bool vulkan = backend == RendererBackend::Vulkan;
        const char* vertexName = vulkan ? UiVertexProgramVK : UiVertexProgramGL;
        const char* pixelName = vulkan ? UiPixelProgramVK : UiPixelProgramGL;
        const char* language = vulkan ? "glslvk" : "glsl";

        Ogre::HighLevelGpuProgramPtr vertex = programs.getByName(
            vertexName, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
        if (vertex.isNull())
        {
            vertex = programs.createProgram(vertexName,
                Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
                language, Ogre::GPT_VERTEX_PROGRAM);
            vertex->setSource(vulkan ? VertexShaderVK : VertexShaderGL);
            if (vulkan)
                vertex->setPrefabRootLayout(Ogre::PrefabRootLayout::Standard);
            vertex->load();
        }

        Ogre::HighLevelGpuProgramPtr pixel = programs.getByName(
            pixelName, Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
        if (pixel.isNull())
        {
            pixel = programs.createProgram(pixelName,
                Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME,
                language, Ogre::GPT_FRAGMENT_PROGRAM);
            pixel->setSource(vulkan ? PixelShaderVK : PixelShaderGL);
            if (vulkan)
                pixel->setPrefabRootLayout(Ogre::PrefabRootLayout::Standard);
            pixel->load();
        }

        auto& materials = Ogre::MaterialManager::getSingleton();
        material_ = materials.getByName(UiMaterialName,
                                        Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
        if (material_.isNull())
            material_ = materials.create(UiMaterialName,
                                         Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);

        pass_ = material_->getTechnique(0)->getPass(0);
        pass_->setVertexProgram(vertexName);
        pass_->setFragmentProgram(pixelName);

        Ogre::HlmsBlendblock blendblock(*pass_->getBlendblock());
        blendblock.mSourceBlendFactor = Ogre::SBF_SOURCE_ALPHA;
        blendblock.mDestBlendFactor = Ogre::SBF_ONE_MINUS_SOURCE_ALPHA;
        blendblock.mSourceBlendFactorAlpha = Ogre::SBF_ONE_MINUS_SOURCE_ALPHA;
        blendblock.mDestBlendFactorAlpha = Ogre::SBF_ZERO;
        blendblock.mBlendOperation = Ogre::SBO_ADD;
        blendblock.mBlendOperationAlpha = Ogre::SBO_ADD;
        blendblock.mSeparateBlend = true;
        blendblock.mIsTransparent = true;

        Ogre::HlmsMacroblock macroblock(*pass_->getMacroblock());
        macroblock.mCullMode = Ogre::CULL_NONE;
        macroblock.mDepthFunc = Ogre::CMPF_ALWAYS_PASS;
        macroblock.mDepthCheck = false;
        macroblock.mDepthWrite = false;
        macroblock.mScissorTestEnabled = true;

        pass_->setBlendblock(blendblock);
        pass_->setMacroblock(macroblock);
    }

    void EnsureRenderPass(Ogre::TextureGpu* target)
    {
        auto* renderSystem = Ogre::Root::getSingletonPtr()->getRenderSystem();
        if (renderPass_ && renderTarget_ == target)
            return;

        if (renderPass_)
            renderSystem->destroyRenderPassDescriptor(renderPass_);
        renderPass_ = renderSystem->createRenderPassDescriptor();
        renderTarget_ = target;
        renderPass_->mColour[0].texture = target;
        renderPass_->mColour[0].loadAction = Ogre::LoadAction::Load;
        renderPass_->mColour[0].storeAction = Ogre::StoreAction::StoreAndMultisampleResolve;
        renderPass_->entriesModified(Ogre::RenderPassDescriptor::All);
    }

    Ogre::TextureGpu* ResolveTexture(const UiTextureRef& ref) const
    {
        auto* root = Ogre::Root::getSingletonPtr();
        auto* renderSystem = root ? root->getRenderSystem() : nullptr;
        auto* textures = renderSystem ? renderSystem->getTextureGpuManager() : nullptr;
        if (!textures || ref.value == 0)
            return nullptr;

        const char* kind = ref.kind == UiTextureKind::Target ? "Target" : "AssetTexture";
        const std::string name = std::string("PlagC/") + kind + std::to_string(ref.value);
        return textures->findTextureNoThrow(Ogre::IdString(name));
    }

    void UpdateProjection(float width, float height, RendererBackend backend)
    {
        const bool vulkan = backend == RendererBackend::Vulkan;
        const Ogre::Matrix4 projection(
            2.0f / width, 0.0f, 0.0f, -1.0f,
            0.0f, (vulkan ? 2.0f : -2.0f) / height, 0.0f,
            (vulkan ? -1.0f : 1.0f),
            0.0f, 0.0f, -1.0f, 0.0f,
            0.0f, 0.0f, 0.0f, 1.0f);
        pass_->getVertexProgramParameters()->setNamedConstant("ProjectionMatrix", projection);
    }

    void Render(const UiDrawData& frame, Ogre::TextureGpu* target, RendererBackend backend)
    {
        if (frame.width <= 0.0f || frame.height <= 0.0f ||
            frame.vertices.empty() || frame.indices.empty() || frame.commands.empty())
            return;

        auto* renderSystem = Ogre::Root::getSingletonPtr()->getRenderSystem();
        EnsureRenderPass(target);
        buffers_->Upload(frame);
        UpdateProjection(frame.width, frame.height, backend);

        psoCache_->clearState();
        psoCache_->setRenderTarget(renderPass_);
        psoCache_->setMacroblock(pass_->getMacroblock());
        psoCache_->setBlendblock(pass_->getBlendblock());
        psoCache_->setVertexShader(const_cast<Ogre::GpuProgramPtr&>(pass_->getVertexProgram()));
        psoCache_->setPixelShader(const_cast<Ogre::GpuProgramPtr&>(pass_->getFragmentProgram()));

        Ogre::v1::RenderOperation& operation = buffers_->Operation();
        const Ogre::VertexElement2VecVec vertexElements =
            operation.vertexData->vertexDeclaration->convertToV2();
        psoCache_->setVertexFormat(vertexElements, operation.operationType, false);

        Ogre::Vector4 viewport(0.0f, 0.0f, 1.0f, 1.0f);
        bool passBegun = false;
        for (const UiDrawCommand& command : frame.commands)
        {
            Ogre::TextureGpu* texture = ResolveTexture(command.texture);
            if (!texture || command.indexCount == 0)
                continue;

            const float left = std::clamp(command.clipLeft / frame.width, 0.0f, 1.0f);
            const float top = std::clamp(command.clipTop / frame.height, 0.0f, 1.0f);
            const float right = std::clamp(command.clipRight / frame.width, left, 1.0f);
            const float bottom = std::clamp(command.clipBottom / frame.height, top, 1.0f);
            if (right <= left || bottom <= top)
                continue;

            Ogre::Vector4 scissors(left, top, right - left, bottom - top);
            renderSystem->beginRenderPassDescriptor(
                renderPass_, target, 0, &viewport, &scissors, 1, false, false);
            renderSystem->executeRenderPassDescriptorDelayedActions();
            passBegun = true;

            Ogre::HlmsPso* pso = psoCache_->getPso();
            renderSystem->_setPipelineStateObject(pso);
            renderSystem->bindGpuProgramParameters(
                Ogre::GPT_VERTEX_PROGRAM, pass_->getVertexProgramParameters(), Ogre::GPV_ALL);
            renderSystem->_setTexture(0, texture, false);
            renderSystem->_setHlmsSamplerblock(0, samplerblock_);

            operation.indexData->indexStart = command.firstIndex;
            operation.indexData->indexCount = command.indexCount;
            Ogre::v1::CbRenderOp draw(operation);
            renderSystem->_setRenderOperation(&draw);
            renderSystem->_render(operation);
        }

        if (passBegun)
            renderSystem->endRenderPassDescriptor();
    }

    std::unordered_set<const Renderer*> renderers_;
    Ogre::CompositorManager2* compositorManager_ = nullptr;
    RendererBackend backend_ = RendererBackend::Auto;
    std::unique_ptr<Ogre::PsoCacheHelper> psoCache_;
    std::unique_ptr<UiBuffers> buffers_;
    Ogre::HlmsSamplerblock const* samplerblock_ = nullptr;
    Ogre::MaterialPtr material_;
    Ogre::Pass* pass_ = nullptr;
    Ogre::RenderPassDescriptor* renderPass_ = nullptr;
    Ogre::TextureGpu* renderTarget_ = nullptr;
    Ogre::TextureGpu* lastTarget_ = nullptr;
    std::size_t lastRenderedFrame_ = std::numeric_limits<std::size_t>::max();
};

OgreUiBackend& Backend()
{
    static OgreUiBackend backend;
    return backend;
}

} // namespace

void RegisterOgreUIRenderer(const Renderer* renderer)
{
    Backend().Register(renderer);
}

void UnregisterOgreUIRenderer(const Renderer* renderer) noexcept
{
    Backend().Unregister(renderer);
}

} // namespace PlagC::Detail
