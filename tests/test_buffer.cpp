#include "test_support.h"

#include "GSScene.h"
#include "vulkan/Buffer.h"
#include "vulkan/VulkanContext.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <numeric>
#include <string>
#include <vector>

namespace {

class BufferTest : public ::testing::Test {
  protected:
    void SetUp() override {
        context = vkgs_test::makeHeadlessContext();
        if (!context) {
            GTEST_SKIP() << "No Vulkan device available";
        }
    }
    std::shared_ptr<VulkanContext> context;
};

TEST_F(BufferTest, FullUploadAndElementReadback) {
    constexpr uint32_t count = 16;
    auto buffer = Buffer::storage(context, count * sizeof(uint32_t));

    std::vector<uint32_t> data(count);
    std::iota(data.begin(), data.end(), 0u);
    buffer->upload(data.data(), count * sizeof(uint32_t));

    EXPECT_EQ(buffer->readOne<uint32_t>(0), 0u);
    EXPECT_EQ(buffer->readOne<uint32_t>(5 * sizeof(uint32_t)), 5u);   // middle
    EXPECT_EQ(buffer->readOne<uint32_t>(15 * sizeof(uint32_t)), 15u); // final element
}

TEST_F(BufferTest, PartialUploadWritesDestinationOffset) {
    constexpr uint32_t count = 16;
    auto buffer = Buffer::storage(context, count * sizeof(uint32_t));

    std::vector<uint32_t> data(count, 0u);
    buffer->upload(data.data(), count * sizeof(uint32_t));

    const uint32_t value = 99u;
    buffer->upload(&value, sizeof(uint32_t), 8 * sizeof(uint32_t));

    EXPECT_EQ(buffer->readOne<uint32_t>(8 * sizeof(uint32_t)), 99u);
    EXPECT_EQ(buffer->readOne<uint32_t>(7 * sizeof(uint32_t)), 0u);
    EXPECT_EQ(buffer->readOne<uint32_t>(9 * sizeof(uint32_t)), 0u);
}

TEST_F(BufferTest, DownloadAppliesSourceOffset) {
    constexpr uint32_t count = 16;
    auto buffer = Buffer::storage(context, count * sizeof(uint32_t));
    std::vector<uint32_t> data(count);
    std::iota(data.begin(), data.end(), 0u);
    buffer->upload(data.data(), count * sizeof(uint32_t));

    auto staging = Buffer::staging(context, sizeof(uint32_t));
    buffer->downloadTo(staging, 10 * sizeof(uint32_t), 0);
    EXPECT_EQ(*static_cast<uint32_t*>(staging->allocation_info.pMappedData), 10u);
}

TEST_F(BufferTest, ReallocPreservesUsability) {
    auto buffer = Buffer::storage(context, 4 * sizeof(uint32_t));
    std::vector<uint32_t> small(4, 1u);
    buffer->upload(small.data(), 4 * sizeof(uint32_t));

    buffer->realloc(32 * sizeof(uint32_t));
    std::vector<uint32_t> big(32);
    std::iota(big.begin(), big.end(), 100u);
    buffer->upload(big.data(), 32 * sizeof(uint32_t));

    EXPECT_EQ(buffer->readOne<uint32_t>(0), 100u);
    EXPECT_EQ(buffer->readOne<uint32_t>(31 * sizeof(uint32_t)), 131u);
}

TEST_F(BufferTest, EmptySceneIsRejected) {
    const std::string fixture = std::string(VKGS_FIXTURE_DIR) + "/zero_vertex.ply";
    GSScene scene(fixture);
    EXPECT_THROW(scene.load(context), std::runtime_error);
}

TEST_F(BufferTest, SchemaMismatchIsRejected) {
    // valid_tiny.ply has 3 vertices but only 10 properties, which does not match
    // the native 62-float vertex layout (VKGS-007).
    const std::string fixture = std::string(VKGS_FIXTURE_DIR) + "/valid_tiny.ply";
    GSScene scene(fixture);
    EXPECT_THROW(scene.load(context), std::runtime_error);
}

} // namespace
