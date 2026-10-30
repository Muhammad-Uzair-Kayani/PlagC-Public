// Build with Editor/src/ImGuiInputBridge.cpp and the four Dear ImGui core sources.
#include "../../Editor/src/ImGuiInputBridge.h"
#include <imgui.h>
#include <cassert>
int main() {
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.DisplaySize = {640,480}; io.IniFilename = nullptr;
    unsigned char* pixels; int w,h;
    io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    PlagC::EventBus bus; PlagC::Editor::ImGuiInputBridge bridge;
    bridge.Initialize(bus);
    auto frame = [] { ImGui::NewFrame(); ImGui::EndFrame(); };
    PlagC::Event key{PlagC::EventType::KeyPressed};
    key.key = PlagC::KeyCode::LeftControl; bus.Dispatch(key); frame();
    assert(io.KeyCtrl);
    bus.Dispatch({PlagC::EventType::Focus,0,0,0}); frame();
    bus.Dispatch({PlagC::EventType::Focus,0,0,1}); frame();
    key.key = PlagC::KeyCode::A; bus.Dispatch(key); frame();
    assert(!io.KeyCtrl && !io.KeyShift && !io.KeyAlt);
    bridge.Shutdown(); ImGui::DestroyContext();
}
