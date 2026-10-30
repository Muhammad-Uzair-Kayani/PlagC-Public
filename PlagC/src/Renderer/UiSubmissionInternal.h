#pragma once
#include <PlagC/Renderer/Renderer.h>

namespace Ogre { class TextureGpu; class CompositorWorkspace; }

namespace PlagC::Detail {
void CheckOgreUIOutput(const Renderer*);
void SetOgreUIOutput(const Renderer*, Ogre::TextureGpu*);
void AttachOgreUIWorkspace(const Renderer*, Ogre::CompositorWorkspace*);
void DetachOgreUIWorkspace(const Renderer*, Ogre::CompositorWorkspace*) noexcept;
void ValidateOgreUITextures(const UiDrawData&);

class OgreUIWorkspaceScope final {
public:
    OgreUIWorkspaceScope(const Renderer* renderer, Ogre::CompositorWorkspace* workspace)
        : renderer_(renderer), workspace_(workspace)
    {
        AttachOgreUIWorkspace(renderer_, workspace_);
    }

    ~OgreUIWorkspaceScope()
    {
        DetachOgreUIWorkspace(renderer_, workspace_);
    }

    OgreUIWorkspaceScope(const OgreUIWorkspaceScope&) = delete;
    OgreUIWorkspaceScope& operator=(const OgreUIWorkspaceScope&) = delete;

private:
    const Renderer* renderer_ = nullptr;
    Ogre::CompositorWorkspace* workspace_ = nullptr;
};

const UiDrawData* PendingUIFrame(const Renderer*) noexcept;
void RemoveUIFrame(const Renderer*) noexcept;
void RegisterOgreUIRenderer(const Renderer*);
void UnregisterOgreUIRenderer(const Renderer*) noexcept;
}
