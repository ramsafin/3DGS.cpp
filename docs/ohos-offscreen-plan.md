# OHOS arm64 off-screen rendering plan

## Current failure

The observed OHOS run reaches Vulkan instance creation and enumerates the `Maleoon 910`, but rejects it before creating a logical device because the Debug build requires `VK_KHR_shader_non_semantic_info` and the renderer requires `shaderInt64`.

Those two diagnostics have different meanings:

- `VK_KHR_shader_non_semantic_info` is currently requested for every `DEBUG` build. This is useful for debug shader metadata, but it should not decide whether a production off-screen renderer can run on OHOS.
- `shaderInt64` is a true renderer requirement today. The existing sort-key path stores a 64-bit key composed from tile index and depth bits, and the Vulkan feature request enables `shaderInt64` before logical-device creation.

## Why start with a separate GPU capability executable

Yes. Start with a small OHOS-deployable capability probe before changing the renderer. The current `3dgs_render` failure only reports the first suitability gate from the renderer's requirements, which makes it hard to distinguish:

1. debug-only extension issues,
2. hard hardware/driver feature gaps,
3. format/queue/timestamp limitations, and
4. shader-path choices that the renderer could potentially make portable.

A separate executable under `apps/` should avoid linking the full renderer path and should only create a Vulkan instance, enumerate devices, and print capabilities. This gives a stable target that can be built and deployed with the existing OHOS toolchain even when `3dgs_render` cannot select a device.

## Proposed probe target

Add an app target such as `vkgs_caps` under `apps/caps/` and gate it with a CMake option such as `VKGS_BUILD_CAPS_APP`. For OHOS, add a preset or override that enables only this app first.

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

The probe should emit both human-readable text and an optional JSON file so OHOS logs can be compared across devices and OS builds.

## Decision tree after collecting capability logs

### 1. `VK_KHR_shader_non_semantic_info` is the only missing item

Make the debug shader-nonsemantic extension optional or validation-only:

- do not push `VK_KHR_shader_non_semantic_info` as a required device extension unconditionally in `DEBUG`, or
- check if it is advertised before adding it to the logical-device extension list.

This is the lowest-risk renderer change because it only affects diagnostics/debug metadata.

### 2. `shaderInt64` is missing, but storage images, compute queues, and workgroup size are OK

Plan a portable sort-key path that avoids 64-bit shader integers. The current shaders build 64-bit keys from `(tileIndex << 32) | depthBits`; this needs `shaderInt64` even in the portable radix-sort path. Options to evaluate:

- split the key into two `uint32_t` buffers/fields (`tileIndex`, `depthBits`) and sort lexicographically,
- perform two stable 32-bit sorts: depth within tile and then tile, or tile then depth depending on the existing sort direction requirements,
- pack tile and quantized depth into a 32-bit key only if output dimensions and quality requirements allow it,
- move key expansion or final ordering to CPU only as a diagnostic fallback, not as the desired renderer path.

This is the likely main OHOS renderer task if the Maleoon 910 truly lacks `shaderInt64`.

### 3. `shaderStorageImageWriteWithoutFormat` or `R8G8B8A8_UNORM` storage+transfer support is missing

Plan a render target fallback:

- choose an OHOS-supported storage image format from the probe results,
- adjust shader image format declarations and readback conversion,
- keep the public off-screen output format unchanged by converting during readback/PPM writing.

### 4. Timestamp bits are missing on compute queues

Keep rendering functional and disable timing queries. The current off-screen requirements do not require timestamp support, so this should remain a performance-metrics-only concern.

## Suggested implementation sequence

1. Add and deploy `vkgs_caps` for OHOS; collect logs from the target device.
2. Remove the debug-only hard dependency on `VK_KHR_shader_non_semantic_info` if the probe confirms it is absent.
3. Re-run `3dgs_render`; if it still fails only on `shaderInt64`, implement a no-`shaderInt64` key/sort path behind a runtime capability switch.
4. Add tests for device-requirement classification and sort-key packing/splitting.
5. Re-enable the OHOS off-screen app preset once the required runtime path is selected automatically.

## Open questions for the capability probe

- Does the OHOS driver expose Vulkan 1.2 or only a lower core version plus extensions?
- Are storage images and transfer-source usage supported for `R8G8B8A8_UNORM` optimal images?
- What subgroup size and subgroup operations are reported for compute?
- Does the device expose `shaderSharedInt64Atomics` even if `shaderInt64` is absent? If not, the fast sort path is impossible, but a 32-bit portable path may still be viable.
- Which queue families support compute, graphics, and timestamps?
