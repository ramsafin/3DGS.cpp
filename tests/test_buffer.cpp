#include "test_support.h"

#include "scene/GpuScene.h"
#include "scene/PlyReader.h"
#include "vulkan/Buffer.h"
#include "vulkan/DescriptorSet.h"
#include "vulkan/VulkanContext.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <limits>
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
    buffer->upload(std::span(data));

    EXPECT_EQ(buffer->readOne<uint32_t>(0), 0u);
    EXPECT_EQ(buffer->readOne<uint32_t>(5 * sizeof(uint32_t)), 5u);   // middle
    EXPECT_EQ(buffer->readOne<uint32_t>(15 * sizeof(uint32_t)), 15u); // final element
}

TEST_F(BufferTest, PartialUploadWritesDestinationOffset) {
    constexpr uint32_t count = 16;
    auto buffer = Buffer::storage(context, count * sizeof(uint32_t));

    std::vector<uint32_t> data(count, 0u);
    buffer->upload(std::span(data));

    const uint32_t value = 99u;
    buffer->uploadObject(value, 8 * sizeof(uint32_t));

    EXPECT_EQ(buffer->readOne<uint32_t>(8 * sizeof(uint32_t)), 99u);
    EXPECT_EQ(buffer->readOne<uint32_t>(7 * sizeof(uint32_t)), 0u);
    EXPECT_EQ(buffer->readOne<uint32_t>(9 * sizeof(uint32_t)), 0u);
}

TEST_F(BufferTest, DownloadAppliesSourceOffset) {
    constexpr uint32_t count = 16;
    auto buffer = Buffer::storage(context, count * sizeof(uint32_t));
    std::vector<uint32_t> data(count);
    std::iota(data.begin(), data.end(), 0u);
    buffer->upload(std::span(data));

    auto staging = Buffer::staging(context, sizeof(uint32_t));
    buffer->downloadTo(staging, 10 * sizeof(uint32_t), 0, sizeof(uint32_t));
    EXPECT_EQ(*static_cast<uint32_t*>(staging->allocation_info.pMappedData), 10u);
}

TEST_F(BufferTest, ExactBoundaryUploadIsAccepted) {
    auto buffer = Buffer::storage(context, 4 * sizeof(uint32_t));
    const uint32_t value = 77u;

    EXPECT_NO_THROW(buffer->uploadObject(value, 3 * sizeof(uint32_t)));
    EXPECT_EQ(buffer->readOne<uint32_t>(3 * sizeof(uint32_t)), value);
}

TEST_F(BufferTest, ExplicitPartialDownloadLengthIsApplied) {
    auto buffer = Buffer::storage(context, 4 * sizeof(uint32_t));
    const std::vector<uint32_t> data = {10u, 20u, 30u, 40u};
    buffer->upload(std::span(data));

    auto staging = Buffer::staging(context, 4 * sizeof(uint32_t));
    const std::vector<uint32_t> initial(4, 99u);
    staging->upload(std::span(initial));
    buffer->downloadTo(staging, sizeof(uint32_t), sizeof(uint32_t), 2 * sizeof(uint32_t));

    EXPECT_EQ(staging->readOne<uint32_t>(0), 99u);
    EXPECT_EQ(staging->readOne<uint32_t>(sizeof(uint32_t)), 20u);
    EXPECT_EQ(staging->readOne<uint32_t>(2 * sizeof(uint32_t)), 30u);
    EXPECT_EQ(staging->readOne<uint32_t>(3 * sizeof(uint32_t)), 99u);
}

TEST_F(BufferTest, OverflowingRangeIsRejected) {
    auto buffer = Buffer::storage(context, sizeof(uint32_t));
    const uint32_t value = 1u;

    EXPECT_THROW(buffer->uploadObject(value, std::numeric_limits<vk::DeviceSize>::max()),
                 std::runtime_error);
}

