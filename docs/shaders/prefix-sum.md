# Prefix Sum Shader

Source: [`src/shaders/prefix_sum.comp`](../../src/shaders/prefix_sum.comp)

The prefix-sum shader turns per-Gaussian tile overlap counts into offsets for
the splat-tile expansion pass.

## Inputs And Outputs

Inputs and outputs are the same two buffers, used as ping/pong storage:

- set 0 binding 0: `uint src[]`
- set 0 binding 1: `uint dst[]`
- push constant: `uint timestep`

Dispatch shape:

- local size: `256 x 1 x 1`
- global work: one invocation per Gaussian

## Algorithm

Before dispatch, the renderer copies `tileOverlapBuffer` into the prefix-sum
ping buffer. The shader then performs an inclusive scan over multiple timesteps.
For each index:

```text
offset = 2^timestep

if index < offset:
    output[index] = input[index]
else:
    output[index] = input[index] + input[index - offset]
```

Even timesteps read from `src` and write `dst`; odd timesteps read from `dst`
and write `src`. The renderer inserts barriers between timesteps and selects the
prefix-sum buffer variant by iteration parity for later expansion.

## Result Use

For splat index `i`, the final inclusive scan value is the end offset of that
splat's expanded tile-instance range. `preprocess_sort.comp` derives the start
offset as:

```text
start = (i == 0) ? 0 : prefixSum[i - 1]
end = prefixSum[i]
```

The renderer also copies the final value for the last Gaussian into a staging
buffer. That value is the total expanded instance count. If it exceeds the
current sort buffer capacity, the renderer grows the sort buffers and records
the preprocess work again.

