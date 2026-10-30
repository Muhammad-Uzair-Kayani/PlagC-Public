#pragma once
#include <PlagC/Renderer/Renderer.h>

namespace Ogre { class TextureGpu; }

namespace PlagC::Detail {
void CheckOgreUIOutput(const Renderer*);
void SetOgreUIOutput(const Renderer*, Ogre::TextureGpu*);
const UiDrawData* PendingUIFrame(const Renderer*) noexcept;
void RemoveUIFrame(const Renderer*) noexcept;
void RegisterOgreUIRenderer(const Renderer*);
void UnregisterOgreUIRenderer(const Renderer*) noexcept;
}
