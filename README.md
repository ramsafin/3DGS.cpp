# 3DGS.cpp

3DGS.cpp is a high-performance, cross-platform implementation
of [Gaussian Splatting](https://repo-sam.inria.fr/fungraph/3d-gaussian-splatting/) built entirely on
the [Vulkan API](https://www.khronos.org/vulkan/) and compute pipelines. The full rasterization
pipeline - covariance precomputation, preprocessing, radix sorting, tile binning, and tiled
rendering - runs as a sequence of compute shaders, with no dependency on a graphics rasterizer.

The renderer ships as a core library plus optional frontends:

- **On-screen viewer** (`3dgs_viewer`) - an interactive GLFW/ImGui window for exploring a scene.
- **Off-screen renderer** (`3dgs_render`) - a headless tool that renders camera poses from a JSON
  config into Vulkan storage images and writes PPM files.

## Status

The renderer is functional on Windows with the Vulkan compute pipeline. The build and tooling have
recently been modernized:

- Single target-based CMake project with `CMakePresets.json` and Ninja single-config builds.
- Archive-pinned, hash-verified dependencies via `FetchContent`.
- Deterministic, incremental SPIR-V embedding through a host Python script.
- Stable `compile_commands.json` plus `clangd`, `clang-format`, and `clang-tidy` configuration.
- Namespaced public PImpl headers for off-screen rendering and the bundled viewer frontend.
- Core-library install/export rules with a `3dgsConfig.cmake` package.
- An `arm64-v8a` OHOS/HarmonyOS core-library cross-compilation preset (build environment required;
  see [Roadmap](#roadmap)).

## Requirements

| Tool | Notes |
| --- | --- |
| **CMake >= 3.28** | Presets and modern FetchContent. |
| **Ninja** | Generator used by all presets. |
| **C++20 compiler** | MSVC (`cl`, VS 2019 16.11+) is the primary Windows path; LLVM/Clang also works. |
| **Vulkan SDK** | Provides headers, the loader, and `glslangValidator`. Export `VULKAN_SDK`. |
| **Python 3** | Required at build time to embed compiled SPIR-V into a C++ header. |

A Vulkan 1.2-capable GPU and driver are required at runtime. The current radix-sort path also
requires `shaderInt64`, shared 64-bit atomics, storage-image writes without a format qualifier, and
compute subgroup size 32 with basic, arithmetic, and ballot subgroup operations. Unsupported GPUs
are rejected at startup with capability diagnostics.

Optional, for editor tooling: `clangd`, `clang-format`, and `clang-tidy` (LLVM).

## Getting Started

Clone the repository:

```bash
git clone https://github.com/shg8/3DGS.cpp/
cd 3DGS.cpp
```

### Building with presets (recommended)

On Windows, launch a **Visual Studio Developer PowerShell** so `cl`, Ninja, and the Windows SDK are
on `PATH`, and make sure `VULKAN_SDK` and a Python 3 interpreter are available. Then:

```powershell
# Configure + build the interactive viewer and off-screen app (Debug)
cmake --preset windows-msvc-onscreen-debug
cmake --build --preset windows-msvc-onscreen-debug
```

Available configure/build presets:

| Preset | Components | Build type | Primary output |
| --- | --- | --- | --- |
| `windows-msvc-onscreen-debug` | Viewer + off-screen app | Debug | `3dgs_viewer.exe`, `3dgs_render.exe` |
| `windows-msvc-onscreen-release` | Viewer + off-screen app | Release | `3dgs_viewer.exe`, `3dgs_render.exe` |
| `windows-msvc-offscreen-debug` | Core + off-screen app + tests | Debug | `3dgs_render.exe` |
| `windows-msvc-offscreen-release` | Core + off-screen app | Release | `3dgs_render.exe` |

Binaries are written to `build/<preset>/apps/viewer/` or `build/<preset>/apps/offscreen/`.

### Running tests

The off-screen Debug preset builds the CPU and Vulkan integration tests:

```powershell
cmake --preset windows-msvc-offscreen-debug
cmake --build --preset windows-msvc-offscreen-debug
ctest --preset windows-msvc-offscreen-debug
```

### Building without presets

```bash
cmake -S . -B build/default -G Ninja -DVKGS_BUILD_VIEWER=ON -DVKGS_BUILD_OFFSCREEN_APP=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/default
```

### Build Components

The viewer library/application and off-screen rendering application can be selected independently:

- `VKGS_BUILD_VIEWER` (default `ON`) builds GLFW window/swapchain support and the desktop ImGui/ImPlot viewer.
- `VKGS_BUILD_OFFSCREEN_APP` (default `ON`) builds the headless `3dgs_render` tool.
- `VKGS_BUILD_TESTS` (default `OFF`) builds the CPU and Vulkan integration tests.

`VKGS_VERBOSE_CONFIGURE` (default `ON`) prints the resolved compiler, dependencies, and toolchain during
configuration.

## Scenes

The renderer consumes **trained Gaussian Splatting** PLY files - binary point clouds whose vertices
carry the 62 float properties produced by training (`x/y/z`, `nx/ny/nz`, `f_dc_*`, `f_rest_*`,
`opacity`, `scale_*`, `rot_*`). A raw COLMAP/Open3D `input.ply` (just positions, normals, and colors)
is **not** compatible and will fail to load.

To grab ready-made trained scenes, download and extract the official pretrained models from the
3D Gaussian Splatting authors:

```bash
# ~14 GB download
curl -L -o models.zip https://repo-sam.inria.fr/fungraph/3d-gaussian-splatting/datasets/pretrained/models.zip
# or, with PowerShell:
# Invoke-WebRequest -Uri https://repo-sam.inria.fr/fungraph/3d-gaussian-splatting/datasets/pretrained/models.zip -OutFile models.zip
```

After extraction, each scene contains a trained model under
`<scene>/point_cloud/iteration_30000/point_cloud.ply`. Point the viewer or render config at that
file (not `input.ply`):

```bash
./3dgs_viewer <scene>/point_cloud/iteration_30000/point_cloud.ply
```

## Usage

### On-screen viewer

```text
  ./3dgs_viewer {OPTIONS} [scene]

    Vulkan Splatting

  OPTIONS:

      -h, --help                        Display this help menu
      --validation                      Enable Vulkan validation layers
      -v, --verbose                     Enable verbose logging
      -d[physical-device],
      --device=[physical-device]        Select physical device by index
      -i, --immediate-swapchain         Set swapchain mode to immediate
                                        (VK_PRESENT_MODE_IMMEDIATE_KHR)
      -w[width], --width=[width]        Set window width
      -h[height], --height=[height]     Set window height
      --no-gui                          Disable GUI
      scene                             Path to scene file
```

### Off-screen renderer

```text
  ./3dgs_render --config <render.json> [--output <dir>] [--device <id>] [--validation] [--verbose]
```

`3dgs_render` reads camera poses from a JSON config, renders each into a Vulkan storage image, and
writes PPM files. A starting point lives at `apps/offscreen/examples/simple_render.json` - set its
`scene` field to a trained `point_cloud.ply` (see [Scenes](#scenes)); see `apps/offscreen/README.md`
for the full config format.

## Editor Tooling

Presets export `compile_commands.json` and the active database is mirrored to
`build/compile_commands.json` for `clangd`. The repository ships `.clang-format`, `.clang-tidy`,
`.clangd`, and `.vscode/` templates (configure/build tasks and `lldb` launch configurations).
Only one preset owns the stable compilation database at a time - refresh it after switching build
components or compiler.

## Project Structure

```text
3DGS.cpp/
  CMakeLists.txt           # Root project
  CMakePresets.json        # Windows + OHOS presets
  cmake/Dependencies.cmake # Archive-pinned, hash-verified FetchContent
  include/3dgs/            # Public PImpl API headers
  src/                     # Core renderer, scene, session, and Vulkan code
    core/                  # Shared CPU utilities
    render/                # GPU ABI types and render sizing helpers
    scene/                 # CPU PLY ingestion and GPU scene upload
    session/               # Off-screen and viewer session adapters
    vulkan/                # Vulkan context, buffers, descriptors, pipelines, windowing
    shaders/               # Compute shaders (.comp) + common GLSL
  apps/
    viewer/                # 3dgs_viewer
    offscreen/             # 3dgs_render
  tools/embed_shaders.py   # Host-side SPIR-V -> C++ header embedder
```

## Roadmap

- **Compute pass extraction** - split the remaining monolithic `Renderer` internals into explicit
  covariance, preprocess, radix-sort, and tile-render modules coordinated by a pipeline object.
- **Public window adapter API** - expand the current bundled GLFW adapter into a custom surface/input
  adapter contract without leaking renderer internals.
- **OHOS/HarmonyOS support** - the `ohos-arm64-core-*` presets currently target a static core
  library only (requires `OHOS_SDK_NATIVE`). A runnable OHOS application still needs native surface
  creation, module packaging, deployment, and remote-debug wiring. GPUs with a subgroup size other
  than 32 also need a revised or specialized radix-sort shader.
- **Package polish** - export optional viewer targets and keep the external-consumer package test
  green as the public API stabilizes.

## License

The main project is licensed under LGPL.

This project uses several third-party libraries:

- **GLM**: [MIT License](https://opensource.org/licenses/MIT).
- **args.hxx**: [MIT License](https://opensource.org/licenses/MIT).
- **spdlog**: [MIT License](https://opensource.org/licenses/MIT).
- **ImGUI**: [MIT License](https://opensource.org/licenses/MIT).
- **Vulkan Memory Allocator**: [MIT License](https://opensource.org/licenses/MIT).
- **VkRadixSort**: [MIT License](https://opensource.org/licenses/MIT).
- **implot**: [MIT License](https://opensource.org/licenses/MIT).
- **glfw**: [zlib/libpng license](https://www.glfw.org/license.html).
- **nlohmann/json**: [MIT License](https://opensource.org/licenses/MIT).
