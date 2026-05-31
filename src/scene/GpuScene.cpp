#include "GpuScene.hpp"

#include "GpuConstants.hpp"
#include "core/CheckedArithmetic.hpp"
#include "render/RenderConstants.hpp"
#include "shaders.h"
#include "spdlog/spdlog.h"
#include "vulkan/DescriptorSet.hpp"
#include "vulkan/Shader.hpp"
#include "vulkan/pipelines/ComputePipeline.hpp"

#include <span>
#include <utility>

namespace vkgs::scene {

GpuScene::GpuScene(GaussianSceneData sceneData) : sceneData(std::move(sceneData)) {}

void GpuScene::upload(const std::shared_ptr<VulkanContext>& context) {
    const auto vertexBytes = vkgs::core::checkedMultiply(sceneData.vertices.size(), sizeof(vkgs::render::SceneVertex),
                                                         "PLY vertex buffer size");
    vertexBuffer = createBuffer(context, vertexBytes);
    vertexBuffer->upload(std::span(sceneData.vertices));
    precomputeCov3D(context);
}

uint64_t GpuScene::getNumVertices() const {
    return sceneData.vertices.size();
}

const SceneBounds& GpuScene::getBounds() const {
    return sceneData.bounds;
}

std::shared_ptr<Buffer> GpuScene::createBuffer(const std::shared_ptr<VulkanContext>& context, vk::DeviceSize size) {
    return std::make_shared<Buffer>(context, size,
                                    vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst,
                                    VMA_MEMORY_USAGE_GPU_ONLY, VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, false);
}

void GpuScene::precomputeCov3D(const std::shared_ptr<VulkanContext>& context) {
    cov3DBuffer = createBuffer(
        context, vkgs::core::checkedMultiply(sceneData.vertices.size(), sizeof(vkgs::render::Cov3DUpperRight),
                                             "Cov3D buffer size"));

    auto pipeline = std::make_shared<ComputePipeline>(
        context, std::make_shared<Shader>(context, "precomp_cov3d", SPV_PRECOMP_COV3D, SPV_PRECOMP_COV3D_len));

    auto descriptorSet = std::make_shared<DescriptorSet>(context, vkgs::render::kFramesInFlight);
    descriptorSet->bindBufferToDescriptorSet(0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                             vertexBuffer);
    descriptorSet->bindBufferToDescriptorSet(1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                             cov3DBuffer);
    descriptorSet->build();

    pipeline->addDescriptorSet(0, descriptorSet);
    pipeline->addPushConstant(vk::ShaderStageFlagBits::eCompute, 0, sizeof(float));
    pipeline->build();

    auto commandBuffer = context->beginOneTimeCommandBuffer();
    pipeline->bind(commandBuffer, 0, 0);
    float scaleFactor = 1.0f;
    commandBuffer->pushConstants(pipeline->pipelineLayout.get(), vk::ShaderStageFlagBits::eCompute, 0, sizeof(float),
                                 &scaleFactor);
    const auto numGroups = static_cast<uint32_t>(sceneData.vertices.size() / gpu::WorkgroupSize +
                                                  (sceneData.vertices.size() % gpu::WorkgroupSize != 0));
    commandBuffer->dispatch(numGroups, 1, 1);
    context->endOneTimeCommandBuffer(std::move(commandBuffer), VulkanContext::Queue::COMPUTE);

    spdlog::info("Precomputed Cov3D");
}

} // namespace vkgs::scene
