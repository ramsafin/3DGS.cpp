# Off-screen renderer

`3dgs_render` is built when configuring the project with `-DVKGS_RENDER_MODE=OFFSCREEN`.
It reads a JSON render configuration, renders each requested camera pose into an off-screen
Vulkan storage image, and writes dependency-free binary PPM images.

## Usage

```bash
3dgs_render --config render.json [--output renders] [--device 0] [--validation] [--verbose]
```

## JSON schema

```json
{
  "scene": "data/scene.ply",
  "output": {
    "directory": "renders",
    "filename_pattern": "frame_%04d.ppm",
    "format": "ppm"
  },
  "render": {
    "width": 1280,
    "height": 720,
    "fov_degrees": 45.0,
    "near": 0.2,
    "far": 1000.0
  },
  "vulkan": {
    "validation": false,
    "physical_device": 0
  },
  "frames": [
    {
      "name": "front",
      "position": [0.0, 0.0, 0.0],
      "rotation_quat": [1.0, 0.0, 0.0, 0.0]
    }
  ]
}
```

### Fields

- `scene`: required path to a Gaussian Splatting PLY file. Relative paths are resolved from the config file directory.
- `output.directory`: output directory. Relative paths are resolved from the config file directory.
- `output.filename_pattern`: optional pattern. `printf`-style integer patterns such as `frame_%04d.ppm` are supported, as are `{index}` and `{name}` replacements.
- `output.format`: currently only `ppm` is supported.
- `render.width` / `render.height`: output dimensions. Defaults are `1280x720`.
- `render.fov_degrees`, `render.near`, `render.far`: default projection settings for all frames.
- `vulkan.validation`: enables Vulkan validation layers.
- `vulkan.physical_device`: optional physical device index.
- `frames`: non-empty array of camera poses.
- `frames[].position`: camera position `[x, y, z]`.
- `frames[].rotation_quat`: camera quaternion in `[w, x, y, z]` order.
- `frames[].fov_degrees`, `frames[].near`, `frames[].far`: optional per-frame projection overrides.
