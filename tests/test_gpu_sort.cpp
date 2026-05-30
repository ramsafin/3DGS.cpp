#include "test_support.h"

#include "shaders.h"
#include "vulkan/Buffer.h"
#include "vulkan/DescriptorSet.h"
#include "vulkan/Shader.h"
#include "vulkan/VulkanContext.h"
#include "vulkan/pipelines/ComputePipeline.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <numeric>
#include <random>
#include <vector>

namespace {

// Mirrors Renderer::RadixSortPushConstants without pulling in Renderer.h
// (which is mode-dependent on ImGui headers).
struct RadixSortPushConstants {
    uint32_t g_num_elements;
    uint32_t g_shift;
    uint32_t g_num_workgroups;
    uint32_t g_num_blocks_per_workgroup;
};

constexpr uint32_t kBlocksPerWorkgroup = 32;

// Runs the GPU radix sort exactly as Renderer does: eight 8-bit passes over
// 64-bit keys, ping-ponging even/odd buffers. Returns the sorted keys and the
// permuted payloads. Designed for element counts that fit a single workgroup.
struct SortResult {
    std::vector<uint64_t> keys;
    std::vector<uint32_t> payloads;
};

SortResult runGpuSort(const std::shared_ptr<VulkanContext>& context, const std::vector<uint64_t>& inputKeys) {
    const uint32_t n = static_cast<uint32_t>(inputKeys.size());

    auto keyEven = Buffer::storage(context, n * sizeof(uint64_t));
    auto keyOdd = Buffer::storage(context, n * sizeof(uint64_t));
    auto payEven = Buffer::storage(context, n * sizeof(uint32_t));
    auto payOdd = Buffer::storage(context, n * sizeof(uint32_t));

    uint32_t globalInvocationSize = n / kBlocksPerWorkgroup + (n % kBlocksPerWorkgroup ? 1 : 0);
    uint32_t numWorkgroups = (globalInvocationSize + 255) / 256;
    auto hist = Buffer::storage(context, numWorkgroups * 256 * sizeof(uint32_t));

    std::vector<uint32_t> payloads(n);
    std::iota(payloads.begin(), payloads.end(), 0u);
    keyEven->upload(inputKeys.data(), static_cast<uint32_t>(n * sizeof(uint64_t)));
    payEven->upload(payloads.data(), static_cast<uint32_t>(n * sizeof(uint32_t)));

    using vk::DescriptorType;
    using vk::ShaderStageFlagBits;

    auto histSet = std::make_shared<DescriptorSet>(context, 1);
    histSet->bindBufferToDescriptorSet(0, DescriptorType::eStorageBuffer, ShaderStageFlagBits::eCompute, keyEven);
    histSet->bindBufferToDescriptorSet(0, DescriptorType::eStorageBuffer, ShaderStageFlagBits::eCompute, keyOdd);
    histSet->bindBufferToDescriptorSet(1, DescriptorType::eStorageBuffer, ShaderStageFlagBits::eCompute, hist);
    histSet->build();

    auto histPipeline =
        std::make_shared<ComputePipeline>(context, std::make_shared<Shader>(context, "hist", SPV_HIST, SPV_HIST_len));
    histPipeline->addDescriptorSet(0, histSet);
    histPipeline->addPushConstant(ShaderStageFlagBits::eCompute, 0, sizeof(RadixSortPushConstants));
    histPipeline->build();

    auto sortSet = std::make_shared<DescriptorSet>(context, 1);
    sortSet->bindBufferToDescriptorSet(0, DescriptorType::eStorageBuffer, ShaderStageFlagBits::eCompute, keyEven);
    sortSet->bindBufferToDescriptorSet(0, DescriptorType::eStorageBuffer, ShaderStageFlagBits::eCompute, keyOdd);
    sortSet->bindBufferToDescriptorSet(1, DescriptorType::eStorageBuffer, ShaderStageFlagBits::eCompute, keyOdd);
    sortSet->bindBufferToDescriptorSet(1, DescriptorType::eStorageBuffer, ShaderStageFlagBits::eCompute, keyEven);
    sortSet->bindBufferToDescriptorSet(2, DescriptorType::eStorageBuffer, ShaderStageFlagBits::eCompute, payEven);
    sortSet->bindBufferToDescriptorSet(2, DescriptorType::eStorageBuffer, ShaderStageFlagBits::eCompute, payOdd);
    sortSet->bindBufferToDescriptorSet(3, DescriptorType::eStorageBuffer, ShaderStageFlagBits::eCompute, payOdd);
    sortSet->bindBufferToDescriptorSet(3, DescriptorType::eStorageBuffer, ShaderStageFlagBits::eCompute, payEven);
    sortSet->bindBufferToDescriptorSet(4, DescriptorType::eStorageBuffer, ShaderStageFlagBits::eCompute, hist);
    sortSet->build();

    auto sortPipeline =
        std::make_shared<ComputePipeline>(context, std::make_shared<Shader>(context, "sort", SPV_SORT, SPV_SORT_len));
    sortPipeline->addDescriptorSet(0, sortSet);
    sortPipeline->addPushConstant(ShaderStageFlagBits::eCompute, 0, sizeof(RadixSortPushConstants));
    sortPipeline->build();

    auto cmd = context->beginOneTimeCommandBuffer();
    for (uint32_t i = 0; i < 8; ++i) {
        uint32_t invocationSize = (n + kBlocksPerWorkgroup - 1) / kBlocksPerWorkgroup;
        invocationSize = (invocationSize + 255) / 256;

        RadixSortPushConstants pc{};
        pc.g_num_elements = n;
        pc.g_num_blocks_per_workgroup = kBlocksPerWorkgroup;
        pc.g_shift = i * 8;
        pc.g_num_workgroups = invocationSize;

        const uint32_t option = i % 2 == 0 ? 0 : 1;

        histPipeline->bind(cmd, 0, option);
        cmd->pushConstants(histPipeline->pipelineLayout.get(), ShaderStageFlagBits::eCompute, 0,
                           sizeof(RadixSortPushConstants), &pc);
        cmd->dispatch(invocationSize, 1, 1);
        hist->computeWriteReadBarrier(cmd.get());

        sortPipeline->bind(cmd, 0, option);
        cmd->pushConstants(sortPipeline->pipelineLayout.get(), ShaderStageFlagBits::eCompute, 0,
                           sizeof(RadixSortPushConstants), &pc);
        cmd->dispatch(invocationSize, 1, 1);

        if (option == 0) {
            keyOdd->computeWriteReadBarrier(cmd.get());
            payOdd->computeWriteReadBarrier(cmd.get());
        } else {
            keyEven->computeWriteReadBarrier(cmd.get());
            payEven->computeWriteReadBarrier(cmd.get());
        }
    }
    context->endOneTimeCommandBuffer(std::move(cmd), VulkanContext::Queue::COMPUTE);

    // After 8 (even count) passes the result lives in the even buffers.
    auto keyBytes = keyEven->download();
    auto payBytes = payEven->download();

    SortResult result;
    result.keys.resize(n);
    result.payloads.resize(n);
    std::memcpy(result.keys.data(), keyBytes.data(), n * sizeof(uint64_t));
    std::memcpy(result.payloads.data(), payBytes.data(), n * sizeof(uint32_t));
    return result;
}

void expectSortedMatchesReference(const std::vector<uint64_t>& input) {
    auto context = vkgs_test::makeHeadlessContext();
    if (!context) {
        GTEST_SKIP() << "No Vulkan device available";
    }

    auto result = runGpuSort(context, input);

    std::vector<uint64_t> reference = input;
    std::sort(reference.begin(), reference.end());
    EXPECT_EQ(result.keys, reference);

    // Payloads must map each sorted slot back to a key equal to the original.
    for (size_t i = 0; i < result.keys.size(); ++i) {
        ASSERT_LT(result.payloads[i], input.size());
        EXPECT_EQ(input[result.payloads[i]], result.keys[i]) << "payload mismatch at " << i;
    }
}

TEST(GpuRadixSort, RandomKeys) {
    std::mt19937_64 gen(12345);
    std::vector<uint64_t> keys(2048);
    for (auto& k : keys) {
        k = gen();
    }
    expectSortedMatchesReference(keys);
}

TEST(GpuRadixSort, ReverseOrdered) {
    std::vector<uint64_t> keys(1024);
    for (uint32_t i = 0; i < keys.size(); ++i) {
        keys[i] = keys.size() - i;
    }
    expectSortedMatchesReference(keys);
}

TEST(GpuRadixSort, Duplicates) {
    std::vector<uint64_t> keys(1024, 7ull);
    for (size_t i = 0; i < keys.size(); i += 2) {
        keys[i] = 3ull;
    }
    expectSortedMatchesReference(keys);
}

} // namespace
