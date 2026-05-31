#include "core/CheckedArithmetic.h"
#include "render/GpuTypes.h"
#include "vulkan/pipelines/Pipeline.h"

#include <gtest/gtest.h>

#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

// The authoritative ABI checks are the static_asserts that live next to each
// struct definition (compiled with the core library). These runtime checks
// duplicate them so the contract is visible and exercised as an explicit test.

TEST(AbiContracts, SceneVertexLayout) {
    EXPECT_EQ(sizeof(vkgs::render::SceneVertex), 240u);
    EXPECT_EQ(offsetof(vkgs::render::SceneVertex, position), 0u);
    EXPECT_EQ(offsetof(vkgs::render::SceneVertex, scaleOpacity), 16u);
    EXPECT_EQ(offsetof(vkgs::render::SceneVertex, rotation), 32u);
    EXPECT_EQ(offsetof(vkgs::render::SceneVertex, shs), 48u);
}

TEST(AbiContracts, Cov3DLayout) {
    EXPECT_EQ(sizeof(vkgs::render::Cov3DUpperRight), 24u);
}

TEST(AbiContracts, DescriptorOptionRejectsMissingEntry) {
    Pipeline::DescriptorOption option(std::vector<uint32_t>{0});
    EXPECT_THROW((void)option.get(1), std::runtime_error);
}

TEST(AbiContracts, CheckedArithmeticRejectsOverflow) {
    constexpr auto max = std::numeric_limits<uint64_t>::max();
    EXPECT_THROW(vkgs::core::checkedAdd(max, 1, "test add"), std::overflow_error);
    EXPECT_THROW(vkgs::core::checkedMultiply(max, 2, "test multiply"), std::overflow_error);
    EXPECT_THROW(vkgs::core::checkedNarrowToUint32(max, "test narrow"), std::overflow_error);
}

TEST(AbiContracts, UniformBufferLayout) {
    EXPECT_EQ(sizeof(vkgs::render::UniformBuffer), 176u);
    EXPECT_EQ(offsetof(vkgs::render::UniformBuffer, projection), 16u);
    EXPECT_EQ(offsetof(vkgs::render::UniformBuffer, view), 80u);
    EXPECT_EQ(offsetof(vkgs::render::UniformBuffer, width), 144u);
    EXPECT_EQ(offsetof(vkgs::render::UniformBuffer, nearPlane), 160u);
}

TEST(AbiContracts, VertexAttributeLayout) {
    EXPECT_EQ(sizeof(vkgs::render::VertexAttribute), 64u);
    EXPECT_EQ(offsetof(vkgs::render::VertexAttribute, depth), 56u);
}

TEST(AbiContracts, RadixSortPushConstantsLayout) {
    EXPECT_EQ(sizeof(vkgs::render::RadixSortPushConstants), 16u);
}
