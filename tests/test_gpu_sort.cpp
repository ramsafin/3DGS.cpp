#include "test_support.hpp"

#include "GpuConstants.hpp"
#include "render/PassSizing.hpp"
#include "render/GpuTypes.hpp"
#include "shaders.h"
#include "vulkan/Buffer.hpp"
#include "vulkan/DescriptorSet.hpp"
#include "vulkan/Shader.hpp"
#include "vulkan/VulkanContext.hpp"
#include "vulkan/pipelines/ComputePipeline.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <numeric>
#include <random>
#include <stdexcept>
#include <vector>

namespace {

constexpr uint32_t kBlocksPerWorkgroup = gpu::RadixBlocksPerWorkgroup;
// Runs the GPU radix sort exactly as Renderer does: eight 8-bit passes over
// 64-bit keys, ping-ponging even/odd buffers. Returns the sorted keys and the
// permuted payloads.
struct SortResult {
    std::vector<uint64_t> keys;
    std::vector<uint32_t> payloads;
};

SortResult runGpuSort(const std::shared_ptr<VulkanContext>& context, const std::vector<uint64_t>& inputKeys,
                      VulkanContext::RadixSortMode mode) {
    if (inputKeys.size() > std::numeric_limits<uint32_t>::max()) {
        throw std::runtime_error("GPU sort test input exceeds uint32_t");
    }
    const uint32_t n = static_cast<uint32_t>(inputKeys.size());

    auto keyEven = Buffer::storage(context, n * sizeof(uint64_t));
    auto keyOdd = Buffer::storage(context, n * sizeof(uint64_t));
    auto payEven = Buffer::storage(context, n * sizeof(uint32_t));
    auto payOdd = Buffer::storage(context, n * sizeof(uint32_t));

    uint32_t globalInvocationSize = vkgs::render::ceilDiv(n, kBlocksPerWorkgroup);
    uint32_t numWorkgroups = vkgs::render::ceilDiv(globalInvocationSize, gpu::WorkgroupSize);
    auto hist = Buffer::storage(context, numWorkgroups * gpu::RadixSortBins * sizeof(uint32_t));

    std::vector<uint32_t> payloads(n);
    std::iota(payloads.begin(), payloads.end(), 0u);
    keyEven->upload(std::span(inputKeys));
    payEven->upload(std::span(payloads));

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
    histPipeline->addPushConstant(ShaderStageFlagBits::eCompute, 0, sizeof(vkgs::render::RadixSortPushConstants));
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

    const auto sortShader = mode == VulkanContext::RadixSortMode::FastSubgroup32
                                ? std::make_shared<Shader>(context, "sort", SPV_SORT, SPV_SORT_len)
                                : std::make_shared<Shader>(context, "sort_portable", SPV_SORT_PORTABLE,
                                                           SPV_SORT_PORTABLE_len);
    auto sortPipeline = std::make_shared<ComputePipeline>(context, sortShader);
    sortPipeline->addDescriptorSet(0, sortSet);
    sortPipeline->addPushConstant(ShaderStageFlagBits::eCompute, 0, sizeof(vkgs::render::RadixSortPushConstants));
    sortPipeline->build();

    auto cmd = context->beginOneTimeCommandBuffer();
    for (uint32_t i = 0; i < 8; ++i) {
        uint32_t invocationSize = vkgs::render::ceilDiv(n, kBlocksPerWorkgroup);
        invocationSize = vkgs::render::ceilDiv(invocationSize, gpu::WorkgroupSize);

        vkgs::render::RadixSortPushConstants pc{};
        pc.numElements = n;
        pc.numBlocksPerWorkgroup = kBlocksPerWorkgroup;
        pc.shift = i * 8;
        pc.numWorkgroups = invocationSize;

        const uint32_t option = i % 2 == 0 ? 0 : 1;

        histPipeline->bind(cmd, 0, option);
        cmd->pushConstants(histPipeline->pipelineLayout.get(), ShaderStageFlagBits::eCompute, 0,
                           sizeof(vkgs::render::RadixSortPushConstants), &pc);
        cmd->dispatch(invocationSize, 1, 1);
        hist->computeWriteReadBarrier(cmd.get());

        sortPipeline->bind(cmd, 0, option);
        cmd->pushConstants(sortPipeline->pipelineLayout.get(), ShaderStageFlagBits::eCompute, 0,
                           sizeof(vkgs::render::RadixSortPushConstants), &pc);
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

void expectSortedMatchesReference(const std::shared_ptr<VulkanContext>& context, const std::vector<uint64_t>& input,
                                  VulkanContext::RadixSortMode mode) {
    auto result = runGpuSort(context, input, mode);

    std::vector<uint64_t> reference = input;
    std::sort(reference.begin(), reference.end());
    EXPECT_EQ(result.keys, reference);

    // Payloads must map each sorted slot back to a key equal to the original.
    for (size_t i = 0; i < result.keys.size(); ++i) {
        ASSERT_LT(result.payloads[i], input.size());
        EXPECT_EQ(input[result.payloads[i]], result.keys[i]) << "payload mismatch at " << i;
    }
}

void exerciseMode(VulkanContext::RadixSortMode mode) {
    auto context = vkgs_test::makeHeadlessContext();
    if (!context) {
        GTEST_SKIP() << "No Vulkan device available";
    }
    if (mode == VulkanContext::RadixSortMode::FastSubgroup32 &&
        context->getRadixSortMode() != VulkanContext::RadixSortMode::FastSubgroup32) {
        GTEST_SKIP() << "Selected Vulkan device does not support the fast radix path";
    }

    expectSortedMatchesReference(context, {7ull}, mode);

    std::vector<uint64_t> reverse(255);
    for (uint32_t i = 0; i < reverse.size(); ++i) {
        reverse[i] = reverse.size() - i;
    }
    expectSortedMatchesReference(context, reverse, mode);

    std::vector<uint64_t> duplicates(256, 7ull);
    for (size_t i = 0; i < duplicates.size(); i += 2) {
        duplicates[i] = 3ull;
    }
    expectSortedMatchesReference(context, duplicates, mode);

    std::mt19937_64 gen(12345);
    std::vector<uint64_t> random(257);
    for (auto& k : random) {
        k = gen();
    }
    expectSortedMatchesReference(context, random, mode);

    std::vector<uint64_t> growth(10000);
    for (auto& k : growth) {
        k = gen();
    }
    expectSortedMatchesReference(context, growth, mode);
}

TEST(GpuRadixSort, PortableMode) {
    exerciseMode(VulkanContext::RadixSortMode::Portable);
}

TEST(GpuRadixSort, FastSubgroup32ModeWhenSupported) {
    exerciseMode(VulkanContext::RadixSortMode::FastSubgroup32);
}

} // namespace
