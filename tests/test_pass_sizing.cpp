#include "GpuConstants.h"
#include "render/PassSizing.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <stdexcept>

TEST(PassSizing, ComputesCeilingDivision) {
    EXPECT_EQ(vkgs::render::ceilDiv(0, 256), 0u);
    EXPECT_EQ(vkgs::render::ceilDiv(1, 256), 1u);
    EXPECT_EQ(vkgs::render::ceilDiv(257, 256), 2u);
    EXPECT_THROW(vkgs::render::ceilDiv(1, 0), std::runtime_error);
}

TEST(PassSizing, ComputesTileBoundaryBufferSize) {
    EXPECT_EQ(vkgs::render::tileBoundaryBytes(gpu::TileWidth + 1, gpu::TileHeight + 1), 4u * 2u * sizeof(uint32_t));
}

TEST(PassSizing, ComputesPrefixSumIterationsWithIntegerMath) {
    EXPECT_EQ(vkgs::render::prefixSumIterations(1), 0u);
    EXPECT_EQ(vkgs::render::prefixSumIterations(2), 1u);
    EXPECT_EQ(vkgs::render::prefixSumIterations(3), 2u);
    EXPECT_EQ(vkgs::render::prefixSumIterations(4), 2u);
    EXPECT_EQ(vkgs::render::prefixSumIterations(5), 3u);
    EXPECT_THROW(vkgs::render::prefixSumIterations(0), std::runtime_error);
}

TEST(PassSizing, ComputesRadixSortDispatchWorkgroups) {
    EXPECT_EQ(vkgs::render::radixSortWorkgroupCount(0, gpu::RadixBlocksPerWorkgroup), 0u);
    EXPECT_EQ(vkgs::render::radixSortWorkgroupCount(1, gpu::RadixBlocksPerWorkgroup), 1u);
    EXPECT_EQ(vkgs::render::radixSortWorkgroupCount(gpu::WorkgroupSize * gpu::RadixBlocksPerWorkgroup,
                                                    gpu::RadixBlocksPerWorkgroup),
              1u);
    EXPECT_EQ(vkgs::render::radixSortWorkgroupCount(gpu::WorkgroupSize * gpu::RadixBlocksPerWorkgroup + 1,
                                                    gpu::RadixBlocksPerWorkgroup),
              2u);
    EXPECT_THROW(vkgs::render::radixSortWorkgroupCount(1, 0), std::runtime_error);
}

TEST(PassSizing, RejectsOverflow) {
    constexpr auto max = std::numeric_limits<uint64_t>::max();
    EXPECT_THROW(vkgs::render::bytesFor(max, 2, "test bytes"), std::overflow_error);
    EXPECT_THROW(vkgs::render::sortCapacity(std::numeric_limits<uint32_t>::max(), 2), std::overflow_error);

    const auto tooManyElements = (static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()) + 1) *
                                 gpu::WorkgroupSize;
    EXPECT_THROW(vkgs::render::workgroupCount(tooManyElements), std::overflow_error);
}
