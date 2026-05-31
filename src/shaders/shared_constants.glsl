// Single source of truth for constants shared across CPU dispatch code and
// GLSL shaders (VKGS-014). This file contains ONLY preprocessor #define lines
// so it is valid both as a GLSL include and as a C++ preprocessor include
// (see src/GpuConstants.hpp, which wraps these into typed constexpr values).
#ifndef VKGS_SHARED_CONSTANTS_GLSL
#define VKGS_SHARED_CONSTANTS_GLSL

#define VKGS_TILE_WIDTH 16
#define VKGS_TILE_HEIGHT 16

// Generic compute workgroup size used by per-splat dispatches.
#define VKGS_WORKGROUP_SIZE 256

// Spherical-harmonic storage: 16 coefficient vectors of 3 floats = 48 floats.
#define VKGS_SH_COEFF_VECTORS 16
#define VKGS_SH_MAX_COEFFS 48

// Radix sort configuration.
#define VKGS_RADIX_SORT_BINS 256
#define VKGS_RADIX_BLOCKS_PER_WORKGROUP 32

#endif // VKGS_SHARED_CONSTANTS_GLSL
