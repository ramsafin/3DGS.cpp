# 3DGS.cpp Documentation

This directory documents the current renderer implementation, the math used by
the forward rendering path, and future extension points for training support.
It is written for engineers who are comfortable with C++, Vulkan, and graphics
pipelines.

## Recommended Reading Order

1. [Architecture](architecture.md) gives the project map: public APIs,
   sessions, renderer orchestration, scene loading, Vulkan wrappers, shader
   embedding, and tests.
2. [Rendering Pipeline](rendering-pipeline.md) follows a trained Gaussian PLY
   file through CPU loading, GPU upload, compute passes, sorting, tile binning,
   and final image output.
3. [Rendering Math](rendering-math.md) explains the equations behind covariance
   construction, projection, conic splats, spherical harmonics color, depth
   sorting, and alpha compositing.
4. [Training Extension Points](training-extension-points.md) describes future
   integration points for differentiable rendering and optimization. This is
   not current functionality.
5. [Shader Notes](shaders/README.md) is the entry point for per-shader analysis.

## Current Scope

3DGS.cpp is currently a forward renderer for already trained 3D Gaussian
Splatting scenes. It consumes binary little-endian PLY files with the 62 float
properties produced by 3DGS training:

- position: `x`, `y`, `z`
- normal placeholders: `nx`, `ny`, `nz`
- spherical harmonics: `f_dc_*`, `f_rest_*`
- density and shape: `opacity`, `scale_*`, `rot_*`

Raw COLMAP or Open3D point clouds are not valid renderer inputs. There is no
training loop, optimizer, loss function, or backward shader pipeline in the
repository today.

## Source Anchors

The main implementation paths referenced by these docs are:

- Public API: [`include/3dgs`](../include/3dgs)
- Renderer orchestration: [`src/Renderer.cpp`](../src/Renderer.cpp)
- GPU ABI types: [`src/render/GpuTypes.hpp`](../src/render/GpuTypes.hpp)
- Scene loading and upload: [`src/scene`](../src/scene)
- Vulkan helpers: [`src/vulkan`](../src/vulkan)
- Compute shaders: [`src/shaders`](../src/shaders)
- Off-screen app docs: [`apps/offscreen/README.md`](../apps/offscreen/README.md)
