# Render Shader

Source: [`src/shaders/render.comp`](../../src/shaders/render.comp)

The render shader composites sorted splats into the output storage image. It
dispatches one workgroup per tile and one invocation per pixel inside that tile.

## Inputs And Output

Inputs:

- set 0 binding 0: `VertexAttribute attr[]`
- set 0 binding 1: tile boundary ranges
- set 0 binding 2: sorted Gaussian payloads
- push constants: output `width` and `height`

Output:

- set 1 binding 0: writeonly `image2D output_image`

Dispatch shape:

- local size: `TILE_WIDTH x TILE_HEIGHT x 1`, currently `16 x 16 x 1`
- workgroups: `tileCountX(width) x tileCountY(height) x 1`

## Per-Pixel Loop

Each invocation computes its pixel coordinate from the tile ID and local
invocation ID. Pixels outside the output extent return early, which handles
partial tiles along the right and bottom edges.

The shader computes the tile's linear index and reads:

```text
start = boundaries[tile * 2 + 0]
end = boundaries[tile * 2 + 1]
```

It then iterates the sorted payload range. Every payload is a Gaussian index into
`VertexAttribute[]`.

## Gaussian Evaluation

For the current pixel, the shader computes offset from splat center:

```text
d = splat_uv - pixel_uv
```

Using stored conic coefficients `(a, b, c)`:

```text
power = -0.5 * (a * d.x^2 + c * d.y^2) - b * d.x * d.y
```

If `power > 0`, the sample is skipped. Otherwise alpha is:

```text
alpha = min(0.99, opacity * exp(power))
```

Samples below `1 / 255` alpha are skipped to avoid negligible work.

## Front-To-Back Compositing

The shader maintains transmittance `T` and color `c`:

```text
T = 1
c = 0

for payload in tile_range:
    alpha = ...
    test_T = T * (1 - alpha)
    if test_T < 0.0001:
        break
    c += splat_color * alpha * T
    T = test_T
```

The sorted key order makes this a front-to-back traversal within each tile. The
shader writes `vec4(c, 1.0)` to the storage image. It does not currently blend
against a configurable background or preserve accumulated alpha in the output.

