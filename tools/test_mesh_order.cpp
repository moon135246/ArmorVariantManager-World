#include "../src/ModelCatalog.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <algorithm>
using namespace worlddiff;
int main(int argc,char** argv){try{
 if(argc!=2)throw std::runtime_error("pass isolated fixture root");std::filesystem::path root=argv[1];unsigned checks=0;
 auto check=[&](bool pass,const char* why){++checks;if(!pass)throw std::runtime_error(why);};
 auto models=LoadCatalog(root);unsigned matched=0;
 for(auto& model:models)if(model.equip=="pl105_0000"&&!model.materialOrder.empty()){
  matched++;for(auto& mesh:model.meshes){check(mesh.index<model.meshes.size(),"original mesh index valid");check(model.materials[mesh.materialId].mrlIndex>=0,"actual MRL hash maps to material");}
 }
 check(matched==4,"all four actual MRL tables loaded without Blender");
 auto arm=std::find_if(models.begin(),models.end(),[](auto& m){return m.equip=="pl105_0000"&&m.part=="arm";});check(arm!=models.end(),"actual arm model present");
 const unsigned expected[]={1,3,4,5,9,10,6,11,12,2,7,8,0,13,14,15};
 for(unsigned i=0;i<16;i++)check(arm->materials[arm->meshes[expected[i]].materialId].mrlIndex==(int)i,"MRL records exactly match Blender material objects 00..15");
 auto orderPath=root/L"nativePC/pl/f_equip/pl105_0000/arm/mod/f_arm105_0000.meshorder";
 {std::ofstream old(orderPath);old<<"WorldDiffMeshOrder 1\nSIGNATURE 0\nMESH 0 \"stale\" \"stale\"\n";}
 std::vector<std::string> diagnostics;auto unchanged=LoadCatalog(root,&diagnostics);
 auto current=std::find_if(unchanged.begin(),unchanged.end(),[](auto& m){return m.equip=="pl105_0000"&&m.part=="arm";});
 check(current!=unchanged.end()&&current->materialOrder==arm->materialOrder,"obsolete sidecar cannot override real material order");
 Model runtime;runtime.relativePath=arm->relativePath;LoadMaterialOrder(runtime,root);check(runtime.materialOrder==arm->materialOrder,"MRL order can be read before live material names arrive");
 auto mrl=root/L"nativePC/pl/f_equip/pl105_0000/arm/mod/f_arm105_0000.mrl3";
 std::ifstream input(mrl,std::ios::binary);std::string original((std::istreambuf_iterator<char>(input)),{});input.close();
 {std::ofstream bad(mrl,std::ios::binary|std::ios::trunc);bad<<"MRL";}
 diagnostics.clear();auto fallback=LoadCatalog(root,&diagnostics);
 for(auto& model:fallback)if(model.equip=="pl105_0000"&&model.part=="arm"){
  check(model.materialOrder.empty(),"malformed MRL does not remove MOD3");for(auto& mesh:model.meshes)check(model.materials[mesh.materialId].mrlIndex==-1,"invalid MRL uses material-slot fallback");
 }
 check(!diagnostics.empty(),"invalid MRL is diagnosed");{std::ofstream restore(mrl,std::ios::binary|std::ios::trunc);restore<<original;}
 std::cout<<"PASS "<<checks<<" real MOD3/MRL material order and fallback checks.\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}