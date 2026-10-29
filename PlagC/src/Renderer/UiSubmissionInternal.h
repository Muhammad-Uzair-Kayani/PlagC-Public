#pragma once
#include <PlagC/Renderer/Renderer.h>

namespace PlagC::Detail {
const UiDrawData* PendingUIFrame(const Renderer*) noexcept;
void RemoveUIFrame(const Renderer*) noexcept;
void RegisterOgreUIRenderer(const Renderer*);
void UnregisterOgreUIRenderer(const Renderer*) noexcept;
}
