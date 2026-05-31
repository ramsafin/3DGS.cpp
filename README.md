# 3DGS.cpp

3DGS.cpp is a Vulkan compute implementation of the forward rendering path for
[3D Gaussian Splatting](https://repo-sam.inria.fr/fungraph/3d-gaussian-splatting/).
It loads already trained Gaussian Splatting PLY scenes, projects and sorts splats on the GPU, and
renders them through compute shaders without using a graphics rasterization pipeline.

The repository currently provides:

- `3dgs_core`: a static C++20 library with the off-screen renderer API.
- `3dgs_viewer`: an optional GLFW/ImGui viewer for interactive scene inspection.
- `3dgs_render`: an optional headless renderer that reads JSON camera jobs and writes PPM images.
- Vulkan compute shaders for covariance precompute, preprocess, prefix sum, radix sort, tile
  boundary construction, and tiled compositing.
- Engineering docs for architecture, rendering math, shader passes, and future training extension
  points under [`docs/`](docs/README.md).

## Current Scope

This is a forward renderer, not a training framework. It does not currently include a training
loop, optimizer, loss functions, gradient buffers, backward shaders, COLMAP reconstruction, or raw
point-cloud conversion.

The renderer consumes trained 3DGS PLY files whose vertices contain the 62 float properties produced
by training:

```text
x/y/z, nx/ny/nz, f_dc_*, f_rest_*, opacity, scale_*, rot_*
```

A raw COLMAP/Open3D `input.ply` with only positions, normals, and colors is not compatible and will
fail validation.

## Runtime Requirements

At runtime the selected GPU must support the renderer's Vulkan compute requirements:

- Vulkan 1.2.
- A compute queue and compute workgroup size of at least 256.
- `shaderInt64`.
- `shaderStorageImageWriteWithoutFormat`.
- Off-screen mode: `R8G8B8A8_UNORM` optimal images with storage-image and transfer-source support.
- Viewer mode: graphics and presentation support, dynamic rendering, swapchain support, and surface
  images usable as both color attachments and storage images.

The renderer chooses between two radix-sort implementations at startup:

- Fast mode uses subgroup size 32, compute subgroup basic/arithmetic/ballot operations, and shared
  64-bit atomics.
- Portable mode avoids those fast-path requirements but still requires the baseline Vulkan features
  above.

Unsupported devices are rejected at startup with capability diagnostics.

## Build Requirements

| Tool | Notes |
| --- | --- |
| CMake >= 3.28 | Presets and modern FetchContent. |
| Ninja | Generator used by the shipped presets. |
| C++20 compiler | MSVC is the primary Windows path; Clang/GCC-style toolchains are supported by CMake code where available. |
| Vulkan SDK | Provides the loader and `glslangValidator`. Export `VULKAN_SDK`. |
| Python 3 | Used at build time to embed compiled SPIR-V into a generated C++ header. |

Dependencies are archive-pinned and fetched by CMake:

- GLM
- spdlog
- Vulkan-Headers
- GLFW, ImGui, and ImPlot when `VKGS_BUILD_VIEWER=ON`
- nlohmann/json when `VKGS_BUILD_OFFSCREEN_APP=ON` or `VKGS_BUILD_TESTS=ON`
- GoogleTest when `VKGS_BUILD_TESTS=ON`

## Building

On Windows, use a Visual Studio Developer PowerShell so `cl`, Ninja, the Windows SDK, Vulkan SDK,
and Python are on `PATH`.

```powershell
cmake --preset windows-msvc-onscreen-debug
cmake --build --preset windows-msvc-onscreen-debug
```

Available presets:

| Preset | Components | Build type | Main outputs |
| --- | --- | --- | --- |
| `windows-msvc-onscreen-debug` | Core, viewer, off-screen app | Debug | `3dgs_viewer.exe`, `3dgs_render.exe` |
| `windows-msvc-onscreen-release` | Core, viewer, off-screen app | Release | `3dgs_viewer.exe`, `3dgs_render.exe` |
| `windows-msvc-offscreen-debug` | Core, off-screen app, tests | Debug | `3dgs_render.exe`, tests |
| `windows-msvc-offscreen-release` | Core, off-screen app | Release | `3dgs_render.exe` |
| `windows-msvc-tooling-debug` | Core, viewer, off-screen app, tests, tooling targets | Debug | all project targets |
| `ohos-arm64-core-debug` | Core library only | Debug | `3dgs_core` |
| `ohos-arm64-core-release` | Core library only | Release | `3dgs_core` |

The OHOS presets require a configured OHOS native SDK and `OHOS_SDK_NATIVE` in the environment.

Without presets:

```bash
cmake -S . -B build/default -G Ninja -DVKGS_BUILD_VIEWER=ON -DVKGS_BUILD_OFFSCREEN_APP=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/default
```

Useful CMake options:

- `VKGS_BUILD_VIEWER` (default `ON`): builds GLFW window/swapchain support and `3dgs_viewer`.
- `VKGS_BUILD_OFFSCREEN_APP` (default `ON`): builds `3dgs_render`.
- `VKGS_BUILD_TESTS` (default `OFF`): builds unit and integration tests.
- `VKGS_ENABLE_PROJECT_WARNINGS` (default `OFF`): enables extra warnings for project-owned targets.
- `VKGS_VERBOSE_CONFIGURE` (default `ON`): prints resolved toolchain and dependency paths.

## Running

### Scene Files

Download trained scenes from the official 3DGS pretrained models if you need sample inputs:

```bash
curl -L -o models.zip https://repo-sam.inria.fr/fungraph/3d-gaussian-splatting/datasets/pretrained/models.zip
```

After extraction, each scene's trained model is usually under:

```text
<scene>/point_cloud/iteration_30000/point_cloud.ply
```

Use that `point_cloud.ply`, not the raw `input.ply`.

### Viewer

```text
3dgs_viewer [--validation] [--verbose] [--device <id>] [--immediate-swapchain]
            [--width <px>] [--height <px>] [--no-gui] <scene.ply>
```

Example:

```powershell
build/windows-msvc-onscreen-debug/apps/viewer/3dgs_viewer.exe `
  path/to/point_cloud/iteration_30000/point_cloud.ply
```

The viewer supports orbit/pan/dolly camera controls and an optional ImGui/ImPlot overlay.

### Off-Screen Renderer

```text
3dgs_render --config <render.json> [--output <dir>] [--device <id>] [--validation] [--verbose]
```

`3dgs_render` reads camera frames from JSON, renders each frame into an off-screen Vulkan storage
image, reads pixels back, and writes binary PPM files. See
[`apps/offscreen/README.md`](apps/offscreen/README.md) and
[`apps/offscreen/examples/simple_render.json`](apps/offscreen/examples/simple_render.json) for the
current config format.

## Library Use

The public headers live under [`include/3dgs`](include/3dgs):

- [`3dgs/OffscreenRenderer.hpp`](include/3dgs/OffscreenRenderer.hpp) exposes the headless rendering
  API.
- [`3dgs/Viewer.hpp`](include/3dgs/Viewer.hpp) exposes the bundled viewer API when viewer support is
  built.
- [`3dgs/Types.hpp`](include/3dgs/Types.hpp) contains shared camera and extent types.

The install rules export the core library as a CMake package:

```powershell
cmake --install build/windows-msvc-offscreen-release --prefix install
```

External CMake projects can consume the installed package with `find_package(3dgs CONFIG REQUIRED)`
and link `3dgs::core`.

## Tests And Tooling

The Windows off-screen Debug and tooling presets build tests:

```powershell
cmake --preset windows-msvc-offscreen-debug
cmake --build --preset windows-msvc-offscreen-debug
ctest --preset windows-msvc-offscreen-debug
```

The test suite covers ABI layout contracts, PLY parsing, render-job JSON parsing, pass sizing,
camera controls, Vulkan device requirements, buffers, timestamp queries, radix sorting, shader
module validation, and a golden-image render path on suitable Vulkan hardware.

The tooling preset also enables:

```powershell
cmake --build --preset windows-msvc-tooling-debug --target vkgs_format_check
cmake --build --preset windows-msvc-tooling-debug --target vkgs_tidy
```

`compile_commands.json` is mirrored to `build/compile_commands.json` for `clangd`. Refresh it after
switching presets because only one preset owns that stable database at a time.

## Documentation

- [Architecture](docs/architecture.md)
- [Rendering pipeline](docs/rendering-pipeline.md)
- [Rendering math](docs/rendering-math.md)
- [Training extension points](docs/training-extension-points.md)
- [Shader pass notes](docs/shaders/README.md)

The training document is a future-plan guide. It does not describe current executable training
support.

## Project Structure

```text
3DGS.cpp/
  CMakeLists.txt           # Root project
  CMakePresets.json        # Windows and OHOS presets
  cmake/                   # Dependency, install, tooling, and target helpers
  docs/                    # Architecture, rendering, math, and shader notes
  include/3dgs/            # Public PImpl API headers
  src/                     # Core renderer, scene, session, Vulkan, and shaders
    core/                  # Shared CPU utilities
    render/                # GPU ABI types, constants, and sizing helpers
    scene/                 # Trained PLY ingestion and GPU scene upload
    session/               # Off-screen and viewer session adapters
    vulkan/                # Vulkan context, buffers, descriptors, pipelines, windowing
    shaders/               # GLSL compute shaders and shared constants
  apps/
    viewer/                # Interactive viewer executable
    offscreen/             # Headless JSON-to-PPM renderer
  tests/                   # CPU and Vulkan tests
  tools/embed_shaders.py   # SPIR-V to C++ header embedder
```

## License

The main project is licensed under LGPL. Third-party dependencies keep their own licenses, including
MIT-licensed GLM, spdlog, args.hxx, ImGui, ImPlot, Vulkan Memory Allocator, VkRadixSort, and
nlohmann/json, plus GLFW's zlib/libpng license.
