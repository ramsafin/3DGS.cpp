# OHOS arm64 off-screen rendering plan

## Current failure

The observed OHOS run reaches Vulkan instance creation and enumerates the `Maleoon 910`. The capability probe shows the device satisfies the hard off-screen Vulkan requirements, but lacks `VK_KHR_shader_non_semantic_info` and `shaderInt64`. The renderer now makes shader debug printf opt-in and can select an integrated no-`shaderInt64` off-screen sort path at runtime.

Those two diagnostics have different meanings:

- `VK_KHR_shader_non_semantic_info` should only be requested when shader debug printf is explicitly enabled. It should not decide whether a production off-screen renderer can run on OHOS.
- Devices with `shaderInt64` can use the existing 64-bit key path composed from tile index and depth bits; devices without it use the portable `uint32` pair sort-key mode.

## Why start with a separate GPU capability executable

Yes. Start with a small OHOS-deployable capability probe before changing the renderer. The current `3dgs_render` failure only reports the first suitability gate from the renderer's requirements, which makes it hard to distinguish:

1. debug-only extension issues,
2. hard hardware/driver feature gaps,
3. format/queue/timestamp limitations, and
4. shader-path choices that the renderer could potentially make portable.

A separate executable under `apps/` should avoid linking the full renderer path and should only create a Vulkan instance, enumerate devices, and print capabilities. This gives a stable target that can be built and deployed with the existing OHOS toolchain even when `3dgs_render` cannot select a device.

## Proposed probe target

`vkgs_caps` lives under `apps/caps/` and is gated by the `VKGS_BUILD_CAPS_APP` CMake option. Use the `ohos-arm64-caps-debug` or `ohos-arm64-caps-release` preset to build only the probe target for OHOS deployment.

Recommended output:

- Instance API version and instance extensions.
- For each physical device:
  - name, vendor ID, device ID, driver version, device type, Vulkan API version,
  - device extensions, especially `VK_KHR_shader_non_semantic_info`, `VK_KHR_swapchain`, `VK_KHR_dynamic_rendering`, and portability/debug extensions if present,
  - Vulkan 1.0/1.1/1.2 feature bits used by this project:
    - `shaderInt64`,
    - `shaderStorageImageWriteWithoutFormat`,
    - `shaderSharedInt64Atomics`,
    - subgroup size/stages/operations,
  - queue families: flags, queue count, timestamp valid bits,
  - limits: `maxComputeWorkGroupInvocations`, `maxComputeWorkGroupSize[0..2]`, storage-buffer and storage-image limits,
  - format properties for the off-screen target formats:
    - `VK_FORMAT_R8G8B8A8_UNORM`,
    - optionally `VK_FORMAT_B8G8R8A8_UNORM` and `VK_FORMAT_R16G16B16A16_SFLOAT`,
  - a renderer-compatibility summary using the same checks as `DeviceRequirements`.

`vkgs_caps --json <path>` emits both human-readable text and a structured JSON file so OHOS logs can be compared across devices and OS builds.

## Decision tree after collecting capability logs

### 1. `VK_KHR_shader_non_semantic_info` is the only missing item

Make the debug shader-nonsemantic extension optional or validation-only:

- do not push `VK_KHR_shader_non_semantic_info` as a required device extension unconditionally in `DEBUG`, or
- check if it is advertised before adding it to the logical-device extension list.

This is the lowest-risk renderer change because it only affects diagnostics/debug metadata.

### 2. `shaderInt64` is missing, but storage images, compute queues, and workgroup size are OK

Use the portable sort-key path that avoids 64-bit shader integers. The renderer stores tile id and depth as separate `uint32_t` buffers, performs four stable 8-bit passes over depth, then four stable 8-bit passes over tile id. Because the radix pass is stable, the second four-pass group preserves depth ordering within each tile while producing the tile-key order expected by boundary generation.

### 3. `shaderStorageImageWriteWithoutFormat` or `R8G8B8A8_UNORM` storage+transfer support is missing

Plan a render target fallback:

- choose an OHOS-supported storage image format from the probe results,
- adjust shader image format declarations and readback conversion,
- keep the public off-screen output format unchanged by converting during readback/PPM writing.

### 4. Timestamp bits are missing on compute queues

Keep rendering functional and disable timing queries. The current off-screen requirements do not require timestamp support, so this should remain a performance-metrics-only concern.

## Suggested implementation sequence

1. Build and deploy `vkgs_caps` for OHOS; collect logs from the target device.
2. Keep `VK_KHR_shader_non_semantic_info` gated behind `VKGS_ENABLE_SHADER_DEBUG_PRINTF` so production OHOS runs do not require the debug-only extension.
3. Re-run `3dgs_render`; devices without `shaderInt64` should select `uint32 pair portable`, allocate the additional 32-bit depth-key buffers, and dispatch the depth-then-tile radix passes automatically.
4. Validate output ordering and image parity on OHOS scenes, then tune the portable sort workgroup sizing if needed.
5. Re-enable or broaden the OHOS off-screen app preset once target-device validation is complete.

## Open questions for the capability probe

- Does the OHOS driver expose Vulkan 1.2 or only a lower core version plus extensions?
- Are storage images and transfer-source usage supported for `R8G8B8A8_UNORM` optimal images?
- What subgroup size and subgroup operations are reported for compute?
- Does the device expose `shaderSharedInt64Atomics` even if `shaderInt64` is absent? If not, the fast sort path is impossible, but a 32-bit portable path may still be viable.
- Which queue families support compute, graphics, and timestamps?

## Milestone C `uint32` pair path

The no-`shaderInt64` shader modules and renderer integration now cover the portable `uint32 pair` path:

- `preprocess_sort_u32.comp` writes separate `uint` tile keys, depth keys, and payloads.
- `sort/hist_u32.comp` builds radix histograms over `uint` active keys.
- `sort/sort_pair_u32_portable.comp` stably scatters one `uint` active key while carrying a companion `uint` key and payload.
- `tile_boundary_u32.comp` derives tile boundaries from sorted `uint` tile keys directly.
- `Renderer` allocates even/odd tile-key, depth-key, and payload buffers for the selected mode, binds mode-specific descriptor alternatives, dispatches depth-key passes followed by tile-key passes, and feeds the final even payload/tile-key buffers into render and boundary generation.
