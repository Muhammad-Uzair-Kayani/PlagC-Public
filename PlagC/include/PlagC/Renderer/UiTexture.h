#pragma once

#include <PlagC/Renderer/Renderer.h>
#include <cstdint>
#include <span>

namespace PlagC {

// Creates a standalone 2D texture for immediate-mode UI sampling.
// Unlike authored asset textures, this path deliberately avoids Ogre texture-array batching
// because the UI shader declares a plain texture2D binding.
TextureHandle CreateUiTexture(Renderer&, unsigned width, unsigned height,
                              std::span<const std::uint8_t> rgba, bool srgb = false);
void DestroyUiTexture(Renderer&, TextureHandle);

} // namespace PlagC
