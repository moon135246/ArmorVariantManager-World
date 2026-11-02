#include "Transform.h"
#include <windows.h>
#include <cmath>
#include <algorithm>
namespace worlddiff::app {
namespace {
template<class T> bool Read(uintptr_t address,T& value){
 if(address<0x10000||address>0x00007FFFFFFEFFFFull)return false;
 SIZE_T got=0;return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),&value,sizeof(value),&got)&&got==sizeof(value);
}
uintptr_t Pointer(uintptr_t address){uintptr_t value=0;return Read(address,value)&&value>=0x10000?value:0;}
int Integer(uintptr_t address,int minimum,int maximum){int value=-1;return Read(address,value)&&value>=minimum&&value<=maximum?value:-1;}
int Flag(uintptr_t address){uint8_t value=0;return Read(address,value)&&value<=1?value:-1;}
}
PlayerSnapshot ReadPlayerSnapshot(){
 PlayerSnapshot p;const auto base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
 // Steam 15.23.00. Pointer chains are read-only and derived from the local
 // MHWI-VisualControllerExtended 1.4.0 reference; no additional hooks or writes.
 auto map=Pointer(base+0x500CDA0);if(map)map=Pointer(map+0x80);if(map)map=Pointer(map+0xEEC0);
 uint16_t mapId=0;if(map&&Read(map+0x118,mapId))p.map=mapId;
 auto manager=Pointer(base+0x50139A0);if(!manager)return p;
 p.entity=Pointer(manager+0x50);if(!p.entity)return p;
 auto weapon=Pointer(p.entity+0xC0);if(weapon)weapon=Pointer(weapon+0x8);if(weapon)weapon=Pointer(weapon+0x78);
 if(weapon)p.weapon=Integer(weapon+0x2E8,0,13);
 auto state=Pointer(p.entity+0x76B0);
 if(state){if(p.weapon==3)p.spirit=Integer(state+0x2370,0,3);if(p.weapon==2){p.demon=Flag(state+0x2368);p.archdemon=Flag(state+0x2369);}}
 if(state&&p.weapon==10){bool read=Read(state+0x2368,p.extractTimers[0])&&Read(state+0x236C,p.extractTimers[1])&&Read(state+0x2370,p.extractTimers[2]);if(read)p.extracts=ExtractMask(p.extractTimers);}
 auto action=Pointer(p.entity+0x468);if(action)p.lmt=Integer(action+0xE9C4,0,0x7fffffff);
 auto vitality=Pointer(p.entity+0x7630);float health=-1,max=-1;
 if(vitality&&Read(vitality+0x64,health)&&Read(vitality+0x60,max)&&std::isfinite(health)&&std::isfinite(max)&&health>=0&&max>0&&max<100000){
  p.health=health;p.maxHealth=max;p.healthPercent=std::clamp(health*100.f/max,0.f,100.f);
 }
 return p;
}
}
