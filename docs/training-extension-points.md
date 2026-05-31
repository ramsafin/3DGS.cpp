# Training Extension Points

This repository does not currently implement training. It is a forward renderer
for already trained 3D Gaussian Splatting PLY files. There is no optimizer, loss
function, camera dataset loader, differentiable backward pass, or gradient
storage.

This page describes practical future extension points for adding a training
pipeline while preserving the current renderer.

## Current Forward-Only Boundaries

The renderer assumes scene parameters are fixed after load:

- `PlyReader` converts disk parameters into render-space `SceneVertex` values.
- `GpuScene` uploads immutable vertex storage and precomputes covariance once.
- `Renderer` owns one scene and records scene-dependent preprocess work.
- Render passes output a storage image, not intermediate training buffers.
- Sort and tile range buffers are temporary acceleration data, not a
  differentiable API contract.

Training support should avoid overloading this path with optimizer state. The
cleanest direction is to keep the public viewer/offscreen APIs forward-only and
add a separate training-oriented scene and execution layer.

## Parameter Ownership

A training pipeline should store learnable values in training space:

- position `p`;
- log-scale values, not exponentiated scale;
- opacity logits, not squashed opacity;
- rotation parameters with explicit normalization policy;
- SH coefficients in a documented layout;
- optional optimizer slots such as moments, learning rates, and masks.

The forward renderer can continue consuming render-space data. A training layer
can either materialize a render-space `SceneVertex` buffer before each forward
pass or introduce mutable GPU buffers that the existing shaders read through the
same ABI after a parameter-conversion pass.

Recommended future abstraction:

```text
GaussianParameterSet
  training buffers: position, log_scale, opacity_logit, rotation, sh
  render buffers: SceneVertex-compatible packed view
  optimizer buffers: moments, gradients, masks
```

This keeps serialization, optimizer state, and rendering ABI concerns separate.

## Forward Pass Reuse

The existing forward passes are useful for training because they already produce
the same visibility, binning, and compositing decisions used by inference:

1. Convert training parameters into render-space buffers.
2. Recompute covariance when scale or rotation changes.
3. Run preprocess, prefix sum, expansion, sort, tile boundaries, and render.
4. Preserve selected intermediates needed by backward passes.

Backward support will need additional retained buffers. At minimum:

- projected center, depth, radius, conic, opacity, and color per Gaussian;
- tile instance ranges and sorted payloads;
- per-pixel accumulated transmittance or enough data to recompute it;
- image residuals or loss gradients.

The current `VertexAttribute` buffer already stores many forward intermediates,
but it is not sufficient by itself for all gradients.

## Backward Pass Work Items

A differentiable renderer needs gradient paths for:

- compositing: color, opacity, alpha, and transmittance;
- conic evaluation: inverse covariance entries and screen-space center;
- 2D covariance projection back to 3D covariance;
- 3D covariance back to scale and rotation;
- spherical harmonics color back to SH coefficients and viewing direction;
- screen projection back to Gaussian position and camera parameters if cameras
  are trainable.

Sorting and tile assignment are discrete. A first implementation should treat
the sorted visibility structure as fixed for the backward pass of a forward
iteration, matching common differentiable rasterization practice. Gradients can
then scatter back from sorted tile instances to per-Gaussian gradient buffers.

## API And Execution Model

Add training as a separate subsystem rather than expanding
`vkgs::OffscreenRenderer`:

```text
TrainingSession
  owns GaussianParameterSet
  owns camera batches and target images
  runs forward render batches
  runs loss and backward passes
  steps optimizer
  exports trained PLY checkpoints
```

For batching, start with one camera per submitted render, matching the current
renderer. Multi-camera batching can be added later by indexing per-camera
uniforms, output images, and loss buffers. This avoids forcing the existing
viewer/offscreen path to understand training datasets.

## Loss And Optimizer Hooks

Loss computation should be pluggable but GPU-resident:

- L1 or L2 image loss is the simplest first target.
- SSIM or DSSIM can be added after image-space gradients and reductions are in
  place.
- Optimizer state should live in dedicated buffers, likely one buffer family per
  parameter group.

The optimizer should operate on training-space parameters, then run a conversion
or normalization step before the next forward pass. Rotation normalization and
opacity/scale transforms should be documented as part of this step.

## Compatibility Constraints

Future training work should preserve these forward-renderer contracts unless an
intentional API migration is planned:

- Keep `SceneVertex`, `UniformBuffer`, and `VertexAttribute` ABI changes guarded
  by static assertions and tests.
- Keep shader constants shared through `shared_constants.glsl`.
- Keep viewer and offscreen rendering able to load trained PLY files without
  training dependencies.
- Keep generated SPIR-V embedding deterministic.
- Keep raw COLMAP/Open3D point-cloud ingestion separate from trained Gaussian
  PLY ingestion.

## Suggested First Milestone

A practical first milestone is not full training. It is a training-ready forward
path:

1. Add a mutable GPU scene representation that can refresh render-space
   `SceneVertex` data from training-space buffers.
2. Keep the existing forward pass sequence and image output unchanged.
3. Expose selected intermediate buffers to a future backward module.
4. Add tests that compare immutable PLY rendering and mutable training-buffer
   rendering for the same Gaussian parameters.

