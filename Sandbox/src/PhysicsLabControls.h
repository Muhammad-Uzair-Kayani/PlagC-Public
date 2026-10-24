#pragma once
#include <PlagC/Events.h>
#include <optional>
namespace PhysicsLabControls {
// Use modifiers captured with the press: live state may already reflect a queued release.
inline std::optional<unsigned> SceneForPress(const PlagC::Event& event,unsigned current) {
    using namespace PlagC;
    if(event.type!=EventType::KeyPressed)return {};
    if(event.key>=KeyCode::F1&&event.key<=KeyCode::F12){
        unsigned scene=unsigned(event.key)-unsigned(KeyCode::F1);
        if((event.modifiers&1)&&scene<2)scene+=12;
        return scene;
    }
    if(event.key==KeyCode::PageDown)return (current+1)%14;
    if(event.key==KeyCode::PageUp)return (current+13)%14;
    return {};
}
}
