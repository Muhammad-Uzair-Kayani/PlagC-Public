#include "pgcpch.h"
#include "ImGuiInputBridge.h"
#include <imgui.h>

namespace PlagC::Editor {
namespace {

ImGuiKey ToImGuiKey(KeyCode key)
{
    switch (key)
    {
    case KeyCode::Space: return ImGuiKey_Space;
    case KeyCode::A: return ImGuiKey_A;
    case KeyCode::B: return ImGuiKey_B;
    case KeyCode::C: return ImGuiKey_C;
    case KeyCode::D: return ImGuiKey_D;
    case KeyCode::E: return ImGuiKey_E;
    case KeyCode::F: return ImGuiKey_F;
    case KeyCode::G: return ImGuiKey_G;
    case KeyCode::H: return ImGuiKey_H;
    case KeyCode::I: return ImGuiKey_I;
    case KeyCode::J: return ImGuiKey_J;
    case KeyCode::K: return ImGuiKey_K;
    case KeyCode::L: return ImGuiKey_L;
    case KeyCode::M: return ImGuiKey_M;
    case KeyCode::N: return ImGuiKey_N;
    case KeyCode::O: return ImGuiKey_O;
    case KeyCode::P: return ImGuiKey_P;
    case KeyCode::Q: return ImGuiKey_Q;
    case KeyCode::R: return ImGuiKey_R;
    case KeyCode::S: return ImGuiKey_S;
    case KeyCode::T: return ImGuiKey_T;
    case KeyCode::U: return ImGuiKey_U;
    case KeyCode::V: return ImGuiKey_V;
    case KeyCode::W: return ImGuiKey_W;
    case KeyCode::X: return ImGuiKey_X;
    case KeyCode::Y: return ImGuiKey_Y;
    case KeyCode::Z: return ImGuiKey_Z;
    case KeyCode::D0: return ImGuiKey_0;
    case KeyCode::D1: return ImGuiKey_1;
    case KeyCode::D2: return ImGuiKey_2;
    case KeyCode::D3: return ImGuiKey_3;
    case KeyCode::D4: return ImGuiKey_4;
    case KeyCode::D5: return ImGuiKey_5;
    case KeyCode::D6: return ImGuiKey_6;
    case KeyCode::D7: return ImGuiKey_7;
    case KeyCode::D8: return ImGuiKey_8;
    case KeyCode::D9: return ImGuiKey_9;
    case KeyCode::Escape: return ImGuiKey_Escape;
    case KeyCode::Enter: return ImGuiKey_Enter;
    case KeyCode::Tab: return ImGuiKey_Tab;
    case KeyCode::Backspace: return ImGuiKey_Backspace;
    case KeyCode::Left: return ImGuiKey_LeftArrow;
    case KeyCode::Right: return ImGuiKey_RightArrow;
    case KeyCode::Up: return ImGuiKey_UpArrow;
    case KeyCode::Down: return ImGuiKey_DownArrow;
    case KeyCode::LeftShift: return ImGuiKey_LeftShift;
    case KeyCode::RightShift: return ImGuiKey_RightShift;
    case KeyCode::LeftControl: return ImGuiKey_LeftCtrl;
    case KeyCode::RightControl: return ImGuiKey_RightCtrl;
    case KeyCode::LeftAlt: return ImGuiKey_LeftAlt;
    case KeyCode::RightAlt: return ImGuiKey_RightAlt;
    case KeyCode::F1: return ImGuiKey_F1;
    case KeyCode::F2: return ImGuiKey_F2;
    case KeyCode::F3: return ImGuiKey_F3;
    case KeyCode::F4: return ImGuiKey_F4;
    case KeyCode::F5: return ImGuiKey_F5;
    case KeyCode::F6: return ImGuiKey_F6;
    case KeyCode::F7: return ImGuiKey_F7;
    case KeyCode::F8: return ImGuiKey_F8;
    case KeyCode::F9: return ImGuiKey_F9;
    case KeyCode::F10: return ImGuiKey_F10;
    case KeyCode::F11: return ImGuiKey_F11;
    case KeyCode::F12: return ImGuiKey_F12;
    case KeyCode::Insert: return ImGuiKey_Insert;
    case KeyCode::Delete: return ImGuiKey_Delete;
    case KeyCode::Home: return ImGuiKey_Home;
    case KeyCode::End: return ImGuiKey_End;
    case KeyCode::PageUp: return ImGuiKey_PageUp;
    case KeyCode::PageDown: return ImGuiKey_PageDown;
    default: return ImGuiKey_None;
    }
}

int ToImGuiMouseButton(MouseCode button)
{
    switch (button)
    {
    case MouseCode::Left: return ImGuiMouseButton_Left;
    case MouseCode::Right: return ImGuiMouseButton_Right;
    case MouseCode::Middle: return ImGuiMouseButton_Middle;
    case MouseCode::Button4: return 3;
    case MouseCode::Button5: return 4;
    default: return -1;
    }
}

} // namespace

void ImGuiInputBridge::Initialize(EventBus& events)
{
    if (!ImGui::GetCurrentContext())
        throw std::logic_error("ImGui input bridge requires an ImGui context");

    ImGuiIO& io = ImGui::GetIO();
    io.BackendPlatformName = "PlagC_EventBus";
    io.BackendPlatformUserData = this;

    keyPressed_ = events.Subscribe(EventType::KeyPressed,
        [this](const Event& event) { OnKey(event, true); });
    keyReleased_ = events.Subscribe(EventType::KeyReleased,
        [this](const Event& event) { OnKey(event, false); });
    textInput_ = events.Subscribe(EventType::TextInput,
        [](const Event& event) {
            if (event.code > 0)
                ImGui::GetIO().AddInputCharacter(static_cast<unsigned int>(event.code));
        });
    mouseMoved_ = events.Subscribe(EventType::MouseMoved,
        [](const Event& event) {
            ImGui::GetIO().AddMousePosEvent(static_cast<float>(event.x),
                                            static_cast<float>(event.y));
        });
    mouseScrolled_ = events.Subscribe(EventType::MouseScrolled,
        [](const Event& event) {
            ImGui::GetIO().AddMouseWheelEvent(static_cast<float>(event.x),
                                              static_cast<float>(event.y));
        });
    mousePressed_ = events.Subscribe(EventType::MousePressed,
        [](const Event& event) {
            const int button = ToImGuiMouseButton(event.button);
            if (button >= 0)
                ImGui::GetIO().AddMouseButtonEvent(button, true);
        });
    mouseReleased_ = events.Subscribe(EventType::MouseReleased,
        [](const Event& event) {
            const int button = ToImGuiMouseButton(event.button);
            if (button >= 0)
                ImGui::GetIO().AddMouseButtonEvent(button, false);
        });
    focus_ = events.Subscribe(EventType::Focus,
        [](const Event& event) {
            ImGui::GetIO().AddFocusEvent(event.code != 0);
        });
}

void ImGuiInputBridge::Shutdown()
{
    focus_.Reset();
    mouseReleased_.Reset();
    mousePressed_.Reset();
    mouseScrolled_.Reset();
    mouseMoved_.Reset();
    textInput_.Reset();
    keyReleased_.Reset();
    keyPressed_.Reset();

    leftShift_ = rightShift_ = false;
    leftControl_ = rightControl_ = false;
    leftAlt_ = rightAlt_ = false;

    if (ImGui::GetCurrentContext())
    {
        ImGuiIO& io = ImGui::GetIO();
        if (io.BackendPlatformUserData == this)
        {
            io.BackendPlatformName = nullptr;
            io.BackendPlatformUserData = nullptr;
        }
    }
}

void ImGuiInputBridge::OnKey(const Event& event, bool down)
{
    if (!ImGui::GetCurrentContext())
        return;

    const ImGuiKey key = ToImGuiKey(event.key);
    if (key != ImGuiKey_None)
        ImGui::GetIO().AddKeyEvent(key, down);
    UpdateModifiers(event.key, down);
}

void ImGuiInputBridge::UpdateModifiers(KeyCode key, bool down)
{
    switch (key)
    {
    case KeyCode::LeftShift: leftShift_ = down; break;
    case KeyCode::RightShift: rightShift_ = down; break;
    case KeyCode::LeftControl: leftControl_ = down; break;
    case KeyCode::RightControl: rightControl_ = down; break;
    case KeyCode::LeftAlt: leftAlt_ = down; break;
    case KeyCode::RightAlt: rightAlt_ = down; break;
    default: break;
    }

    ImGuiIO& io = ImGui::GetIO();
    io.AddKeyEvent(ImGuiMod_Shift, leftShift_ || rightShift_);
    io.AddKeyEvent(ImGuiMod_Ctrl, leftControl_ || rightControl_);
    io.AddKeyEvent(ImGuiMod_Alt, leftAlt_ || rightAlt_);
}

} // namespace PlagC::Editor
