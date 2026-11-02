#include "../src/State.h"
#include <psapi.h>
#include <fstream>
#include <iostream>
#include <cstring>
#include <map>
#include <functional>
#include <stdexcept>
using namespace worlddiff;using namespace worlddiff::app;
namespace {
HANDLE process=GetCurrentProcess();
std::map<uintptr_t,uintptr_t> globals;std::vector<uintptr_t> reads;std::function<void(uintptr_t)> duringRead;
bool Memory(uintptr_t address,void* destination,size_t size){
 reads.push_back(address);if(duringRead)duringRead(address);
 if(auto at=globals.find(address);at!=globals.end()&&size==8){memcpy(destination,&at->second,8);return true;}
 SIZE_T got=0;return ReadProcessMemory(process,(const void*)address,destination,size,&got)&&got==size;
}
void Ptr(std::vector<uint8_t>& b,size_t offset,uintptr_t value){memcpy(b.data()+offset,&value,8);}
struct Avatar {
 std::vector<uint8_t> entity=std::vector<uint8_t>(0xD000),armor=std::vector<uint8_t>(0x380);
 std::array<std::vector<uint8_t>,5> contexts,resources;std::array<std::string,5> paths;
 Avatar(){constexpr size_t offsets[]={0x300,0,0x80,0x100,0x180};const char* parts[]={"helm","body","arm","wst","leg"};Ptr(entity,0xC928,(uintptr_t)armor.data());
  for(size_t i=0;i<5;i++){contexts[i].resize(0x400);resources[i].resize(0x400);auto context=i==1?(uintptr_t)entity.data():(uintptr_t)contexts[i].data();Ptr(armor,offsets[i],context);if(i==1)Ptr(entity,0x2A0,(uintptr_t)resources[i].data());else Ptr(contexts[i],0x2A0,(uintptr_t)resources[i].data());paths[i]="pl\\f_equip\\pl105_0000\\"+std::string(parts[i])+"\\mod\\f_"+parts[i]+"105_0000";memcpy(resources[i].data()+0xC,paths[i].c_str(),paths[i].size()+1);}
 }
 uintptr_t address()const{return (uintptr_t)entity.data();}
};
struct Scene {
 static constexpr uintptr_t base=0x1000000000ull;
 std::vector<uint8_t> manager=std::vector<uint8_t>(0x100),players=std::vector<uint8_t>(0x100),gui=std::vector<uint8_t>(0x300),table=std::vector<uint8_t>(0x200),window=std::vector<uint8_t>(0x100);
 Avatar preview,player;
 void Show(bool show){uint32_t flags=show?0x13:3;memcpy(window.data()+0x14,&flags,4);}
 Scene(){globals.clear();duringRead={};globals[base+0x50139A0]=(uintptr_t)manager.data();globals[base+0x500ECA0]=(uintptr_t)players.data();globals[base+0x51C4640]=(uintptr_t)gui.data();Ptr(players,0,base+0x33FE5B0);Ptr(players,0x58,preview.address());Ptr(gui,0x268,0x4000000020ull);Ptr(gui,0x278,(uintptr_t)table.data());Ptr(table,0xA8,(uintptr_t)window.data());Ptr(window,0,base+0x3487740);Show(true);}
 OutfitSnapshot Read(){reads.clear();return InspectSceneOutfit(base,Memory);}
};
}
int main(int argc,char** argv){try{
 if(argc>=3&&std::string(argv[1])=="--pid"){
  auto pid=(DWORD)std::stoul(argv[2]);process=OpenProcess(PROCESS_VM_READ|PROCESS_QUERY_INFORMATION,FALSE,pid);if(!process)throw std::runtime_error("cannot read game process");HMODULE modules[1024]{};DWORD needed=0;if(!K32EnumProcessModules(process,modules,sizeof(modules),&needed))throw std::runtime_error("cannot find game module");auto out=InspectSceneOutfit((uintptr_t)modules[0],Memory);
  std::cout<<"LIVE ready="<<out.ready<<" savePreview="<<out.savePreview<<" entity="<<std::hex<<out.entity<<std::dec<<'\n';std::cout<<"LOOKUPS titleGui="<<std::count(reads.begin(),reads.end(),(uintptr_t)modules[0]+0x51C4640)<<" previewManager="<<std::count(reads.begin(),reads.end(),(uintptr_t)modules[0]+0x500ECA0)<<'\n';for(size_t i=0;i<5;i++)if(!out.paths[i].empty())std::cout<<"slot="<<i<<" context="<<std::hex<<out.contexts[i]<<std::dec<<" path="<<out.paths[i]<<'\n';
  if(out.ready&&argc>=5){auto& s=Get();s.directory=PathUtf8(argv[3]);s.configPath=s.directory/L"Settings.cfg";s.models=LoadCatalog(PathUtf8(argv[4]));s.trackingPlayer=true;s.outfit=out;if(!LoadSettings())throw std::runtime_error("copied settings unreadable");std::set<std::string> scopes;for(auto& path:out.paths)if(!path.empty())scopes.insert(Equip(path));for(auto& scope:scopes)if(!LoadEquipmentProfile(scope))throw std::runtime_error("copied equipment profile rejected");size_t presets=0;for(auto& g:s.data.groups)presets+=g.presets.size();std::cout<<"COPIED_PROFILE scopes="<<scopes.size()<<" groups="<<s.data.groups.size()<<" presets="<<presets<<" meshViews="<<s.data.meshDisplay.size()<<'\n';}
  CloseHandle(process);return 0;
 }
 unsigned checks=0;auto check=[&](bool good,const char* reason){checks++;if(!good)throw std::runtime_error(reason);};Scene s;
 auto save=s.Read();check(save.ready&&save.savePreview&&save.entity==s.preview.address()&&save.paths==s.preview.paths,"active save selection resolves all five preview parts");
 s.Show(false);auto title=s.Read();check(!title.ready&&!title.savePreview,"title menu rejects the still-cached preview actor");check(std::find(reads.begin(),reads.end(),Scene::base+0x500ECA0)==reads.end(),"inactive save window never inspects the preview player singleton");
 s.Show(true);check(s.Read().savePreview,"reopening save selection reuses its cached avatar safely");
 Ptr(s.table,0xA8,0);Ptr(s.table,0xC0,(uintptr_t)s.window.data());check(s.Read().savePreview,"GUI registration order changes do not break save selection");Ptr(s.table,0xC0,0);Ptr(s.table,0xA8,(uintptr_t)s.window.data());
 Ptr(s.gui,0x268,0x100100001001ull);check(!s.Read().ready,"unbounded GUI registry is rejected");Ptr(s.gui,0x268,0x1000000020ull);check(!s.Read().ready,"GUI count cannot exceed its capacity");Ptr(s.gui,0x268,0x4000000020ull);
 Ptr(s.manager,0x50,s.player.address());auto play=s.Read();check(play.ready&&!play.savePreview&&play.entity==s.player.address(),"normal local player takes priority over a visible save window");check(std::find(reads.begin(),reads.end(),Scene::base+0x51C4640)==reads.end()&&std::find(reads.begin(),reads.end(),Scene::base+0x500ECA0)==reads.end(),"normal gameplay does not read any save UI or preview reference");
 Ptr(s.player.armor,0,s.preview.address());check(!s.Read().ready&&std::find(reads.begin(),reads.end(),Scene::base+0x51C4640)==reads.end(),"invalid non-null gameplay actor cannot fall back to another character");Ptr(s.player.armor,0,s.player.address());Ptr(s.manager,0x50,0);
 Ptr(s.window,0,Scene::base+0x34870E8);check(!s.Read().ready,"other title GUI classes cannot enable save previews");Ptr(s.window,0,Scene::base+0x3487740);
 Ptr(s.players,0,Scene::base+0x33FE5B8);check(!s.Read().ready,"wrong player manager type is rejected");Ptr(s.players,0,Scene::base+0x33FE5B0);
 Ptr(s.players,0x58,0);check(!s.Read().ready,"empty save slot has no editable preview");Ptr(s.players,0x58,s.preview.address());
 Ptr(s.table,0xA8,0);check(!s.Read().ready,"missing save window is safe");Ptr(s.table,0xA8,(uintptr_t)s.window.data());
 Ptr(s.preview.armor,0,s.player.address());check(!s.Read().ready,"save avatar must own its body attachment");Ptr(s.preview.armor,0,s.preview.address());
 duringRead=[&](uintptr_t address){if(address==(uintptr_t)s.preview.resources[0].data()+0xC)s.Show(false);};check(!s.Read().ready,"closing save selection during inspection invalidates the snapshot");duringRead={};s.Show(true);
 duringRead=[&](uintptr_t address){if(address==(uintptr_t)s.preview.resources[0].data()+0xC)Ptr(s.manager,0x50,s.player.address());};check(!s.Read().ready,"gameplay appearing during preview inspection invalidates the snapshot");duringRead={};Ptr(s.manager,0x50,0);
 duringRead=[&](uintptr_t address){if(address==(uintptr_t)s.preview.resources[0].data()+0xC)Ptr(s.players,0x58,s.player.address());};check(!s.Read().ready,"switching save avatar during inspection cannot mix targets");duringRead={};Ptr(s.players,0x58,s.preview.address());
 duringRead=[&](uintptr_t address){if(address==(uintptr_t)s.preview.resources[0].data()+0xC)Ptr(s.gui,0x268,0x400000001Full);};check(!s.Read().ready,"GUI registration changes invalidate an in-flight preview snapshot");duringRead={};Ptr(s.gui,0x268,0x4000000020ull);
 check(s.Read().ready,"reader recovers after a scene transition");check(!InspectSceneOutfit(0,Memory).ready&&!InspectSceneOutfit(Scene::base,nullptr).ready,"invalid reader or module base is safe");
 std::cout<<"PASS "<<checks<<" scene checks: save preview, title rejection, gameplay priority, no gameplay preview reads and scene transition races\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
