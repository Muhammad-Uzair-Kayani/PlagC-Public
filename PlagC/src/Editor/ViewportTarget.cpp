#include "pgcpch.h"
#include <PlagC/Editor/ViewportTarget.h>
#include <PlagC/Thread.h>

namespace PlagC::Editor {

ViewportTarget::ViewportTarget(Renderer& renderer) : renderer_(renderer) {
    Thread::RequireMain();
}

ViewportTarget::~ViewportTarget() {
    if (handle_ && Thread::IsMain()) {
        try {
            renderer_.DestroyTarget(handle_);
        } catch (...) {
            // Engine shutdown must not throw from an editor helper destructor.
        }
    }
}

void ViewportTarget::Resize(unsigned width, unsigned height) {
    Thread::RequireMain();
    width = width ? width : 1;
    height = height ? height : 1;

    if (!handle_) {
        handle_ = renderer_.CreateTarget(width, height);
        width_ = width;
        height_ = height;
        ++revision_;
        return;
    }

    if (width_ == width && height_ == height)
        return;

    // Recreate instead of Renderer::ResizeTarget so the public TargetHandle
    // continues to match Ogre's deterministic PlagC/Target<id> texture name.
    // Presentation adapters can therefore resolve the texture without exposing
    // Renderer::Impl or backend-native GL/Vulkan objects.
    const auto replacement = renderer_.CreateTarget(width, height);
    try {
        renderer_.DestroyTarget(handle_);
    } catch (...) {
        try {
            renderer_.DestroyTarget(replacement);
        } catch (...) {
        }
        throw;
    }

    handle_ = replacement;
    width_ = width;
    height_ = height;
    ++revision_;
}

void ViewportTarget::Reset() {
    Thread::RequireMain();
    if (!handle_)
        return;
    renderer_.DestroyTarget(handle_);
    handle_ = {};
    width_ = 1;
    height_ = 1;
    ++revision_;
}

} // namespace PlagC::Editor
