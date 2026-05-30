# Source Graphics Revision — Implementation Results

Companion to [`source-graphics-revision-audit.md`](./source-graphics-revision-audit.md). This document records the work completed across Stages 0–5 of the staged execution plan, the verification performed, and what remains deferred.

- **Status as of this report:** Stages 0–4 complete and verified. Stage 5 capability gating and cleanup complete on Windows; OHOS target validation and any non-32-subgroup fallback remain deferred.
- **Platform verified:** Windows / MSVC, Vulkan SDK 1.4.350.0.
- **Build presets exercised:** `windows-msvc-offscreen-debug`, `windows-msvc-onscreen-debug`, `windows-msvc-offscreen-release`, `windows-msvc-onscreen-release` — all build green.
- **Automated tests:** 21/21 passing (`ctest --preset windows-msvc-offscreen-debug`).
- **Runtime validation:** Offscreen render with `--validation` produces a clean frame (`frame_0000.ppm`) with no validation-layer errors. Automatic selection rejects the integrated Intel GPU with an actionable shared-64-bit-atomics diagnostic and renders on the NVIDIA GPU.

---

## Stage 0 — Test harness, ABI contracts, build hygiene

Goal: establish deterministic verification before touching graphics behavior.

| ID | Item | Result |
|----|------|--------|
| VKGS-028 | `FETCH_CONTENT_QUIET` → `FETCHCONTENT_QUIET` typo | Fixed in `cmake/Dependencies.cmake` and `CMakePresets.json` |
| — | Release build presets | Added offscreen/onscreen release presets |
| — | Test scaffolding | GoogleTest via `FetchContent`; `VKGS_BUILD_TESTS` option (default OFF); `tests/` subdir + `enable_testing()`; offscreen Debug configure/build/test preset enables the suite reproducibly |
| VKGS-011 | CPU/GPU ABI assertions | `static_assert` on size/offset for `GSScene::Vertex` (240 B), `Cov3DUpperRight` (24 B), `Renderer::UniformBuffer`, `VertexAttributeBuffer`, `RadixSortPushConstants` |

**Tests added:**
- `test_abi_contracts` — runtime `sizeof`/`offsetof` checks mirroring the static asserts.
- `test_ply_parser` — header parsing on valid/zero-vertex/malformed fixtures.
- `test_buffer` — upload/download/offset/realloc round-trips (device-gated).
- `test_gpu_sort` — radix-sort pipeline vs `std::sort` (device-gated).
- `test_golden_image` — determinism (render twice, compare hash) + golden FNV-1a regression hash (device-gated).

**Fixtures:** `valid_tiny.ply`, `zero_vertex.ply`, `no_end_header.ply` + binary PLY writer helper in `test_support.h`.

> Note: the `alignof(Vertex) == 16` assertion was removed — under MSVC/GLM `glm::vec4` carries 4-byte alignment, so the assertion was incorrect. Size/offset contracts are retained.

---

## Stage 1 — Queue policy & synchronization

| ID | Item | Result |
|----|------|--------|
| VKGS-001 | One-time command buffers tied to a single pool | `beginOneTimeCommandBuffer` now takes a `Queue::Type` (default COMPUTE); per-family lazy pools via `getOneTimePool`; removed shared `commandPool` |
| VKGS-002 | Compute↔transfer barriers | Replaced compute-write-read barriers with explicit `computeToTransferReadBarrier` / `transferToComputeReadBarrier` around prefix-sum copies |
| VKGS-003 | Swapchain sharing indices | Use `queue.second.queueFamily` (not map key) when building the unique-family set |
| VKGS-004 | Graphics+compute family selection | `findQueueFamilies` prioritizes a single family supporting both graphics and compute |

---

## Stage 2 — Robustness of data paths

| ID | Item | Result |
|----|------|--------|
| VKGS-005 | Empty scenes | `GSScene::load` throws on zero vertices; covered by `test_buffer.EmptySceneIsRejected` |
| VKGS-006 | Buffer copy offsets | `upload()`/`downloadTo()` now apply correct src/dst offsets to both copy region and `memcpy`, with bounds checks |
| VKGS-012 | Descriptor realloc | `vk::DescriptorBufferInfo` offset set to 0 (allocation-relative) instead of VMA allocation offset |
| VKGS-008 (partial) | Device suitability | Throw if no suitable devices; verify graphics/compute/present family values before logical-device creation |

---

## Stage 3 — Type discipline, PLY hardening, shared constants

