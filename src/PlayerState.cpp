#include "Transform.h"
#include "State.h"
#include <windows.h>
#include <cmath>
#include <algorithm>
#include <cstring>
namespace worlddiff::app {
namespace {
bool ReadBytes(uintptr_t address,void* value,size_t size){SIZE_T got=0;return address>=0x10000&&address<0x00007FFFFFFEFFFFull&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),value,size,&got)&&got==size;}
template<class T> bool Read(uintptr_t address,T& value){
 if(address<0x10000||address>0x00007FFFFFFEFFFFull)return false;
 SIZE_T got=0;return ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<const void*>(address),&value,sizeof(value),&got)&&got==sizeof(value);
}
}
OutfitSnapshot InspectPlayerOutfit(uintptr_t entity,MemoryReader read){
 OutfitSnapshot out;out.entity=entity;if(!entity||!read)return out;
 uintptr_t armor=0,body=0;if(!read(entity+0xC928,&armor,8)||!armor||!read(armor,&body,8)||body!=entity)return out;
 constexpr uintptr_t offsets[]={0x300,0,0x80,0x100,0x180};
 constexpr const char* parts[]={"helm","body","arm","wst","leg"};
 for(size_t i=0;i<5;i++){
  uintptr_t ctx=0,resource=0;if(!read(armor+offsets[i],&ctx,8)||!ctx||!read(ctx+0x2A0,&resource,8)||!resource)continue;
  char path[256]{};if(!read(resource+0x0C,path,255)||!memchr(path,0,255))continue;
  auto normalized=Normalize(path);if(normalized.rfind("pl\\f_equip\\",0)!=0&&normalized.rfind("pl\\m_equip\\",0)!=0)continue;
  if(Equip(normalized).empty()||ProfilePart(normalized)!=parts[i]||normalized.find("\\mod\\")==std::string::npos)continue;
  out.contexts[i]=ctx;out.resources[i]=resource;out.paths[i]=normalized;
 }
 uintptr_t finalArmor=0,finalBody=0;if(!read(entity+0xC928,&finalArmor,8)||finalArmor!=armor||!read(armor,&finalBody,8)||finalBody!=entity)return {};
 out.ready=std::any_of(out.paths.begin(),out.paths.end(),[](auto& p){return !p.empty();});return out;
}
OutfitSnapshot InspectSceneOutfit(uintptr_t base,MemoryReader read){
 if(!base||!read)return {};
 auto pointer=[&](uintptr_t address){uintptr_t value=0;return read(address,&value,sizeof(value))&&value>=0x10000&&value<0x00007FFFFFFEFFFFull?value:uintptr_t{0};};
 auto manager=pointer(base+0x50139A0);auto entity=manager?pointer(manager+0x50):0;
 if(entity){
  // Normal gameplay never consults the title GUI or cached preview avatar.
  auto out=InspectPlayerOutfit(entity,read);if(pointer(base+0x50139A0)!=manager||pointer(manager+0x50)!=entity)return {};return out;
 }
 // Steam 15.23.00: cached save avatars survive returning to the title menu.
 // Require the actual uGUITitleDataSelect visibility flag, not just its pointer.
 auto gui=pointer(base+0x51C4640);auto table=gui?pointer(gui+0x278):0;uint32_t count=0,capacity=0;
 if(!table||!read(gui+0x268,&count,4)||!read(gui+0x26C,&capacity,4)||!count||count>capacity||capacity>4096)return {};
 // GUI registration order changes between title and gameplay. Search the
 // bounded live unit array by exact class instead of assuming a fixed slot.
 std::vector<uintptr_t> windows(count);if(!read(table,windows.data(),windows.size()*sizeof(uintptr_t)))return {};
 uintptr_t window=0;size_t windowIndex=0;
 for(size_t i=0;i<windows.size();i++){uint32_t flags=0;auto candidate=windows[i];if(candidate>=0x10000&&pointer(candidate)==base+0x3487740&&read(candidate+0x14,&flags,4)&&(flags&0x10)){window=candidate;windowIndex=i;break;}}
 if(!window)return {};
 auto players=pointer(base+0x500ECA0);if(!players||pointer(players)!=base+0x33FE5B0)return {};
 auto avatar=pointer(players+0x58);auto out=InspectPlayerOutfit(avatar,read);if(!out.ready)return {};
 uint32_t finalFlags=0,finalCount=0,finalCapacity=0;auto finalManager=pointer(base+0x50139A0);
 if(finalManager!=manager||(manager&&pointer(manager+0x50))||pointer(base+0x51C4640)!=gui||pointer(gui+0x278)!=table||!read(gui+0x268,&finalCount,4)||!read(gui+0x26C,&finalCapacity,4)||finalCount!=count||finalCapacity!=capacity||pointer(table+windowIndex*8)!=window||pointer(window)!=base+0x3487740||!read(window+0x14,&finalFlags,sizeof(finalFlags))||!(finalFlags&0x10)||pointer(base+0x500ECA0)!=players||pointer(players+0x58)!=avatar)return {};
 out.savePreview=true;return out;
}
OutfitSnapshot ReadPlayerOutfit(){return InspectSceneOutfit(reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)),ReadBytes);}
bool InspectTextInputActive(uintptr_t base,MemoryReader read){
 if(!base||!read)return true;auto pointer=[&](uintptr_t a){uintptr_t v=0;return read(a,&v,8)&&v>=0x10000&&v<0x00007FFFFFFEFFFFull?v:uintptr_t{0};};
 auto gui=pointer(base+0x51C4640),table=gui?pointer(gui+0x278):0;uint32_t count=0,capacity=0;
 if(!table||!read(gui+0x268,&count,4)||!read(gui+0x26c,&capacity,4)||count>capacity||!count||capacity>4096)return true;
 std::vector<uintptr_t> units(count);if(!read(table,units.data(),units.size()*8))return true;
 for(auto unit:units){uint32_t flags=0;if(!unit||!read(unit+0x14,&flags,4)||!(flags&0x10))continue;
  auto vt=pointer(unit),method=vt?pointer(vt+4*8):0;unsigned char code[8]{};
  // MT Framework's getDTI virtual method returns its class descriptor.
  if(!method||!read(method,code,8)||code[0]!=0x48||code[1]!=0x8d||code[2]!=0x05||code[7]!=0xc3)continue;
  int32_t displacement=0;memcpy(&displacement,code+3,4);auto dti=method+7+displacement;auto text=pointer(dti+8);char name[96]{};
  if(!text||!read(text,name,sizeof(name))||!memchr(name,0,sizeof(name)))continue;
  std::string_view type(name);
  if(type=="uGUIChat"){
   // Steam 15.23.00: the root stays active even with no chat UI. Its +0x380
   // bits also retain history; only +0x494 returned 0/1/0 in paired captures.
   uint8_t opened=0;if(!read(unit+0x494,&opened,1)||opened)return true;
  }else if(type.starts_with("uGUIKeyboard")||type.starts_with("uGUIKeyBoard"))return true;
 }
 uint32_t finalCount=0;return pointer(base+0x51C4640)!=gui||pointer(gui+0x278)!=table||!read(gui+0x268,&finalCount,4)||finalCount!=count;
}
bool GameTextInputActive(){return InspectTextInputActive(reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)),ReadBytes);}
std::vector<PlayerInstance> InspectPlayers(uintptr_t base,const OutfitSnapshot& local,MemoryReader read){
 std::vector<PlayerInstance> out;if(!base||!read||!local.ready||local.savePreview)return out;
 auto pointer=[&](uintptr_t a){uintptr_t v=0;return read(a,&v,8)&&v>=0x10000&&v<0x00007FFFFFFEFFFFull?v:uintptr_t{0};};
 auto manager=pointer(base+0x50139A0);if(!manager||pointer(manager+0x50)!=local.entity)return out;
 auto container=pointer(base+0x500ECA0);if(!container||pointer(container)!=base+0x33FE5B0)return out;
 std::set<uintptr_t> seen;
 for(size_t i=0;i<16;i++){
  auto record=container+0x40+i*0x740;if(pointer(record)!=base+0x33FE588)continue;
  uintptr_t token=0;if(!read(record+8,&token,8))continue;auto actor=pointer(record+0x18);
  if(!actor||pointer(actor)!=base+0x34A6D18||!seen.insert(actor).second)continue;
  auto outfit=InspectPlayerOutfit(actor,read);uintptr_t finalToken=0;
  if(!outfit.ready||pointer(record+0x18)!=actor||!read(record+8,&finalToken,8)||finalToken!=token)continue;
  out.push_back({std::move(outfit),record,token,container,actor==local.entity});
 }
 if(pointer(base+0x500ECA0)!=container||pointer(container)!=base+0x33FE5B0||pointer(base+0x50139A0)!=manager||pointer(manager+0x50)!=local.entity)return {};
 // No arbitrary first-player fallback if the explicit local entry is absent.
 if(std::none_of(out.begin(),out.end(),[](auto& p){return p.local;}))return {};
 return out;
}
std::vector<PlayerInstance> ReadPlayers(const OutfitSnapshot& local){return InspectPlayers(reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)),local,ReadBytes);}
bool ReadRuntimeModel(uintptr_t resource,const std::string& path,Model& model){
 char resourcePath[256]{};if(!ReadBytes(resource+0xC,resourcePath,255)||!memchr(resourcePath,0,255)||Normalize(resourcePath)!=Normalize(path))return false;
 uintptr_t meshes=0,names=0;uint32_t count=0,materials=0;
 if(!Read(resource+0xC8,meshes)||!Read(resource+0xD0,count)||!Read(resource+0x400,names)||!Read(resource+0xE4,materials)||!meshes||!names||!count||count>65535||!materials||materials>65535)return false;
 std::vector<char> text((size_t)materials*128);std::vector<std::array<uint8_t,80>> rows(count);
 if(!ReadBytes(names,text.data(),text.size())||!ReadBytes(meshes,rows.data(),rows.size()*80))return false;
 Model result;result.relativePath=Normalize(path);result.equip=Equip(path);result.part=ProfilePart(path);result.display=path.substr(path.find_last_of('\\')+1);
 for(size_t i=0;i<materials;i++){auto first=text.data()+i*128;auto end=(const char*)memchr(first,0,128);if(!end)return false;Material material;material.name.assign(first,(size_t)(end-first));material.hash=MaterialNameHash(material.name);result.materials.push_back(std::move(material));}
 for(uint32_t i=0;i<count;i++){Mesh mesh;mesh.index=i;mesh.raw=rows[i];memcpy(&mesh.vertexCount,rows[i].data()+2,2);memcpy(&mesh.groupId,rows[i].data()+4,2);memcpy(&mesh.materialId,rows[i].data()+6,2);memcpy(&mesh.lod,rows[i].data()+8,2);memcpy(&mesh.blockType,rows[i].data()+0x18,4);memcpy(&mesh.indexCount,rows[i].data()+0x20,4);if(mesh.materialId>=materials)return false;result.meshes.push_back(mesh);}
 uintptr_t finalMeshes=0,finalNames=0;uint32_t finalCount=0,finalMaterials=0;char finalPath[256]{};
 if(!Read(resource+0xC8,finalMeshes)||!Read(resource+0x400,finalNames)||!Read(resource+0xD0,finalCount)||!Read(resource+0xE4,finalMaterials)||!ReadBytes(resource+0xC,finalPath,255)||memcmp(resourcePath,finalPath,255)||finalMeshes!=meshes||finalNames!=names||finalCount!=count||finalMaterials!=materials)return false;
 result.signature=MeshOrderSignature(result);model=std::move(result);return true;
}
PlayerSnapshot InspectPlayerSnapshot(uintptr_t base,uintptr_t entity,MemoryReader reader,bool remote){
 PlayerSnapshot p;if(!base||!entity||!reader)return p;p.entity=entity;
 auto read=[&](uintptr_t a,auto& v){return reader(a,&v,sizeof(v));};
 auto Pointer=[&](uintptr_t a){uintptr_t v=0;return read(a,v)&&v>=0x10000&&v<0x00007FFFFFFEFFFFull?v:uintptr_t{0};};
 auto Integer=[&](uintptr_t a,int lo,int hi){int v=-1;return read(a,v)&&v>=lo&&v<=hi?v:-1;};
 auto Flag=[&](uintptr_t a){uint8_t v=0;return read(a,v)&&v<=1?v:-1;};
 auto TimerFlag=[&](uintptr_t a){float v=0;return read(a,v)?TimerState(v):-1;};
 // Steam 15.23.00. Pointer chains are read-only and derived from the local
 // MHWI-VisualControllerExtended 1.4.0 reference; no additional hooks or writes.
 auto map=Pointer(base+0x500CDA0);if(map)map=Pointer(map+0x80);if(map)map=Pointer(map+0xEEC0);
 uint16_t mapId=0;if(map&&read(map+0x118,mapId))p.map=mapId;
 auto weapon=Pointer(p.entity+0xC0);if(weapon)weapon=Pointer(weapon+0x8);if(weapon)weapon=Pointer(weapon+0x78);
 if(weapon)p.weapon=Integer(weapon+0x2E8,0,13);
 auto state=Pointer(p.entity+0x76B0);
 if(state){if(p.weapon==3)p.spirit=Integer(state+0x2370,0,3);if(p.weapon==2){p.demon=Flag(state+0x2368);p.archdemon=Flag(state+0x2369);}}
 if(state&&p.weapon==10){bool valid=read(state+0x2368,p.extractTimers[0])&&read(state+0x236C,p.extractTimers[1])&&read(state+0x2370,p.extractTimers[2]);if(valid)p.extracts=ExtractMask(p.extractTimers);}
 // Verified against read-only 50 Hz captures of charging, release, morphing,
 // red shield/sword/axe and amped sword. 0x23A0 retains the last bow attack's
 // charge; use 0x23B4 instead. Preserve the 109 storage values 1..4, displayed
 // as uncharged / first / second / third charge in the panel.
 if(state&&p.weapon==11){int level=Integer(state+0x23B4,0,3);if(level>=0)p.bowCharge=level+1;}
 if(state&&p.weapon==4)p.hammerCharge=Integer(state+0x235C,0,3);
 if(state&&p.weapon==9)p.chargeBladeMode=Flag(state+0x20E2);
 if(state&&p.weapon==8)p.switchAxeMode=Flag(state+0x20E2);
 if(state&&p.weapon==9){int shield=TimerFlag(state+0x2378),sword=TimerFlag(state+0x237C),axe=TimerFlag(state+0x2470);if(shield>=0&&sword>=0&&axe>=0)p.chargeBladeBuffs=shield|(sword<<1)|(axe<<2);}
 if(state&&p.weapon==8){
  p.switchAxeAmped=TimerFlag(state+0x2364);
  // The power-axe buff lives on the abnormality object, not mechanics. Its
  // timer can retain a stale positive value when the active flag is false.
  auto abnormal=Pointer(p.entity+0x7D20);if(abnormal){int active=Flag(abnormal+0x6E5);if(active==0)p.switchAxePowered=0;else if(active==1)p.switchAxePowered=TimerFlag(abnormal+0x6E8);}
 }
 auto action=Pointer(p.entity+0x468);if(action)p.lmt=Integer(action+0xE9C4,0,0x7fffffff);
 // Remote motion streams can expose absent/sentinel values. They must not
 // accidentally satisfy a sheathed/drawn or custom action condition.
 if(remote&&(p.lmt==0||p.lmt==65535))p.lmt=-1;
 auto vitality=Pointer(p.entity+0x7630);float health=-1,max=-1;
 if(vitality&&read(vitality+0x64,health)&&read(vitality+0x60,max)&&std::isfinite(health)&&std::isfinite(max)&&health>=0&&max>0&&max<100000){
  p.health=health;p.maxHealth=max;p.healthPercent=std::clamp(health*100.f/max,0.f,100.f);
 }
 return p;
}
PlayerSnapshot ReadActorSnapshot(uintptr_t entity,bool remote){return InspectPlayerSnapshot(reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr)),entity,ReadBytes,remote);}
PlayerSnapshot ReadPlayerSnapshot(){
 const auto base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));uintptr_t manager=0,entity=0;
 if(!Read(base+0x50139A0,manager)||!manager||!Read(manager+0x50,entity)||!entity)return {};
 return InspectPlayerSnapshot(base,entity,ReadBytes);
}
}
