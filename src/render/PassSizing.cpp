#include "PassSizing.h"

#include "GpuConstants.h"
#include "core/CheckedArithmetic.h"

#include <stdexcept>

namespace vkgs::render {

uint32_t ceilDiv(uint32_t numerator, uint32_t denominator) {
    if (denominator == 0) {
        throw std::runtime_error("Cannot divide by zero");
    }
    return numerator / denominator + (numerator % denominator != 0);
}

uint32_t tileCountX(uint32_t width) {
    return ceilDiv(width, gpu::TileWidth);
}

uint32_t tileCountY(uint32_t height) {
    return ceilDiv(height, gpu::TileHeight);
}

uint32_t workgroupCount(uint64_t elementCount) {
    return vkgs::core::checkedNarrowToUint32(elementCount / gpu::WorkgroupSize +
                                                 (elementCount % gpu::WorkgroupSize != 0),
                                             "Compute workgroup count");
}

uint32_t prefixSumIterations(uint64_t elementCount) {
    if (elementCount == 0) {
        throw std::runtime_error("Prefix sum requires at least one element");
    }

    uint32_t iterations = 0;
    auto highestIndex = elementCount - 1;
    while (highestIndex > 0) {
        highestIndex >>= 1;
        ++iterations;
    }
    return iterations;
}

uint32_t radixSortWorkgroupCount(uint32_t elementCount, uint32_t blocksPerWorkgroup) {
    return workgroupCount(ceilDiv(elementCount, blocksPerWorkgroup));
}

uint64_t bytesFor(uint64_t count, uint64_t elementSize, std::string_view context) {
    return vkgs::core::checkedMultiply(count, elementSize, context);
}

uint32_t sortCapacity(uint64_t vertexCount, uint32_t multiplier) {
    return vkgs::core::checkedNarrowToUint32(
        vkgs::core::checkedMultiply(vertexCount, multiplier, "Sort element capacity"), "Sort element capacity");
}

uint64_t sortHistogramBytes(uint32_t workgroupCount) {
    return bytesFor(bytesFor(workgroupCount, gpu::RadixSortBins, "Sort histogram entry count"), sizeof(uint32_t),
                    "Sort histogram buffer size");
}

uint64_t tileBoundaryBytes(uint32_t width, uint32_t height) {
    const auto tileCount = vkgs::core::checkedNarrowToUint32(
        vkgs::core::checkedMultiply(tileCountX(width), tileCountY(height), "Tile count"), "Tile count");
    return bytesFor(tileCount, 2 * sizeof(uint32_t), "Tile boundary buffer size");
}

} // namespace vkgs::render