| ID | Item | Result |
|----|------|--------|
| VKGS-007 / VKGS-021 | PLY schema validation | Rewrote `loadPlyHeader` to track current element and route properties; added `validatePlyLayout` (format, 62 float properties, leading names/types); `parseHeaderOnly`/`getHeader` for testing; counts widened to `uint32_t` |
| VKGS-014 | Shared GLSL/C++ constants | New `shaders/shared_constants.glsl` (`#define`s) + `GpuConstants.h` (`constexpr`). Replaced hardcoded tile/workgroup/SH/radix literals across `GSScene`, `Renderer`, `common.glsl`. Shader rebuild dependency wired in CMake |
| VKGS-015 | Near-plane uniform + shader guards | Added `near_plane` to `UniformBuffer` (now 176 B, std140) and `preprocess.comp` `Params`; reordered view-space depth check before perspective divide; epsilon guards on ray-direction length and `p_hom.w`; clamped color channels to ≥ 0 |
| VKGS-016 | Dimension validation | `Renderer` ctor and offscreen `main.cpp` reject zero width/height; `readPixels` byte-size computed as `vk::DeviceSize` to avoid overflow |
| VKGS-021 | Device-id narrowing | Viewer rejects `--device` id > 255 before narrowing to `uint8_t` |

---

## Stage 4 — Lifetime, RAII, coherency, bounds, cleanup

| ID | Item | Result |
|----|------|--------|
| VKGS-017 | Swapchain recreation | `createSwapchain` passes `oldSwapchain`; image/semaphore vectors cleared at (re)creation; removed premature `reset()`/`clear()` in `recreate()` |
| VKGS-018 | Image-count correctness | Fixed `imageCount` when `maxImageCount == 0` (unlimited); ImGui `ImageCount` uses actual swapchain image count |
| VKGS-019 | GLFW RAII | Constructor checks `glfwInit`/`glfwCreateWindow`; destructor destroys window and terminates; copy ops deleted |
| VKGS-013 | Mapped-memory coherency | `Buffer::flush()`/`invalidate()` (VMA) wired into upload/download for mapped paths |
| VKGS-010 | SPIR-V alignment | `embed_shaders.py` emits `alignas(4)` byte arrays |
| VKGS-020 | Descriptor bounds | `getDescriptorSet` validates option and computed index against `descriptorSets.size()` |
| VKGS-026 | Query-pool capacity | Centralized `kTimestampQueryCount`; `QueryManager::setCapacity` + over-capacity throw; pool create/reset use the constant |
| VKGS-022 | Control flow | Replaced `goto` in render loop with `while(true)` + `continue`/`break`; hoisted `submitInfo` |
| VKGS-023 | Self-contained headers | Added missing includes (`<cstdint>`, `<string>`, `<vector>`, `<cstddef>`, `<array>`, `<utility>`) to `GSScene.h`, `Renderer.h`, `Window.h` |
| VKGS-024 | Facade lifecycle guards | `VulkanSplatting::draw/readPixels/logMovement` throw if uninitialized; `stop()` is a no-op if uninitialized |

---

## Stage 5 — Capability gating and deferred cleanup

| ID | Item | Result |
|----|------|--------|
| VKGS-008 / VKGS-009 | Renderer capability profile | Added `getDeviceUnsuitabilityReasons`: Vulkan 1.2, extensions, queue families, compute timestamps, 256-wide workgroups, storage-image format/usage, shader features, shared 64-bit atomics, subgroup operations, and subgroup size 32 are checked before selection. Explicit `--device` selection no longer bypasses suitability checks. |
| VKGS-008 | Reduce unnecessary requirements | Offscreen mode no longer requests dynamic rendering; logical-device creation no longer forces unused sampler anisotropy or buffer 64-bit atomics. On-screen mode still checks and enables dynamic rendering. Debug builds enable shader non-semantic info consistently with their `debugPrintfEXT` SPIR-V instrumentation. |
| VKGS-008 | Partial-initialization lifetime | Initialize the VMA allocator handle to null and guard destruction so capability rejection before allocator creation reports the intended error. |
| VKGS-022 | Dead debug cleanup | Removed inert commented shader branches and stale commented timestamp/render debug blocks without changing shader behavior. |

---

## Verification summary

```text
ctest --preset windows-msvc-offscreen-debug:
                                         100% tests passed, 0 failed out of 21
build presets:                          4/4 green (offscreen/onscreen × debug/release)
offscreen --validation render:          clean; wrote frame_0000.ppm; no validation errors
offscreen --device 1 rejection:         clear error: shaderSharedInt64Atomics is not supported
```

---

## Remaining deferred work

These remain open by design:

- **VKGS-008 / VKGS-009** — Configure and run against the selected OpenHarmony (OHOS) SDK/device profile. The current code fails clearly when the radix-sort requirements are absent; it does not yet provide a specialized or runtime-safe fallback for subgroup sizes other than 32.
- **VKGS-025** — Optional `Renderer` decomposition and performance work.

## Notes for the next session

- `OHOS_SDK_NATIVE` is not set in the current environment, so the OHOS configure/build presets remain unverified.
- Golden-image regression hash is recorded under the build dir (`VKGS_GOLDEN_DIR`) and is re-recordable; re-baseline intentionally if rendering output is meant to change.
- GPU-dependent tests are device-gated and will skip cleanly on machines without a suitable Vulkan device.
