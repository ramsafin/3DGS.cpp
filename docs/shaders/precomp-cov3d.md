# Covariance Precompute Shader

Source: [`src/shaders/precomp_cov3d.comp`](../../src/shaders/precomp_cov3d.comp)

This shader runs once after scene upload. It converts each Gaussian's render
scale and rotation into a packed 3D covariance used by the camera-dependent
preprocess pass.

## Inputs And Outputs

Inputs:

- binding 0: readonly `Vertex vertices[]`
- push constant: `float scale_factor`

Output:

- binding 1: writeonly `float cov3ds[]`, six floats per Gaussian

Dispatch shape:

- local size: `256 x 1 x 1`
- global work: one invocation per Gaussian

## Formula

For each Gaussian:

```text
S = diag(scale.x * scale_factor,
         scale.y * scale_factor,
         scale.z * scale_factor)

R = rotationFromQuaternion(rotation)
M = S R
Sigma = M^T M
```

The shader stores the symmetric covariance in upper-right packed order:

```text
cov3ds[i * 6 + 0] = Sigma_xx
cov3ds[i * 6 + 1] = Sigma_xy
cov3ds[i * 6 + 2] = Sigma_xz
cov3ds[i * 6 + 3] = Sigma_yy
cov3ds[i * 6 + 4] = Sigma_yz
cov3ds[i * 6 + 5] = Sigma_zz
```

## Integration Notes

`GpuScene::precomputeCov3D()` creates this pipeline, binds the uploaded vertex
buffer and covariance output buffer, pushes `scale_factor = 1.0`, dispatches,
and waits through the one-time command helper.

Because covariance only depends on scale and rotation, the current forward
renderer can compute it once per loaded scene. A future training path must
recompute or update this buffer whenever scale or rotation changes.

