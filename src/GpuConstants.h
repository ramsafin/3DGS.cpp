#ifndef VKGS_GPU_CONSTANTS_H
#define VKGS_GPU_CONSTANTS_H

#include <cstdint>

// Pull in the shared #define constants that GLSL shaders also consume, then
// expose them as typed constexpr values and undefine the macros so they do not
// leak into the rest of the C++ translation unit (VKGS-014).
#include "shaders/shared_constants.glsl"

namespace gpu {
constexpr uint32_t TileWidth = VKGS_TILE_WIDTH;
constexpr uint32_t TileHeight = VKGS_TILE_HEIGHT;
constexpr uint32_t WorkgroupSize = VKGS_WORKGROUP_SIZE;
constexpr uint32_t ShCoeffVectors = VKGS_SH_COEFF_VECTORS;
constexpr uint32_t ShMaxCoeffs = VKGS_SH_MAX_COEFFS;
constexpr uint32_t RadixSortBins = VKGS_RADIX_SORT_BINS;
constexpr uint32_t RadixBlocksPerWorkgroup = VKGS_RADIX_BLOCKS_PER_WORKGROUP;

static_assert(ShCoeffVectors * 3 == ShMaxCoeffs, "SH coefficient vector count must match the flat SH array size");
} // namespace gpu

#undef VKGS_TILE_WIDTH
#undef VKGS_TILE_HEIGHT
#undef VKGS_WORKGROUP_SIZE
#undef VKGS_SH_COEFF_VECTORS
#undef VKGS_SH_MAX_COEFFS
#undef VKGS_RADIX_SORT_BINS
#undef VKGS_RADIX_BLOCKS_PER_WORKGROUP

#endif // VKGS_GPU_CONSTANTS_H
