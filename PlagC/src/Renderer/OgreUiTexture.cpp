#include "pgcpch.h"
#include <PlagC/Renderer/UiTexture.h>
#include <PlagC/Thread.h>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4244 4267 4324 4996)
#endif
#include <OgreImage2.h>
#include <OgreRoot.h>
#include <OgreRenderSystem.h>
#include <OgreTextureBox.h>
#include <OgreTextureGpu.h>
#include <OgreTextureGpuManager.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <atomic>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

namespace PlagC {
namespace {

constexpr std::uint64_t UiTextureBase = std::uint64_t{1} << 62;
std::atomic<std::uint64_t> nextUiTexture{UiTextureBase};

std::string UiTextureName(std::uint64_t id)
{
    return "PlagC/AssetTexture" + std::to_string(id);
}

Ogre::TextureGpuManager& TextureManager()
{
    auto* root = Ogre::Root::getSingletonPtr();
    auto* renderSystem = root ? root->getRenderSystem() : nullptr;
    auto* manager = renderSystem ? renderSystem->getTextureGpuManager() : nullptr;
    if (!manager)
        throw std::logic_error("Ogre texture manager is unavailable for editor UI");
    return *manager;
}

} // namespace

TextureHandle CreateUiTexture(Renderer& renderer, unsigned width, unsigned height,
                              std::span<const std::uint8_t> rgba, bool srgb)
{
    Thread::RequireMain();
    if (renderer.Backend() == RendererBackend::Null)
        throw std::logic_error("editor UI textures require a graphical renderer");
    if (!width || !height || width > 8192 || height > 8192 ||
        rgba.size() != std::uint64_t(width) * height * 4u)
        throw std::invalid_argument("invalid RGBA editor UI texture");

    const std::uint64_t id = nextUiTexture.fetch_add(1u, std::memory_order_relaxed);
    if (id >= (std::uint64_t{1} << 63))
        throw std::overflow_error("editor UI texture handle space exhausted");

    auto image = std::make_unique<Ogre::Image2>();
    const Ogre::PixelFormatGpu format =
        srgb ? Ogre::PFG_RGBA8_UNORM_SRGB : Ogre::PFG_RGBA8_UNORM;
    image->createEmptyImage(width, height, 1u, Ogre::TextureTypes::Type2D, format);
    auto box = image->getData(0);
    for (unsigned row = 0; row < height; ++row)
    {
        std::memcpy(static_cast<unsigned char*>(box.data) + row * box.bytesPerRow,
                    rgba.data() + std::size_t(row) * width * 4u,
                    std::size_t(width) * 4u);
    }

    auto& manager = TextureManager();
    const Ogre::uint32 flags =
        srgb ? Ogre::TextureFlags::PrefersLoadingFromFileAsSRGB : 0u;
    Ogre::TextureGpu* texture = manager.createTexture(
        UiTextureName(id), Ogre::GpuPageOutStrategy::Discard, flags,
        Ogre::TextureTypes::Type2D);
    try
    {
        // Immediate-mode UI binds this resource directly as sampler2D. Ogre automatic
        // batching can replace a Type2D texture's display SRV with its shared Type2DArray
        // pool owner, which is incompatible with that shader contract.
        if (texture->hasAutomaticBatching() ||
            texture->getTextureType() != Ogre::TextureTypes::Type2D)
            throw std::logic_error(
                "editor UI texture must be a standalone Ogre Type2D resource");

        // Standalone programmatic textures do not have AutomaticBatching's metadata path.
        // Define the resource shape explicitly before asking Ogre to upload the Image2.
        texture->setResolution(width, height);
        texture->setPixelFormat(format);
        texture->scheduleTransitionTo(Ogre::GpuResidency::Resident, image.release(), true);
        manager.waitForStreamingCompletion();

        if (!texture->isDataReady())
            throw std::runtime_error("editor UI texture upload did not become ready");
    }
    catch (...)
    {
        manager.destroyTexture(texture);
        throw;
    }

    return TextureHandle{id};
}

void DestroyUiTexture(Renderer&, TextureHandle handle)
{
    Thread::RequireMain();
    if (!handle)
        return;
    if (handle.value < UiTextureBase || handle.value >= (std::uint64_t{1} << 63))
        throw std::invalid_argument("invalid editor UI texture handle");

    auto& manager = TextureManager();
    if (Ogre::TextureGpu* texture =
            manager.findTextureNoThrow(Ogre::IdString(UiTextureName(handle.value))))
        manager.destroyTexture(texture);
}

} // namespace PlagC
