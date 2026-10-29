#pragma once

#include <PlagC/Editor/ViewportTarget.h>
#include <cstdint>

namespace Ogre {
class TextureGpu;
}

namespace PlagC::Editor {

// Backend-integration data only. Normal editor panels should consume
// ViewportTarget and must not depend on Ogre or graphics-API objects.
struct OgreViewportBinding {
    Ogre::TextureGpu* texture = nullptr;
    RendererBackend backend = RendererBackend::Auto;
    TargetHandle target{};
    std::uint64_t revision = 0;

    explicit operator bool() const noexcept { return texture != nullptr; }
};

// Main-thread only. Resolves the Ogre render-target texture for a ViewportTarget.
// The returned pointer is borrowed and becomes stale when ViewportTarget::Revision
// changes or the target is reset/destroyed. The UI integration must rebuild any
// ImGui/OpenGL/Vulkan binding on a revision change.
OgreViewportBinding ResolveOgreViewportBinding(const ViewportTarget& viewport);

} // namespace PlagC::Editor
