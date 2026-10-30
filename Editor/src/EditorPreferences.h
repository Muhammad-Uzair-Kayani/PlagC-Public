#pragma once

#include <PlagC/Renderer/Backend.h>

namespace PlagC {
class Engine;
}

namespace PlagC::Editor {

class EditorSession;

class EditorPreferences final {
public:
    void Initialize(Engine&);
    void Draw(Engine&, EditorSession&);
    void Shutdown() noexcept;

    void Toggle() noexcept { open_ = !open_; }
    bool Open() const noexcept { return open_; }
    bool RestartRequired(RendererBackend active) const noexcept
    {
        return preferred_ != RendererBackend::Auto && preferred_ != active;
    }

private:
    bool Restart(Engine&);

    bool open_ = false;
    RendererBackend preferred_ = RendererBackend::Auto;
};

} // namespace PlagC::Editor
