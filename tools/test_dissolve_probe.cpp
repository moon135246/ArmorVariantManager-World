#include "../src/DissolveProbe.h"
#include "../src/Dx12SceneDraw.h"
#include "../src/IndexedHandoff.h"
#include "../src/NativeCoverage.h"
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace probe = worlddiff::dissolve_probe;

template<class T> void field(unsigned char* object, size_t offset, T value) {
 std::memcpy(object + offset, &value, sizeof(value));
}
static bool CheckNativeDrawIdentity() {
 using namespace worlddiff;
 using namespace worlddiff::dissolve;
 alignas(8) unsigned char model[0x300]{}, resource[0x120]{}, rows[160]{}, backend[0x160]{},
     worker[0x1a00]{}, packet[0x50]{}, material[0x10]{}, vs[0x10]{},
     constants[0xb0]{}, indexObject[0x68]{}, indexBuffer[0x48]{}, targets[0x50]{};
 unsigned binding = (3u << 24) | (51u << 12);
 auto* vsPointer = vs;
 const uintptr_t list = 0x1230;
 ModelPolicy p;
 p.local = true; p.actor = 0x100; p.part = 2; p.context = reinterpret_cast<uintptr_t>(model);
 p.resource = reinterpret_cast<uintptr_t>(resource); p.meshes = reinterpret_cast<uintptr_t>(rows);
 p.materials = {3,3};
 RenderPolicies policies;
 policies.localEntity = p.actor; policies.models.push_back(p);
 localModelContexts[2] = p.context;
 field(model, 0x2a0, resource); field(resource, 0xc8, rows); field(resource, 0xd0, 2u);
 field(resource, 0x108, indexObject); field(indexObject, 0x60, indexBuffer);
 field(indexBuffer, 0x40, UINT64{0x10000}); field(indexBuffer, 0x10, 64u);
 field(rows, 6, uint16_t{3}); field(rows, 0x20, 6u);
 field(rows+80, 6, uint16_t{3}); field(rows+80, 0x1c, 6u); field(rows+80, 0x20, 6u);
 field(rows+80, 0x24, -2);
 field(packet, 0, uint8_t{1}); field(packet, 0x10, targets); field(packet, 0x20, material);
 field(packet, 0x40, 6u); field(packet, 0x44, 6u); field(packet, 0x48, -2);
 field(targets, 0x48, uint8_t{6}); field(targets, 0x40, uintptr_t{0x200});
 field(backend, 0x158, packet); field(backend, 0x20, list);
 field(backend, 0x88, D3D12_INDEX_BUFFER_VIEW{0x10000,64,DXGI_FORMAT_R16_UINT});
 field(backend, 0x108, uintptr_t{0x300}); field(backend, 0x110, uintptr_t{0x400});
 field(material, 8, &vsPointer); field(vs, 0, &binding); field(vs, 8, uint16_t{1});
 field(worker, 0x1860+51*8, constants); field(constants, 160, model);
 SceneDraw hit{};
 auto read = [&] { return ReadSceneDraw(list, reinterpret_cast<uintptr_t>(backend),
     reinterpret_cast<uintptr_t>(worker), reinterpret_cast<uintptr_t>(packet), policies, hit); };
 if (!read() || hit.mesh != 1 || hit.draw.BaseVertexLocation != -2 || hit.draw.StartIndexLocation != 6 ||
     hit.draw.IndexCountPerInstance != 6 || hit.draw.InstanceCount != 1 || hit.draw.StartInstanceLocation ||
     hit.colors != 6 || hit.depthView != 0x200 || reinterpret_cast<uintptr_t>(hit.commands) != list) return false;
 field(targets, 0x48, uint8_t{0});
 if (!read() || hit.colors != 0) return false;
 field(constants, 160, uintptr_t{0x12345});
 if (read()) return false;
 field(constants, 160, model);
 field(rows, 0x1c, 6u); field(rows, 0x24, -2);
 if (read()) return false;
 field(rows, 0x1c, 0u); field(rows, 0x24, 0);
 field(indexBuffer, 0x40, UINT64{0x20000});
 if (read()) return false;
 field(indexBuffer, 0x40, UINT64{0x10000});
 binding += 1u << 12;
 if (read()) return false;
 binding -= 1u << 12;
 field(backend, 0x158, uintptr_t{});
 if (read()) return false;
 field(backend, 0x158, packet);
 field(backend, 0x20, list+8);
 if (read()) return false;
 field(backend, 0x20, list);
 localModelContexts[2] = 0;
 return !read();
}
static bool CheckIndexHandoff() {
 using namespace worlddiff::dissolve;
 std::array<unsigned char,80> diskRow{},liveRow{};
 for(unsigned count:{3u,300u,82134u})for(float phase:{0.f,.125f,.25f,.5f,.75f,.875f,1.f}){
  auto old=NativeCoverage(count,phase,true),incoming=NativeCoverage(count,1.f-phase,false);
  if(old.indices+incoming.indices!=count)return false;
  std::vector<uint8_t> uses(count/3);
  for(const auto& set:{old,incoming})for(unsigned i=0;i<set.used;++i){
   const auto s=set.ranges[i];
   if(s.first%3 || s.count%3 || s.first+s.count>count)return false;
   for(unsigned j=s.first/3;j<(s.first+s.count)/3;++j)++uses[j];
  }
  if(std::any_of(uses.begin(),uses.end(),[](auto n){return n!=1;}))return false;
 }
 diskRow[14]=liveRow[14]=36;
 field(diskRow.data(),24,0x3c730760u);field(liveRow.data(),24,6u);
 if(!MatchesHandoffRow(liveRow.data(),diskRow.data()))return false;
 liveRow[32]=1;
 if(MatchesHandoffRow(liveRow.data(),diskRow.data()))return false;
 liveRow[32]=0;liveRow[24]=8;
 if(MatchesHandoffRow(liveRow.data(),diskRow.data()))return false;
 std::vector<uint8_t> bytes(1024);
 auto* data=bytes.data();
 std::memcpy(data,"MOD\0\xed\0",6);
 field(data,8,uint16_t{2});
 field(data,0x48,uint64_t{320}); field(data,0x50,uint64_t{480}); field(data,0x58,uint64_t{800});
 for(unsigned side=0;side<2;++side) {
  auto* row=data+320+80*side;
  field(row,2,uint16_t{6}); field(row,8,uint16_t{1}); field(row,14,uint8_t{12});
  field(row,16,2u+side); field(row,20,side*100u); field(row,28,side*6u);
  field(row,32,6u); field(row,36,-int(2+side));
  for(unsigned i=0;i<6;++i) {
   field(data+480+side*100+12*i,4,i<3?10.f:0.f);
   field(data+800+12*side,2*i,uint16_t(i+2+side));
  }
 }
 IndexedHandoff h;
 if(!DecodeHandoff(bytes,{0,1},h) || h.indices!=std::vector<uint16_t>{5,6,7,2,3,4,6,7,8,3,4,5} ||
    h.Cut(0)!=0 || h.Cut(.5f)!=3 || h.Cut(1)!=6 || h.ShapeCut(.5f)!=3 || h.base[1]!=-3) return false;
 field(data+580,0,3.f);
 if(!DecodeHandoff(bytes,{0,1},h) || h.ShapeCut(0)!=0 || h.ShapeCut(.5f)!=6 || h.ShapeCut(1)!=6) return false;
 PairSweep sweep;
 sweep.Set(false,0,900,true); sweep.Set(true,10,900,false);
 if(sweep.At(10)!=0.f || sweep.At(460)!=.5f || sweep.At(910)!=1.f)return false;
 sweep.Set(false,460,900,false);
 if(sweep.At(460)!=.5f || sweep.At(685)!=.25f || sweep.At(910)!=0.f)return false;
 sweep.Set(true,1000,900,true);
 if(sweep.At(1000)!=1.f)return false;
 if(DecodeHandoff(bytes,{1,1},h) || DecodeHandoff(bytes,{1,2},h)) return false;
 field(data+812,0,uint16_t{4});
 if(DecodeHandoff(bytes,{0,1},h)) return false;
 field(data+812,0,uint16_t{3}); field(data+480,4,std::numeric_limits<float>::quiet_NaN());
 if(DecodeHandoff(bytes,{0,1},h)) return false;
 return !DecodeHandoff(std::span<const uint8_t>(bytes).first(799),{0,1},h);
}
int wmain(int argc, wchar_t** argv) {
 if(argc==5 && std::wstring_view(argv[1])==L"--asset") {
  worlddiff::dissolve::IndexedHandoff pair;
  if(!worlddiff::dissolve::LoadHandoff(argv[2],{unsigned(std::stoul(argv[3])),unsigned(std::stoul(argv[4]))},pair))return 12;
  unsigned previous=0;
  for(unsigned i=0;i<=1000;++i) {
   const auto cut=pair.ShapeCut(float(i)/1000.f);
   if(cut<previous || cut%3 || cut/3>pair.thresholds.size() || pair.insideQuad[cut/3])return 13;
   previous=cut;
  }
  if(previous/3!=pair.thresholds.size())return 14;
  std::ifstream model(argv[2],std::ios::binary);
  uint64_t indices=0;
  model.seekg(0x58);model.read(reinterpret_cast<char*>(&indices),sizeof(indices));
  using Triangle=std::array<uint16_t,3>;
  for(unsigned s=0;s<2;++s) {
   std::vector<Triangle> original(pair.thresholds.size()),sorted(original.size());
   const auto bytes=original.size()*sizeof(Triangle);
   model.seekg(indices+2ull*pair.first[s]);
   if(!model.read(reinterpret_cast<char*>(original.data()),bytes))return 15;
   std::memcpy(sorted.data(),pair.indices.data()+3*s*sorted.size(),bytes);
   std::sort(original.begin(),original.end());std::sort(sorted.begin(),sorted.end());
   if(original!=sorted)return 16;
  }
  std::cout<<"PASS original material geometry: triangles="<<pair.thresholds.size()
           <<" flipped quads="<<std::count(pair.insideQuad.begin(),pair.insideQuad.end(),1)<<'\n';
  return 0;
 }
 if (!CheckIndexHandoff()) return 11;
 std::cout << "PASS complementary triangle order, shifted indices, signed base and invalid pair rejection\n";
 if (!CheckNativeDrawIdentity()) return 10;
 std::cout << "PASS native indexed draw identity, argument preservation and rejection cases\n";
 if (argc != 2) return 2;
 const std::filesystem::path folder(argv[1]);
 if (!std::filesystem::is_empty(folder)) return 2;
 alignas(8) unsigned char model[0x320]{}, draw[0x80]{}, mesh[80]{};
 const auto actor = reinterpret_cast<uintptr_t>(model);
 const auto packet = reinterpret_cast<uintptr_t>(draw);
 const auto row = reinterpret_cast<uintptr_t>(mesh);
 std::ofstream(folder / L"dissolve-probe.once").close();
 probe::Tick(folder);
 if (!probe::active || std::filesystem::exists(folder / L"dissolve-probe.once")) return 3;
 field(model, 0x314, 1.0f); field(draw, 0x64, 0.625f);
 field(draw, 0x30, row); field(draw, 0x70, 2u);
 field(mesh,0x1c,26706u);field(mesh,0x20,20238u);field(mesh,0x24,0);
 worlddiff::ModelPolicy owner;owner.context=actor;owner.local=true;owner.part=2;owner.meshes=row-160;
 owner.materials={0,0,4};owner.hidden={0,0,1};
 auto snapshot=std::make_shared<worlddiff::RenderPolicies>();snapshot->tick=GetTickCount();
 owner.transitions.push_back(worlddiff::dissolve::Retarget(2,snapshot->tick-200,0,true,false));
 snapshot->models.push_back(owner);worlddiff::renderPolicies.store(snapshot);probe::NextFrame();
 probe::Observe(actor, row, packet, 2, 2, 4, true, true,&owner);
 probe::Observe(actor, row, packet, 2, 2, 4, true, true);
 probe::Observe(actor, row, packet, 2, 3, 4, true, false);
 probe::Observe(actor, row, 0, 2, 4, 4, true, false);
 if (probe::pending.size()!=3 || probe::pending[0].packetValue!=0.625f ||
     probe::pending[0].modelFade!=1.0f || !probe::pending[0].packetMeshMatches ||
     probe::pending[1].packetIndex==probe::pending[1].mesh || probe::pending[2].readable) return 4;
 auto& sample=probe::pending.front();
 if(sample.plannedHidden!=owner.transitions.front().At(sample.frameTick) ||
    sample.firstIndex!=26706 || sample.indexCount!=20238 || sample.baseVertex!=0 ||
    probe::pending[1].plannedHidden!=-1)return 8;
 owner.token++;
 probe::lastSample[2][2]=0;
 probe::Observe(actor,row,packet,2,2,4,true,true,&owner);
 if(probe::pending.back().plannedHidden!=-1)return 9;
 probe::Tick(folder);
 if (!probe::pending.empty() || !probe::active) return 5;
 probe::started -= 30000; // Single-threaded fixture for the worker deadline.
 probe::Tick(folder);
 if (probe::output || probe::active) return 6;
 probe::Observe(actor, row, packet, 2, 9, 4, true, true);
 if (!probe::pending.empty()) return 7;
 std::cout << "PASS capture trigger, identity, sampling, flush and shutdown\n";
}
