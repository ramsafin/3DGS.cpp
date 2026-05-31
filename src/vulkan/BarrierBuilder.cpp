#include "BarrierBuilder.h"

#include "Buffer.h"

namespace vkgs::vulkan {

BarrierBuilder& BarrierBuilder::queueFamilyIndex(uint32_t queueFamilyIndex) {
    sourceQueueFamily = queueFamilyIndex;
    destinationQueueFamily = queueFamilyIndex;
    return *this;
}

BarrierBuilder& BarrierBuilder::addBufferBarrier(const std::shared_ptr<Buffer>& buffer, vk::AccessFlags sourceAccess,
                                                 vk::AccessFlags destinationAccess, uint32_t sourceQueueFamily,
                                                 uint32_t destinationQueueFamily) {
    barriers.emplace_back(sourceAccess, destinationAccess, sourceQueueFamily, destinationQueueFamily, buffer->buffer, 0,
                          buffer->size);
    return *this;
}

BarrierBuilder& BarrierBuilder::addBufferBarrier(const std::shared_ptr<Buffer>& buffer, vk::AccessFlags sourceAccess,
                                                 vk::AccessFlags destinationAccess) {
    return addBufferBarrier(buffer, sourceAccess, destinationAccess, sourceQueueFamily, destinationQueueFamily);
}

BarrierBuilder& BarrierBuilder::sourceQueueFamilyIndex(uint32_t queueFamilyIndex) {
    sourceQueueFamily = queueFamilyIndex;
    return *this;
}

BarrierBuilder& BarrierBuilder::destinationQueueFamilyIndex(uint32_t queueFamilyIndex) {
    destinationQueueFamily = queueFamilyIndex;
    return *this;
}

void BarrierBuilder::build(vk::CommandBuffer commandBuffer, vk::PipelineStageFlags sourceStage,
                           vk::PipelineStageFlags destinationStage) const {
    commandBuffer.pipelineBarrier(sourceStage, destinationStage, vk::DependencyFlags(), 0, nullptr, barriers.size(),
                                  barriers.data(), 0, nullptr);
}

} // namespace vkgs::vulkan
