#include "pgcpch.h"
#include <PlagC/Editor/OgreViewportInterop.h>
#include <PlagC/Thread.h>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4244 4324 4996)
#endif
#include <OgreIdString.h>
#include <OgreRenderSystem.h>
#include <OgreRoot.h>
#include <OgreTextureGpuManager.h>
#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace PlagC::Editor {

OgreViewportBinding ResolveOgreViewportBinding(const ViewportTarget& viewport) {
    Thread::RequireMain();

    const auto target = viewport.Handle();
    if (!target)
        return {};
    if (viewport.Backend() == RendererBackend::Null)
        throw std::logic_error("NULL renderer has no presentable editor viewport texture");

    auto* root = Ogre::Root::getSingletonPtr();
    if (!root || !root->getRenderSystem())
        throw std::logic_error("Ogre render system is not available");

    auto* manager = root->getRenderSystem()->getTextureGpuManager();
    if (!manager)
        throw std::logic_error("Ogre texture manager is not available");

    const auto name = std::string("PlagC/Target") + std::to_string(target.value);
    auto* texture = manager->findTextureNoThrow(Ogre::IdString(name));
    if (!texture)
        throw std::logic_error("editor viewport target texture is stale or unavailable");

    return {texture, viewport.Backend(), target, viewport.Revision()};
}

} // namespace PlagC::Editor
