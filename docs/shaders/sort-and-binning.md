# Sort And Binning Shaders

Sources:

- [`preprocess_sort.comp`](../../src/shaders/preprocess_sort.comp)
- [`sort/hist.comp`](../../src/shaders/sort/hist.comp)
- [`sort/sort.comp`](../../src/shaders/sort/sort.comp)
- [`sort/sort_portable.comp`](../../src/shaders/sort/sort_portable.comp)
- [`tile_boundary.comp`](../../src/shaders/tile_boundary.comp)

These shaders convert visible splats into sorted per-tile instance lists.

## Expansion

`preprocess_sort.comp` reads `VertexAttribute[]` and the selected prefix-sum
buffer. Invisible splats, identified by `color_radii.w == 0`, emit no items.

For every tile in a visible splat's AABB, it writes:

```text
tile_index = tile_x + tile_y * tile_count_x
depth_bits = floatBitsToUint(view_depth)
key = (uint64(tile_index) << 32) | uint64(depth_bits)
payload = gaussian_index
```

The high key bits group instances by tile. The low key bits sort visible splats
by positive view-space depth inside each tile.

## Radix Histogram

`sort/hist.comp` processes one byte of the 64-bit key per radix pass. It builds
a 256-bin histogram for each workgroup:

```text
bin = (key >> shift) & 255
```

The renderer runs eight passes with shifts `0, 8, 16, ..., 56`.

## Radix Scatter

The fast shader, `sort/sort.comp`, uses subgroup operations and 64-bit shared
atomics. The portable shader, `sort_portable.comp`, avoids those subgroup and
64-bit atomic requirements while preserving the same buffer contract.

Both shaders:

1. Compute global offsets for each bin from the per-workgroup histograms.
2. Build local bin flags for the current workgroup block.
3. Compute each element's prefix within its bin.
4. Scatter key and payload to the output buffers.
5. Advance the bin offset after the last element in a local bin group.

After eight passes over a 64-bit key, the sorted result is back in the even key
and payload buffers. `Renderer::recordRenderCommandBuffer()` binds those even
buffers for boundary construction and rendering.

## Tile Boundaries

`tile_boundary.comp` reads sorted keys and writes two unsigned integers per tile:

```text
boundaries[tile * 2 + 0] = start
boundaries[tile * 2 + 1] = end
```

The renderer clears the boundary buffer before dispatch. The shader writes a
start when it sees the first instance for a tile and writes the previous tile's
end when the key changes. The final invocation closes the last non-empty tile.

Empty tiles remain `[0, 0)`, so `render.comp` naturally skips them.

