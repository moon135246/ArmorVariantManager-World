#include "../src/State.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace worlddiff;using namespace worlddiff::app;
int main(){try{
 unsigned checks=0;auto check=[&](bool pass,const char* reason){++checks;if(!pass)throw std::runtime_error(reason);};
 auto& s=Get();s.directory=std::filesystem::temp_directory_path()/(L"WorldDiff115Display_"+std::to_wstring(GetTickCount64()));std::filesystem::create_directories(s.directory);s.configPath=s.directory/L"view.cfg";
 Model model;model.equip="pl105_0000";model.part="arm";model.relativePath="pl\\f_equip\\pl105_0000\\arm\\mod\\f_arm105_0000";model.signature=123;
 auto a=MaterialNameHash("共同材质"),b=MaterialNameHash("第二材质");model.materials={{"共同材质",a,7},{"第二材质",b,2}};
 for(uint32_t i=0;i<3;i++){Mesh mesh;mesh.index=i;mesh.materialId=i<2?0:1;model.meshes.push_back(mesh);}s.models.push_back(model);Main(model.equip);
 auto indices=[](const Model& m){std::vector<uint32_t> result;for(auto* mesh:OrderedMeshes(m))result.push_back(mesh->index);return result;};
 check(indices(s.models[0])==std::vector<uint32_t>({2,0,1}),"MRL order with shared material keeps both actual meshes");
 auto visibility=[&]{std::vector<bool> result;for(auto& mesh:s.models[0].meshes)result.push_back(Hidden(model.relativePath,mesh.index,s.models[0].materials[mesh.materialId].hash));return result;};
 SetVisible(Main(model.equip),{model.relativePath,1,true},false);auto before=visibility();
 check(SetMeshNote(s.models[0],1,"左侧饰品"),"Unicode note assigned");check(!MeshView(s.models[0],0),"shared material does not copy note to another mesh");
 check(MoveMeshBefore(s.models[0],1,2),"manual insertion before first mesh");check(indices(s.models[0])==std::vector<uint32_t>({1,2,0}),"manual order is explicit insertion, not swap");
 check(visibility()==before,"order and note never change visibility targets");check(Save(),"save new config and portable profile");s.data={};check(Load(),"reload new config");
 check(MeshView(s.models[0],1)&&MeshView(s.models[0],1)->note=="左侧饰品","note survives restart");check(indices(s.models[0])==std::vector<uint32_t>({1,2,0}),"manual order survives restart");
 check(MoveMeshBefore(s.models[0],1,UINT32_MAX),"manual move to last position");check(indices(s.models[0])==std::vector<uint32_t>({2,0,1}),"drop below last row supported");
 ResetMeshOrder(s.models[0]);check(indices(s.models[0])==std::vector<uint32_t>({2,0,1}),"reset returns to MRL record order");check(MeshView(s.models[0],1)->note=="左侧饰品","reset preserves notes");
 check(SetMeshNote(s.models[0],1,{}),"clear note");check(s.data.meshDisplay.empty(),"reset and clear remove empty presentation records");
 check(!SetMeshNote(s.models[0],1,std::string(161,'x'))&&!SetMeshNote(s.models[0],1,"bad\nline"),"reject excessive or control-character notes");
 check(!MoveMeshBefore(s.models[0],999,0)&&!MoveMeshBefore(s.models[0],0,999),"unknown mesh IDs cannot create ordering entries");
 SetMeshNote(s.models[0],1,"共享材质的第二网格");MoveMeshBefore(s.models[0],1,2);s.models[0].signature++;
 check(!MeshView(s.models[0],1)&&indices(s.models[0])==std::vector<uint32_t>({2,0,1}),"changed model signature disables stale presentation");s.models[0].signature--;
 s.models[0].materials[0].hash++;check(!MeshView(s.models[0],1),"material identity also guards notes");s.models[0].materials[0].hash--;
 s.models[0].signature=0;check(!MeshView(s.models[0],1),"unverified live model cannot claim a saved disk signature");s.models[0].signature=123;
 Model target=s.models[0];target.equip="pl106_0000";target.relativePath="pl\\f_equip\\pl106_0000\\arm\\mod\\f_arm106_0000";target.signature=456;s.models.push_back(target);
 check(ExportProfile(model.equip,"显示布局测试"),"export manual layout with profile");check(ApplyProfile(target.equip,"显示布局测试.wdp"),"apply same MOD to different equipment");
 check(MeshView(s.models[1],1)&&MeshView(s.models[1],1)->note=="共享材质的第二网格","portable notes remap by individual mesh");check(indices(s.models[1])==std::vector<uint32_t>({1,2,0}),"portable manual order remaps to target equipment");
 for(auto& mesh:s.models[1].meshes)mesh.index+=100;s.models[1].signature++;
 check(ApplyProfile(target.equip,"显示布局测试.wdp"),"profile remaps after export changes mesh indices");check(MeshView(s.models[1],101)&&MeshView(s.models[1],101)->note=="共享材质的第二网格","note follows original geometry/material binding");
 check(indices(s.models[1])==std::vector<uint32_t>({101,102,100}),"manual order follows remapped mesh IDs");check(Save(),"persist both equipment layouts");
 s.data={};check(Load()&&s.data.meshDisplay.size()==6,"all per-mesh presentation records round trip through separate equipment files");
 for(auto* text:{"WorldDiffInternal 115\nSET 1 1 0 120 1 1\nMESHVIEW \"pl\\f_equip\\pl105_0000\\arm\\mod\\x\" 0 1 2 -2 \"bad\"\n",
                  "WorldDiffInternal 115\nSET 1 1 0 120 1 1\nMESHVIEW \"unscoped\" 0 1 2 0 \"bad\"\n"}){std::istringstream bad(text);Document ignored;check(!ReadDocument(bad,ignored),"malformed metadata rejected atomically");}
 std::istringstream old("WorldDiffInternal 110\nSET 1 1 0 120 1 1\n");Document legacy;check(ReadDocument(old,legacy)&&legacy.meshDisplay.empty(),"existing 110 config remains readable");
 Model fallback=model;for(auto& material:fallback.materials)material.mrlIndex=-1;fallback.signature=999;check(indices(fallback)==std::vector<uint32_t>({0,1,2}),"missing MRL falls back to material slots and mesh index");
 std::cout<<"PASS "<<checks<<" mesh display checks: material order, notes, persistence, shared material, portable remapping and identity guards.\n";return 0;
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
