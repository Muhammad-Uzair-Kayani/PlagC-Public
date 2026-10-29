#pragma once

#include <PlagC/Renderer/Renderer.h>
#include <cstdint>

namespace PlagC::Editor {

// Owns only the engine render target lifecycle. How that target is presented
// inside ImGui is backend-specific (Ogre/OpenGL/Vulkan) and remains outside
// this class so normal editor panels never depend on raw graphics handles.
//
// Resize may replace the TargetHandle. Presentation adapters must observe
// Revision() and rebuild any backend-specific texture/descriptor binding when
// it changes.
class ViewportTarget {
public:
    explicit ViewportTarget(Renderer&);
    ~ViewportTarget();

    ViewportTarget(const ViewportTarget&) = delete;
    ViewportTarget& operator=(const ViewportTarget&) = delete;

    void Resize(unsigned width, unsigned height);
    void Reset();

    TargetHandle Handle() const noexcept { return handle_; }
    RendererBackend Backend() const noexcept { return renderer_.Backend(); }
    unsigned Width() const noexcept { return width_; }
    unsigned Height() const noexcept { return height_; }
    float Aspect() const noexcept {
        return static_cast<float>(width_) / static_cast<float>(height_);
    }
    std::uint64_t Revision() const noexcept { return revision_; }

private:
    Renderer& renderer_;
    TargetHandle handle_{};
    unsigned width_ = 1;
    unsigned height_ = 1;
    std::uint64_t revision_ = 0;
};

} // namespace PlagC::Editor
