#pragma once
#include <PlagC/Events.h>
#include <array>
#include <cstddef>
namespace PlagC {
struct Vec2 { double x = 0, y = 0; };
// Standalone state supports deterministic input replay/testing without a window.
class InputState {
public:
    void BeginFrame();
    void Reset();
    void RebaseCursor() { positioned_ = false; delta_ = {}; }
    void Apply(const Event&);
    bool IsKeyPressed(KeyCode) const;
    bool IsMouseButtonPressed(MouseCode) const;
    Vec2 GetMousePosition() const { return position_; }
    Vec2 GetMouseDelta() const { return delta_; }
    Vec2 GetScrollDelta() const { return scroll_; }
private:
    std::array<bool, static_cast<std::size_t>(KeyCode::Count)> keys_{};
    std::array<bool, static_cast<std::size_t>(MouseCode::Count)> buttons_{};
    Vec2 position_, delta_, scroll_;
    bool positioned_ = false;
};
// Single main-window facade; all operations belong to the main thread.
class Input {
public:
    static bool IsKeyPressed(KeyCode);
    static bool IsMouseButtonPressed(MouseCode);
    static Vec2 GetMousePosition();
    static Vec2 GetMouseDelta();
    static Vec2 GetScrollDelta();
    static double GetMouseX() { return GetMousePosition().x; }
    static double GetMouseY() { return GetMousePosition().y; }
private:
    friend class Window;
    static InputState state_;
};
}
