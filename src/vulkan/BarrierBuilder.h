#ifndef VKGS_VULKAN_BARRIER_BUILDER_H
#define VKGS_VULKAN_BARRIER_BUILDER_H

#include <cstdint>
#include <memory>
#include <vector>
#include <vulkan/vulkan.hpp>

class Buffer;

namespace vkgs::vulkan {

class BarrierBuilder {
  public:
    BarrierBuilder& queueFamilyIndex(uint32_t queueFamilyIndex);

    BarrierBuilder& addBufferBarrier(const std::shared_ptr<Buffer>& buffer, vk::AccessFlags sourceAccess,
                                     vk::AccessFlags destinationAccess, uint32_t sourceQueueFamily,
                                     uint32_t destinationQueueFamily);

    BarrierBuilder& addBufferBarrier(const std::shared_ptr<Buffer>& buffer, vk::AccessFlags sourceAccess,
                                     vk::AccessFlags destinationAccess);

    BarrierBuilder& sourceQueueFamilyIndex(uint32_t queueFamilyIndex);

    BarrierBuilder& destinationQueueFamilyIndex(uint32_t queueFamilyIndex);

    void build(vk::CommandBuffer commandBuffer, vk::PipelineStageFlags sourceStage,
               vk::PipelineStageFlags destinationStage) const;

  private:
    std::vector<vk::BufferMemoryBarrier> barriers;
    uint32_t sourceQueueFamily = VK_QUEUE_FAMILY_IGNORED;
    uint32_t destinationQueueFamily = VK_QUEUE_FAMILY_IGNORED;
};

} // namespace vkgs::vulkan

#endif // VKGS_VULKAN_BARRIER_BUILDER_H
