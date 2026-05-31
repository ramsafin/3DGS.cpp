# Shared Shader ABI

The shader ABI is shared by C++ structs, GLSL structs, descriptor bindings, push
constants, and generated SPIR-V symbols. Layout drift here usually causes
incorrect rendering rather than a clean compile failure, so the project keeps
several checks close to the data definitions.

## Shared Constants

[`shared_constants.glsl`](../../src/shaders/shared_constants.glsl) is included
from GLSL and wrapped by [`GpuConstants.hpp`](../../src/GpuConstants.hpp) for
C++:

| Constant | Value | Meaning |
| --- | --- | --- |
| `VKGS_TILE_WIDTH` | `16` | Tile width and render workgroup X size. |
| `VKGS_TILE_HEIGHT` | `16` | Tile height and render workgroup Y size. |
| `VKGS_WORKGROUP_SIZE` | `256` | Generic one-dimensional compute workgroup size. |
| `VKGS_SH_COEFF_VECTORS` | `16` | Number of RGB SH coefficient vectors. |
| `VKGS_SH_MAX_COEFFS` | `48` | Flat SH float count. |
| `VKGS_RADIX_SORT_BINS` | `256` | One 8-bit radix bin per byte value. |
| `VKGS_RADIX_BLOCKS_PER_WORKGROUP` | `32` | Sort blocks processed by each workgroup. |

## Struct Layouts

[`GpuTypes.hpp`](../../src/render/GpuTypes.hpp) is the authoritative C++ ABI
definition. It uses `static_assert` checks for size and offsets. Runtime tests in
`tests/test_abi_contracts.cpp` duplicate the most important checks.

The central structs are:

- `SceneVertex`: `position`, `scaleOpacity`, `rotation`, and 48 SH floats.
- `Cov3DUpperRight`: six packed floats for a symmetric 3D covariance.
- `UniformBuffer`: camera position, combined projection-view matrix, view
  matrix, image size, FOV tangents, and near plane.
- `VertexAttribute`: projected conic and opacity, color and radius, tile AABB,
  screen-space center, depth, and a debug magic value.
- `RadixSortPushConstants`: element count, byte shift, workgroup count, and
  blocks per workgroup.

The matching GLSL definitions live in
[`common.glsl`](../../src/shaders/common.glsl) and individual shader storage
buffer declarations.

## Descriptor And Push Constant Conventions

The compute pipeline wrapper supports multiple descriptor-set options. The
renderer uses that for ping/pong buffers and output image selection.

Common bindings:

- `preprocess.comp`: set 0 contains scene buffers; set 1 contains uniforms and
  preprocess outputs.
- `prefix_sum.comp`: set 0 contains source and destination scan buffers.
- `preprocess_sort.comp`: set 0 contains projected attributes, the selected
  prefix-sum buffer, key output, and payload output.
- radix sort shaders: set 0 contains key buffers, payload buffers, and
  histograms.
- `tile_boundary.comp`: set 0 contains sorted keys and boundary output.
- `render.comp`: set 0 contains attributes, boundaries, sorted payloads; set 1
  contains the output storage image.

Push constants are intentionally small:

- covariance precompute: scale factor;
- prefix sum: timestep;
- expansion: tile count in X;
- radix sort: `RadixSortPushConstants`;
- tile boundary: instance count;
- render: output width and height.

## SPIR-V Embedding

[`src/shaders/CMakeLists.txt`](../../src/shaders/CMakeLists.txt) compiles each
shader to SPIR-V using `glslangValidator`. The generated modules are passed to
[`tools/embed_shaders.py`](../../tools/embed_shaders.py), which emits a C++
header containing byte arrays such as `SPV_PREPROCESS` and `SPV_RENDER`.

Runtime shader construction loads those embedded arrays through
[`Shader`](../../src/vulkan/Shader.cpp). Tests cover empty and malformed embedded
module inputs.

