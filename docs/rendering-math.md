# Rendering Math

This page explains the math implemented by the current forward renderer. It is
not a full 3DGS derivation. It focuses on the formulas that appear in CPU scene
conversion and the GLSL compute passes.

## Notation

- `p`: Gaussian center in world coordinates.
- `s`: positive per-axis scale.
- `q`: normalized rotation quaternion in `[w, x, y, z]` order.
- `R(q)`: 3x3 rotation matrix built from `q`.
- `Sigma`: 3D covariance.
- `J`: approximate perspective projection Jacobian.
- `C`: 2D screen-space covariance.
- `Q = C^-1`: conic matrix used during pixel evaluation.
- `T`: accumulated transmittance in front-to-back alpha blending.

## Training Values To Render Values

PLY values are stored in training parameter space. The CPU converts them before
upload:

```text
s = exp(scale)
opacity = 1 / (1 + exp(-opacity_logit))
q = normalize(q_raw)
```

The renderer stores opacity next to scale in `scaleOpacity.w`, so all GPU passes
read the opacity as an already-squashed alpha scale.

Spherical harmonic coefficients are stored as 16 RGB vectors. The input PLY
stores `f_dc_0..2` first, then `f_rest_0..44`. The loader keeps the DC RGB
triple as coefficient 0 and rearranges the remaining coefficients into
coefficient-major RGB order:

```text
coeff[k].rgb = (f_rest[k-1], f_rest[k+14], f_rest[k+29]) for k = 1..15
```

## 3D Covariance

[`precomp_cov3d.comp`](../src/shaders/precomp_cov3d.comp) constructs one
object-space covariance per Gaussian. The shader builds a scale matrix `S` and
rotation matrix `R`, then computes:

```text
M = S R
Sigma = M^T M
```

Only the symmetric upper-right entries are stored:

```text
Sigma = [ xx xy xz
          xy yy yz
          xz yz zz ]

packed = [ xx, xy, xz, yy, yz, zz ]
```

The current C++ caller uses `scale_factor = 1.0`, but the shader has a push
constant for future scale adjustment.

## Camera And Projection Convention

The CPU builds a view matrix from the camera pose and stores two matrices in the
uniform buffer:

- `view`: world to view transform, with the renderer's axis adjustments applied.
- `projection`: the combined projection-view transform used directly for
  homogeneous projection in the shader.

The shader culls against view-space `z`:

```text
p_view = view * p
visible only if p_view.z > near_plane
```

Then it projects to normalized device coordinates:

```text
p_hom = projection * p
ndc = p_hom.xyz / p_hom.w
```

NDC is converted to pixel coordinates with:

```text
pixel(v, S) = ((v + 1) * S - 1) / 2
```

## Projected 2D Covariance

[`preprocess.comp`](../src/shaders/preprocess.comp) projects the 3D covariance
into screen space with an approximate Jacobian. The shader first clamps the
view-space center used for the Jacobian to a slightly expanded FOV range:

```text
x/z clamped to [-1.3 tan_fovx, 1.3 tan_fovx]
y/z clamped to [-1.3 tan_fovy, 1.3 tan_fovy]
```

The focal lengths are:

```text
f_x = width  / (2 tan_fovx)
f_y = height / (2 tan_fovy)
```

The implemented Jacobian is:

```text
J = [ f_x / z, 0,       -f_x x / z^2
      0,       f_y / z, -f_y y / z^2
      0,       0,        0           ]
```

With `W = transpose(mat3(view))`, the shader computes:

```text
T = W J
C = T^T Sigma T
C[0,0] += 0.3
C[1,1] += 0.3
```

The `0.3` diagonal term is a low-pass guard used by the reference 3DGS rasterizer
style to keep projected splats numerically stable.

## Conic Form And Radius

The renderer inverts the 2D covariance:

```text
Q = C^-1 = [ a b
             b c ]
```

It stores `(a, b, c, opacity)` as `conic_opacity`.

For a pixel offset `d = center - pixel`, the render shader evaluates:

```text
power = -0.5 (a d_x^2 + c d_y^2) - b d_x d_y
alpha = min(0.99, opacity * exp(power))
```

Positive `power` values are skipped because they would increase opacity away
from the splat center.

The preprocess shader estimates a conservative screen radius from the larger
eigenvalue of `C`:

```text
mid = 0.5 (C_xx + C_yy)
lambda1 = mid + sqrt(max(0.1, mid^2 - det(C)))
lambda2 = mid - sqrt(max(0.1, mid^2 - det(C)))
radius = ceil(3 sqrt(max(lambda1, lambda2)))
```

That radius forms a tile-space AABB. Splats with an empty tile AABB are culled.

## Spherical Harmonics Color

The preprocess shader evaluates degree-3 spherical harmonics using 16 RGB
coefficient vectors. The direction is from camera to Gaussian center:

```text
dir = normalize(p_world - camera_position)
```

The shader applies constants `SH_C0`, `SH_C1`, `SH_C2`, and `SH_C3`, adds `0.5`
to the result, then clamps each channel to be non-negative:

```text
color = max(eval_sh(dir, coeffs) + 0.5, 0)
```

This color is stored in `VertexAttribute.color_radii.xyz`; the radius is stored
in `.w`.

## Tile And Depth Key

Every visible splat expands into one item for each overlapped tile. The key is a
64-bit integer:

```text
key = (uint64(tile_index) << 32) | floatBitsToUint(view_depth)
```

Since visible depths are positive, sorting the float bit pattern orders depth
monotonically. Sorting by the full 64-bit key groups items by tile first and
orders splats within each tile from smaller positive depth to larger positive
depth.

The payload is the source Gaussian index. The final tile-boundary pass turns the
sorted key array into `[start, end)` ranges per tile.

## Alpha Compositing

The render shader composites splats front to back:

```text
T = 1
color = 0

for splat in sorted_tile_range:
    alpha = min(0.99, opacity * exp(power))
    if alpha < 1 / 255:
        continue

    next_T = T * (1 - alpha)
    if next_T < 0.0001:
        break

    color += splat_color * alpha * T
    T = next_T
```

The storage image receives `vec4(color, 1.0)`. The shader does not currently
write accumulated alpha or background color.

