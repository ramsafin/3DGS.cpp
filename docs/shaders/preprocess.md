# Preprocess Shader

Source: [`src/shaders/preprocess.comp`](../../src/shaders/preprocess.comp)

The preprocess shader is the main per-Gaussian forward pass. It projects each
Gaussian into screen space, computes a 2D conic, evaluates color, and determines
which tiles the splat can affect.

## Inputs And Outputs

Inputs:

- set 0 binding 0: `Vertex vertices[]`
- set 0 binding 1: packed 3D covariance floats
- set 1 binding 0: camera and render parameters

Outputs:

- set 1 binding 1: `VertexAttribute attr[]`
- set 1 binding 2: `uint tiles_overlap[]`

Dispatch shape:

- local size: `TILE_WIDTH * TILE_HEIGHT`, currently `256`
- global work: one invocation per Gaussian

## Culling

Each invocation initializes the splat as invisible by setting radius and overlap
count to zero. The shader then rejects:

- indices beyond the vertex buffer length;
- centers with `p_view.z <= near_plane`;
- homogeneous projections with near-zero `w`;
- projected covariances with non-positive determinant;
- projected splats with an empty tile AABB.

The render pass later treats `color_radii.w == 0` as invisible.

## Projection And 2D Covariance

The shader computes both view-space and clip-space positions:

```text
p_view = view_mat * position
p_hom = proj_mat * position
ndc = p_hom.xyz / p_hom.w
```

`compute_cov2d()` rebuilds the symmetric 3D covariance from the packed buffer,
computes the projection Jacobian, and applies:

```text
T = transpose(mat3(view_mat)) * J
C = T^T Sigma T
C_xx += 0.3
C_yy += 0.3
```

The inverse covariance is stored as conic coefficients:

```text
conic_opacity.xyz = (C^-1_xx, C^-1_xy, C^-1_yy)
conic_opacity.w = opacity
```

## Color

`compute_sh()` evaluates degree-3 spherical harmonics from the direction
`position - camera_position`. It stores the clamped result in
`color_radii.xyz`. The loader has already rearranged SH coefficients into the
coefficient-major RGB order used by the shader.

## Radius And Tile AABB

The shader estimates a 3-sigma radius from the larger eigenvalue of the 2D
covariance:

```text
radius = ceil(3 sqrt(max(lambda1, lambda2)))
```

The projected NDC center is converted to pixel coordinates with `ndc2Pix()`.
The pixel radius is then converted into a clamped tile AABB:

```text
aabb = [min_tile_x, min_tile_y, max_tile_x, max_tile_y)
tiles_overlap = (max_tile_x - min_tile_x) * (max_tile_y - min_tile_y)
```

Visible splats write their AABB, overlap count, depth, radius, color, UV center,
and debug magic value. The prefix-sum pass consumes `tiles_overlap[]`, and later
passes consume `VertexAttribute[]`.

