#pragma once

#include <cstdint>
#include <string_view>

namespace vkgs::render {

uint32_t ceilDiv(uint32_t numerator, uint32_t denominator);
uint32_t tileCountX(uint32_t width);
uint32_t tileCountY(uint32_t height);
uint32_t workgroupCount(uint64_t elementCount);
uint32_t prefixSumIterations(uint64_t elementCount);
uint32_t radixSortWorkgroupCount(uint32_t elementCount, uint32_t blocksPerWorkgroup);
uint64_t bytesFor(uint64_t count, uint64_t elementSize, std::string_view context);
uint32_t sortCapacity(uint64_t vertexCount, uint32_t multiplier);
uint64_t sortHistogramBytes(uint32_t workgroupCount);
uint64_t tileBoundaryBytes(uint32_t width, uint32_t height);

} // namespace vkgs::render
