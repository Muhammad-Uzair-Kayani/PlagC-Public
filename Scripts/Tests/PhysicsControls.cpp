#include <PlagC/Application.h>
#include <PlagC/Input/Input.h>
#include "../../Sandbox/src/PhysicsLabControls.h"
#include <iostream>
#include <stdexcept>
using namespace PlagC;
void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);std::cout<<"[PASS] "<<message<<'\n';}
int main(){try{
 Check(EngineConfig{}.physicsLab,"normal launch configuration opens Physics Lab");
 EventBus bus;InputState input;unsigned selected=0,changes=0;
 auto connection=bus.Subscribe(EventType::KeyPressed,[&](const Event& e){if(auto next=PhysicsLabControls::SceneForPress(e,selected)){selected=*next;++changes;}});
 auto queue=[&](Event e){input.Apply(e);if(!bus.Post(e))throw std::runtime_error("event queue full");};
 for(unsigned i=0;i<12;++i){Event e{EventType::KeyPressed};e.key=KeyCode(unsigned(KeyCode::F1)+i);queue(e);bus.Pump();Check(selected==i,"queued function key selects corresponding scene");}
 for(auto shift:{KeyCode::LeftShift,KeyCode::RightShift}){
  Event press{EventType::KeyPressed};press.key=shift;queue(press);
  Event select{EventType::KeyPressed};select.key=KeyCode::F1;select.modifiers=1;queue(select);
  press.type=EventType::KeyReleased;queue(press);
  Check(!input.IsKeyPressed(shift),"Shift released before event pump");bus.Pump();Check(selected==12,"captured Shift modifier still selects LOD");
 }
 Event e{EventType::KeyPressed};e.key=KeyCode::F2;e.modifiers=1;queue(e);bus.Pump();Check(selected==13,"Shift F2 selects ragdolls");
 e.modifiers=0;e.key=KeyCode::PageDown;queue(e);bus.Pump();Check(selected==0,"Page Down wraps to first scene");
 e.key=KeyCode::PageUp;queue(e);bus.Pump();Check(selected==13,"Page Up wraps to last scene");
 auto before=changes;e.type=EventType::KeyRepeated;e.key=KeyCode::F1;queue(e);e.type=EventType::KeyReleased;queue(e);bus.Pump();Check(changes==before,"repeat and release do not reload scenes");
 e.type=EventType::KeyPressed;e.key=KeyCode::W;queue(e);bus.Pump();Check(changes==before&&input.IsKeyPressed(KeyCode::W),"movement keys update input without changing scenes");
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
