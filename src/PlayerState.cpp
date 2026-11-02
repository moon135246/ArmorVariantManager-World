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
uintptr_t Pointer(uintptr_t address){uintptr_t value=0;return Read(address,value)&&value>=0x10000?value:0;}
int Integer(uintptr_t address,int minimum,int maximum){int value=-1;return Read(address,value)&&value>=minimum&&value<=maximum?value:-1;}
int Flag(uintptr_t address){uint8_t value=0;return Read(address,value)&&value<=1?value:-1;}
int TimerFlag(uintptr_t address){float seconds=0;return Read(address,seconds)?TimerState(seconds):-1;}
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
OutfitSnapshot ReadPlayerOutfit(){
 auto base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));auto manager=Pointer(base+0x50139A0);auto entity=manager?Pointer(manager+0x50):0;
 auto out=InspectPlayerOutfit(entity,ReadBytes);if(manager&&Pointer(manager+0x50)!=entity)return {};return out;
}
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
 auto vitality=Pointer(p.entity+0x7630);float health=-1,max=-1;
 if(vitality&&Read(vitality+0x64,health)&&Read(vitality+0x60,max)&&std::isfinite(health)&&std::isfinite(max)&&health>=0&&max>0&&max<100000){
  p.health=health;p.maxHealth=max;p.healthPercent=std::clamp(health*100.f/max,0.f,100.f);
 }
 return p;
}
}
