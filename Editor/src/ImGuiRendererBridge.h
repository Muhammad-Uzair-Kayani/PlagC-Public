#pragma once

#include <PlagC/Renderer/Renderer.h>
#include <cstdint>

struct ImDrawData;

namespace PlagC::Editor {

class ImGuiRendererBridge {
public:
    void Initialize(Renderer&);
    void Submit(Renderer&, const ImDrawData*);
    void Shutdown(Renderer&);

    static std::uint64_t TextureID(TextureHandle) noexcept;
    static std::uint64_t TargetID(TargetHandle) noexcept;
    static double LastSubmitMilliseconds() noexcept;

private:
    TextureHandle fontTexture_{};
};

} // namespace PlagC::Editor
