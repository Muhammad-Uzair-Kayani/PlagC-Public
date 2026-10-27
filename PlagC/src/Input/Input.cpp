#include "pgcpch.h"
#include <PlagC/Input/Input.h>
#include <PlagC/LatencyProfiler.h>
#include <PlagC/Thread.h>
namespace PlagC {
void InputState::BeginFrame() { delta_ = {}; scroll_ = {}; }
void InputState::Reset() { *this = InputState{}; }
bool InputState::IsKeyPressed(KeyCode key) const {
    auto i = static_cast<std::size_t>(key);
    return i > 0 && i < keys_.size() && keys_[i];
}
bool InputState::IsMouseButtonPressed(MouseCode button) const {
    auto i = static_cast<std::size_t>(button);
    return i > 0 && i < buttons_.size() && buttons_[i];
}
void InputState::Apply(const Event& e) {
    if (e.type == EventType::Focus && !e.code) { Reset(); return; }
    auto key = static_cast<std::size_t>(e.key);
    if (key > 0 && key < keys_.size()) {
        if (e.type == EventType::KeyPressed || e.type == EventType::KeyRepeated) keys_[key] = true;
        if (e.type == EventType::KeyReleased) keys_[key] = false;
    }
    auto button = static_cast<std::size_t>(e.button);
    if (button > 0 && button < buttons_.size()) {
        if (e.type == EventType::MousePressed) buttons_[button] = true;
        if (e.type == EventType::MouseReleased) buttons_[button] = false;
    }
    if (e.type == EventType::MouseMoved) {
        if (positioned_) { delta_.x += e.x - position_.x; delta_.y += e.y - position_.y; }
        position_ = {e.x, e.y}; positioned_ = true;
    }
    if (e.type == EventType::MouseScrolled) { scroll_.x += e.x; scroll_.y += e.y; }
}
InputState Input::state_;
bool Input::IsKeyPressed(KeyCode k) { Thread::RequireMain(); LatencyProfiler::OnGameRead(); return state_.IsKeyPressed(k); }
bool Input::IsMouseButtonPressed(MouseCode b) { Thread::RequireMain(); LatencyProfiler::OnGameRead(); return state_.IsMouseButtonPressed(b); }
Vec2 Input::GetMousePosition() { Thread::RequireMain(); return state_.GetMousePosition(); }
Vec2 Input::GetMouseDelta() { Thread::RequireMain(); LatencyProfiler::OnGameRead(); return state_.GetMouseDelta(); }
Vec2 Input::GetScrollDelta() { Thread::RequireMain(); return state_.GetScrollDelta(); }
}
