#include "GSScene.h"

#include <gtest/gtest.h>

#include <cstddef>

// The authoritative ABI checks are the static_asserts that live next to each
// struct definition (compiled with the core library). These runtime checks
// duplicate them so the contract is visible and exercised as an explicit test.

TEST(AbiContracts, SceneVertexLayout) {
    EXPECT_EQ(sizeof(GSScene::Vertex), 240u);
    EXPECT_EQ(offsetof(GSScene::Vertex, position), 0u);
    EXPECT_EQ(offsetof(GSScene::Vertex, scale_opacity), 16u);
    EXPECT_EQ(offsetof(GSScene::Vertex, rotation), 32u);
    EXPECT_EQ(offsetof(GSScene::Vertex, shs), 48u);
}

TEST(AbiContracts, Cov3DLayout) {
    EXPECT_EQ(sizeof(GSScene::Cov3DUpperRight), 24u);
}

#ifdef VKGS_RENDER_MODE_OFFSCREEN
// Renderer.h is only safe to include outside ONSCREEN builds, where it would
// transitively require ImGui headers that are not on the test include path.
#include "Renderer.h"

TEST(AbiContracts, UniformBufferLayout) {
    EXPECT_EQ(sizeof(Renderer::UniformBuffer), 176u);
    EXPECT_EQ(offsetof(Renderer::UniformBuffer, proj_mat), 16u);
    EXPECT_EQ(offsetof(Renderer::UniformBuffer, view_mat), 80u);
    EXPECT_EQ(offsetof(Renderer::UniformBuffer, width), 144u);
    EXPECT_EQ(offsetof(Renderer::UniformBuffer, near_plane), 160u);
}

TEST(AbiContracts, VertexAttributeLayout) {
    EXPECT_EQ(sizeof(Renderer::VertexAttributeBuffer), 64u);
    EXPECT_EQ(offsetof(Renderer::VertexAttributeBuffer, depth), 56u);
}

TEST(AbiContracts, RadixSortPushConstantsLayout) {
    EXPECT_EQ(sizeof(Renderer::RadixSortPushConstants), 16u);
}
#endif
