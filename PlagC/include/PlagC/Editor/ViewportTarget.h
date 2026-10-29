#pragma once

#include <PlagC/Renderer/Renderer.h>
#include <cstdint>

namespace PlagC::Editor {

// Owns only the engine render target lifecycle. How that target is presented
// inside ImGui is backend-specific (Ogre/OpenGL/Vulkan) and remains outside
// this class so the editor UI never depends on raw GL/Vulkan handles.
class ViewportTarget {
public:
    explicit ViewportTarget(Renderer&);
    ~ViewportTarget();

    ViewportTarget(const ViewportTarget&) = delete;
    ViewportTarget& operator=(const ViewportTarget&) = delete;

    void Resize(unsigned width, unsigned height);
    void Reset();

    TargetHandle Handle() const noexcept { return handle_; }
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