TEST_F(BufferTest, ReallocPreservesUsability) {
    auto buffer = Buffer::storage(context, 4 * sizeof(uint32_t));
    std::vector<uint32_t> small(4, 1u);
    buffer->upload(std::span(small));

    buffer->realloc(32 * sizeof(uint32_t));
    std::vector<uint32_t> big(32);
    std::iota(big.begin(), big.end(), 100u);
    buffer->upload(std::span(big));

    EXPECT_EQ(buffer->readOne<uint32_t>(0), 100u);
    EXPECT_EQ(buffer->readOne<uint32_t>(31 * sizeof(uint32_t)), 131u);
}

TEST_F(BufferTest, FailedReallocPreservesUsability) {
    auto buffer = Buffer::storage(context, sizeof(uint32_t));
    const uint32_t before = 17u;
    buffer->uploadObject(before);

    EXPECT_THROW(buffer->realloc(0), std::runtime_error);
    EXPECT_EQ(buffer->readOne<uint32_t>(), before);
}

TEST_F(BufferTest, ReallocRefreshesBuiltDescriptor) {
    auto buffer = Buffer::storage(context, sizeof(uint32_t));
    auto descriptorSet = std::make_shared<DescriptorSet>(context, 1);
    descriptorSet->bindBufferToDescriptorSet(0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                             buffer);
    descriptorSet->build();

    EXPECT_NO_THROW(buffer->realloc(2 * sizeof(uint32_t)));
    EXPECT_THROW(descriptorSet->getDescriptorSet(0, 1), std::runtime_error);
    EXPECT_THROW(descriptorSet->getDescriptorSet(1, 0), std::runtime_error);
    const uint32_t value = 23u;
    buffer->uploadObject(value, sizeof(uint32_t));
    EXPECT_EQ(buffer->readOne<uint32_t>(sizeof(uint32_t)), value);
}

TEST_F(BufferTest, InvalidDescriptorBacklinkRejectsRealloc) {
    auto buffer = Buffer::storage(context, sizeof(uint32_t));
    auto descriptorSet = std::make_shared<DescriptorSet>(context, 1);
    buffer->boundToDescriptorSet(descriptorSet, 1, 0, vk::DescriptorType::eStorageBuffer);

    EXPECT_THROW(buffer->realloc(2 * sizeof(uint32_t)), std::runtime_error);
    const uint32_t value = 29u;
    buffer->uploadObject(value);
    EXPECT_EQ(buffer->readOne<uint32_t>(), value);
}

TEST_F(BufferTest, EmptySceneIsRejected) {
    const std::string fixture = std::string(VKGS_FIXTURE_DIR) + "/zero_vertex.ply";
    EXPECT_THROW((void)vkgs::scene::PlyReader(fixture).read(), std::runtime_error);
}

TEST_F(BufferTest, SchemaMismatchIsRejected) {
    // valid_tiny.ply has 3 vertices but only 10 properties, which does not match
    // the explicit 62-float disk layout (VKGS-007).
    const std::string fixture = std::string(VKGS_FIXTURE_DIR) + "/valid_tiny.ply";
    EXPECT_THROW((void)vkgs::scene::PlyReader(fixture).read(), std::runtime_error);
}

TEST_F(BufferTest, TruncatedPlyPayloadIsRejected) {
    const std::string path = std::string(VKGS_GOLDEN_DIR) + "/truncated_payload.ply";
    vkgs_test::PlyVertexRecord record{};
    record.rotation[3] = 1.0f;
    vkgs_test::writeBinaryPly(path, {record});
    std::filesystem::resize_file(path, std::filesystem::file_size(path) - sizeof(float));

    EXPECT_THROW((void)vkgs::scene::PlyReader(path).read(), std::runtime_error);
}

TEST_F(BufferTest, CompletePlyDiskRecordLoads) {
    const std::string path = std::string(VKGS_GOLDEN_DIR) + "/complete_payload.ply";
    vkgs_test::PlyVertexRecord record{};
    record.rotation[3] = 1.0f;
    vkgs_test::writeBinaryPly(path, {record});

    vkgs::scene::GpuScene scene(vkgs::scene::PlyReader(path).read());
    EXPECT_NO_THROW(scene.upload(context));
}

} // namespace
