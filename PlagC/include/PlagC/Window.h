#pragma once
#include <memory>
#include <cstdint>
#include <string>
namespace PlagC {
class EventBus;
struct WindowConfig { int width = 1280, height = 720; std::string title = "PlagC — Phase 1 Sandbox"; bool nullPlatform = false; };
struct NativeWindow { enum class Kind { None,Win32,X11,Cocoa }; Kind kind=Kind::None; std::uintptr_t value=0; void* display=nullptr; };
class Window {
public:
    Window(EventBus&, const WindowConfig&);
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    void Poll();
    bool ShouldClose() const;
    void RequestClose();
    void SetTitle(const std::string&);
    void Resize(int, int);
    void SetCursorCaptured(bool);
    int Width() const;
    int Height() const;
    bool Minimized() const;
    NativeWindow NativeHandle() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}

