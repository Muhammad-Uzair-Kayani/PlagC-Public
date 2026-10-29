#pragma once

#include <PlagC/Events.h>

namespace PlagC::Editor {

class ImGuiInputBridge {
public:
    void Initialize(EventBus&);
    void Shutdown();

private:
    void OnKey(const Event&, bool down);
    void UpdateModifiers(KeyCode, bool down);

    EventBus::Connection keyPressed_;
    EventBus::Connection keyReleased_;
    EventBus::Connection textInput_;
    EventBus::Connection mouseMoved_;
    EventBus::Connection mouseScrolled_;
    EventBus::Connection mousePressed_;
    EventBus::Connection mouseReleased_;
    EventBus::Connection focus_;

    bool leftShift_ = false;
    bool rightShift_ = false;
    bool leftControl_ = false;
    bool rightControl_ = false;
    bool leftAlt_ = false;
    bool rightAlt_ = false;
};

} // namespace PlagC::Editor
