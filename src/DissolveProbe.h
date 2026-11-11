#pragma once
// Optional read-only capture in a diagnostic build. Draw records are valid
// only inside the query callback. The worker writes copies of scalar values.
#include <windows.h>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <vector>
#include "RenderPolicy.h"

namespace worlddiff::dissolve_probe {
struct Sample {
 ULONGLONG time;
 uintptr_t instance;
 uint32_t part, mesh, material, packetIndex;
 bool original, resolved, readable, packetMeshMatches;
 float modelFade, packetValue;
 uint32_t frameTick=0,firstIndex=0,indexCount=0;
 int32_t baseVertex=0;
 float plannedHidden=-1;
};

struct Frame {
 std::shared_ptr<const RenderPolicies> policies;
 DWORD tick;
};
inline std::atomic<std::shared_ptr<const Frame>> frame;

inline bool ReadFields(uintptr_t model, uintptr_t mesh, uintptr_t draw, Sample& s) {
 if (!model || !mesh || !draw) return false;
 __try {
  s.modelFade = *reinterpret_cast<const float*>(model + 0x314);
  s.packetMeshMatches = (*reinterpret_cast<const uintptr_t*>(draw + 0x30) == mesh);
  s.packetIndex = *reinterpret_cast<const uint32_t*>(draw + 0x70);
  s.firstIndex = *reinterpret_cast<const uint32_t*>(mesh + 0x1c);
  s.indexCount = *reinterpret_cast<const uint32_t*>(mesh + 0x20);
  s.baseVertex = *reinterpret_cast<const int32_t*>(mesh + 0x24);
  // Candidate from the shared query caller. Its meaning on equipment draws
  // must be measured independently of its offset or numerical range.
  s.packetValue = *reinterpret_cast<const float*>(draw + 0x64);
  return true;
 } __except (EXCEPTION_EXECUTE_HANDLER) {
  return false;
 }
}

inline std::mutex mutex;
inline std::atomic<bool> active = false;
inline void NextFrame() {
 if (!active.load(std::memory_order_acquire)) return;
 auto policies=renderPolicies.load(std::memory_order_acquire);
 frame.store(std::make_shared<const Frame>(Frame{std::move(policies),GetTickCount()}),std::memory_order_release);
}
inline std::atomic<unsigned> dropped = 0;
inline std::vector<Sample> pending, batch;
inline std::array<std::array<ULONGLONG, 65536>, 5> lastSample{};
inline ULONGLONG started = 0;
inline unsigned captureSeconds=30;
inline FILE* output = nullptr;
constexpr size_t capacity = 8192;

inline void Observe(uintptr_t model, uintptr_t meshPtr, uintptr_t draw, uint32_t part,
                    uint32_t mesh, uint32_t material, bool original, bool resolved,const ModelPolicy* current=nullptr) {
 if (!active.load(std::memory_order_acquire) || part >= lastSample.size() || mesh >= 65536) return;
 std::unique_lock lock(mutex, std::try_to_lock);
 if (!lock.owns_lock()) { ++dropped; return; }
 if (!active.load(std::memory_order_relaxed)) return;
 const auto now = GetTickCount64();
 auto& previous = lastSample[part][mesh];
 if (previous && now - previous < 12) return;
 if (pending.size() == capacity) { ++dropped; return; }
 previous = now;
 Sample s{};
 s.time = now - started;
 s.instance = model;
 s.part = part;
 s.mesh = mesh;
 s.material = material;
 s.original = original;
 s.resolved = resolved;
 s.readable = ReadFields(model, meshPtr, draw, s);
 const auto submitted=frame.load(std::memory_order_acquire);
 if(s.readable && s.packetMeshMatches && s.packetIndex==mesh && submitted && submitted->policies &&
    submitted->policies->enabled && GetTickCount()-submitted->tick<1500) {
  auto& snapshot=*submitted->policies;
  auto* owner=snapshot.Find(model);
  if(submitted->tick-snapshot.tick<1500 && owner && current && SameDissolveInstance(*owner,*current) &&
     owner->part==part && mesh<owner->hidden.size() && mesh<owner->materials.size() &&
     owner->materials[mesh]==material && owner->meshes+80ull*mesh==meshPtr) {
   s.frameTick=submitted->tick;
   auto* move=owner->TransitionFor(mesh);
   s.plannedHidden=move?move->At(submitted->tick):static_cast<float>(owner->hidden[mesh]);
  }
 }
 pending.push_back(s);
}

inline void Tick(const std::filesystem::path& folder) {
 if (!output) {
  auto request = folder / L"dissolve-probe.once";
  std::error_code ec;
  if (!std::filesystem::is_regular_file(request, ec)) return;
  captureSeconds=30;
  {
   std::ifstream options(request);
   unsigned seconds{};
   if(options>>seconds && seconds>=1 && seconds<=90)captureSeconds=seconds;
  }
  const auto now = GetTickCount64();
  auto file = folder / (L"dissolve-probe-" + std::to_wstring(GetCurrentProcessId()) +
                        L"-" + std::to_wstring(now) + L".csv");
  if (_wfopen_s(&output, file.c_str(), L"wb") != 0) return;
  if (!std::filesystem::remove(request, ec)) {
   fclose(output); output = nullptr;
   std::filesystem::remove(file, ec);
   return;
  }
  fputs("ms,instance,part,mesh,material,original,resolved,readable,mesh_ref_matches,packet_index,model_314,packet_64,present_tick,planned_hidden,first_index,index_count,base_vertex\n",output);
  std::lock_guard lock(mutex);
  for (auto& part : lastSample) part.fill(0);
  started = now;
  frame.store({},std::memory_order_release);
  pending.clear(); batch.clear();
  pending.reserve(capacity); batch.reserve(capacity);
  dropped = 0;
  active.store(true, std::memory_order_release);
  return;
 }
 const bool finished = GetTickCount64() - started >= 1000ull*captureSeconds;
 {
  std::lock_guard lock(mutex);
  if (finished) active.store(false, std::memory_order_release);
  pending.swap(batch);
 }
 for (const auto& s : batch) {
  fprintf(output, "%llu,%llx,%u,%u,%u,%d,%d,%d,%d,%u,%.9g,%.9g,%u,%.9g,%u,%u,%d\n",
          s.time, static_cast<unsigned long long>(s.instance), s.part, s.mesh,
          s.material, s.original, s.resolved, s.readable, s.packetMeshMatches,
          s.packetIndex, s.modelFade, s.packetValue,s.frameTick,s.plannedHidden,s.firstIndex,s.indexCount,s.baseVertex);
 }
 batch.clear();
 if (finished) {
  fprintf(output, "# dropped=%u\n", dropped.load());
  fclose(output); output = nullptr;
  frame.store({},std::memory_order_release);
 } else { fflush(output); }
}
} // namespace worlddiff::dissolve_probe
