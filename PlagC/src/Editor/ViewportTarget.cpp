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

    renderer_.ResizeTarget(handle_, width, height);
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
