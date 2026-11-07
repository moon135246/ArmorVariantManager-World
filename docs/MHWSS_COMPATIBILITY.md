# MHWSS compatibility investigation

This change targets WorldDiff 1.3.0 with the reported MHWSS 1.0.2 interaction on
Steam MHW 15.23.00 (421810). It does not modify or distribute MHWSS.

## Findings

The old overlay created a D3D11 device/swapchain and a second D3D12 device/queue
to discover shared virtual methods. A D3D12CreateDevice hook installed by another
mod observes this extra creation. There was also an unchecked successful null
device return, a raw device-pointer comparison for queue selection, and resource
creation through Swapchain::GetDevice rather than the selected queue's device.
Those assumptions are unsuitable for wrapped graphics objects.

The supplied report is not treated as a verified crash analysis. In the supplied
MHWSS.dll (SHA256 `55d52caf2e7bba5220ec721149bc067679bf9391bbf55d62bed3ec9522ccd09a`),
the ExecuteCommandLists proxy explicitly passes through a command list when its
private QueryInterface fails. The reported +0x114D47 instruction belongs to a
different helper, called with width/height data, and is not that unwrapping loop.
The report's claim that unknown command lists directly cause this null write is
therefore unsupported. No original crash dumps were supplied; the precise failed
object and an alleged NGX initialization race still require a real crash capture.

## Implementation

- Redirect only the game's two verified Present callsites, with full instruction
  signature checks before installing either hook. DX12: RVA 0x23AA1EA; DX11:
  0x23AA337. These were independently inspected in the running 421810 executable.
- At the DX12 call, RBX is the renderer, R12 the swapchain index and RCX the actual
  swapchain. Verify the swap at renderer+0x1470+index*0x48, then borrow its DIRECT
  queue from renderer+0x20. No process-wide first-queue heuristic or probe device.
- Use that exact queue's GetDevice for all overlay objects, including the ImGui
  font upload. Preserve the game's proxy objects; never access private MHWSS
  layouts or patch its instructions.
- Call the original Present once with unchanged arguments and return its HRESULT
  unchanged. The handoff is thread-local. Resize hook originals are stored by
  implementation so raw and proxy swapchains cannot replace each other's target.
- Release partial initialization on failure, reject null device outputs and
  unsupported buffer counts, and rebuild from the current descriptor after
  resize. Release the held queue and swapchain on shutdown.

The overlay is deliberately disabled if the executable's callsite signatures
do not match. Existing visibility control continues independently. Game updates
or another mod patching those same instructions require rechecking the signature;
this is preferable to guessing a queue or calling an unknown renderer layout.

## Validation

Run `tools\test-render-compat.cmd`. It compiles with /W4 /WX and exercises the actual
overlay dispatcher and backend using an isolated WARP device, hidden swapchain
and proxy queue. Coverage includes exact queue selection, invalid/null inputs,
signature mismatch, DX11/DX12 forwarding and HRESULTs, thread-local state,
successful-null GetDevice, failed/partial initialization, changed buffer counts,
font upload and rendering submissions, and repeated cleanup/reinitialization.
The fixture's Swapchain::GetDevice deliberately fails: relying on it regresses
the tests. No game files or personal settings are read or changed by the test.

Passing this suite is not NVIDIA DLAA or real MHWSS acceptance. Verify separately:
normal cold starts, the loaded MHWSS version, overlay opening/closing, DLAA/FSR
output, resize/fullscreen transitions, and shutdown. Avoid forced-kill restart
loops. Do not bundle third-party DLLs with the WorldDiff candidate.

### Local acceptance, 2026-11-07

The installed candidate and unmodified MHWSS 1.0.2 were tested on an AMD RX 9070,
DX12, TAA, High resolution scaling and CAS off. Both DLLs were verified loaded;
WorldDiff's logged device and queue vtables resolved to MHWSS. The user confirmed
both panels and gameplay worked after selecting TAA. A second normal cold start
also reached the gathering hub.

MHWSS logged a GetJitter constant-buffer range assertion and Map error 80070057.
A control run with the WorldDiff DLL removed (and verified absent from loaded
modules) reproduced both messages and reached the same hub. Other mods were kept
unchanged. These errors therefore occur independently of WorldDiff in this setup;
the control does not establish their root cause or validate FSR image correctness.
NGX initialization also fails on this AMD system, so NVIDIA DLAA acceptance remains
outstanding. Do not describe these local checks as an error-free MHWSS run or proof
that every reported startup crash is fixed.

After restoring the same candidate DLL, a third combined cold start reached the
hub. The user confirmed switching to windowed and back to borderless mode, then
opening/closing both panels, with normal gameplay. The candidate and MHWSS were
left installed for further testing. NVIDIA hardware, long sessions, all other
loader combinations and exclusive-fullscreen transitions remain unverified.

## Interface and location references

- [Microsoft CreateSwapChain contract](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgifactory-createswapchain):
  a D3D12 swapchain is associated with a DIRECT command queue, not merely an adapter.
- [SharpPluginLoader address records](https://github.com/Fexty12573/SharpPluginLoader/blob/master/Assets/Common/AddressRecords.json)
  and [D3DModule](https://github.com/Fexty12573/SharpPluginLoader/blob/master/mhw-cs-plugin-loader/D3DModule.cpp):
  independent references for the game presentation boundaries and renderer fields.
  The implementation here was written for WorldDiff and checked against the local game.
