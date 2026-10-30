#include "pgcpch.h"
#include <PlagC/Input/Input.h>
#include <PlagC/LatencyProfiler.h>
#include <PlagC/Thread.h>

namespace PlagC
{
void InputState::BeginFrame()
{
    delta_ = {};
    scroll_ = {};
}

void InputState::Reset()
{
    *this = InputState{};
}

void InputState::ResetKeyboard()
{
    keys_.fill(false);
}

void InputState::ResetMouse()
{
    buttons_.fill(false);
    position_ = {};
    delta_ = {};
    scroll_ = {};
    positioned_ = false;
}

bool InputState::IsKeyPressed(KeyCode key) const
{
    const auto index = static_cast<std::size_t>(key);
    return index > 0 && index < keys_.size() && keys_[index];
}

bool InputState::IsMouseButtonPressed(MouseCode button) const
{
    const auto index = static_cast<std::size_t>(button);
    return index > 0 && index < buttons_.size() && buttons_[index];
}

void InputState::Apply(const Event &event)
{
    if (event.type == EventType::Focus && !event.code)
    {
        Reset();
        return;
    }

    const auto key = static_cast<std::size_t>(event.key);
    if (key > 0 && key < keys_.size())
    {
        if (event.type == EventType::KeyPressed || event.type == EventType::KeyRepeated)
            keys_[key] = true;
        if (event.type == EventType::KeyReleased)
            keys_[key] = false;
    }

    const auto button = static_cast<std::size_t>(event.button);
    if (button > 0 && button < buttons_.size())
    {
        if (event.type == EventType::MousePressed)
            buttons_[button] = true;
        if (event.type == EventType::MouseReleased)
            buttons_[button] = false;
    }

    if (event.type == EventType::MouseMoved)
    {
        if (positioned_)
        {
            delta_.x += event.x - position_.x;
            delta_.y += event.y - position_.y;
        }
        position_ = {event.x, event.y};
        positioned_ = true;
    }

    if (event.type == EventType::MouseScrolled)
    {
        scroll_.x += event.x;
        scroll_.y += event.y;
    }
}

InputState Input::state_;

bool Input::IsKeyPressed(KeyCode key)
{
    Thread::RequireMain();
    LatencyProfiler::OnInputRead();
    return state_.IsKeyPressed(key);
}

bool Input::IsMouseButtonPressed(MouseCode button)
{
    Thread::RequireMain();
    LatencyProfiler::OnInputRead();
    return state_.IsMouseButtonPressed(button);
}

Vec2 Input::GetMousePosition()
{
    Thread::RequireMain();
    return state_.GetMousePosition();
}

Vec2 Input::GetMouseDelta()
{
    Thread::RequireMain();
    LatencyProfiler::OnInputRead();
    return state_.GetMouseDelta();
}

Vec2 Input::GetScrollDelta()
{
    Thread::RequireMain();
    return state_.GetScrollDelta();
}
} // namespace PlagC
