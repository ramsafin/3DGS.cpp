#include "OffscreenRenderTarget.hpp"

#include <stdexcept>
#include <utility>

OffscreenRenderTarget::OffscreenRenderTarget(std::shared_ptr<VulkanContext> context, uint32_t width, uint32_t height,
                                             vk::Format format)
    : extent{width, height}, format(format), context(std::move(context)) {
    vk::ImageCreateInfo imageInfo{};
    imageInfo.imageType = vk::ImageType::e2D;
    imageInfo.format = format;
    imageInfo.extent = vk::Extent3D{width, height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = vk::SampleCountFlagBits::e1;
    imageInfo.tiling = vk::ImageTiling::eOptimal;
    imageInfo.usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferSrc;
    imageInfo.sharingMode = vk::SharingMode::eExclusive;
    imageInfo.initialLayout = vk::ImageLayout::eUndefined;

    VmaAllocationCreateInfo allocationInfo{};
    allocationInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VkImage vkImage = VK_NULL_HANDLE;
    auto vkImageInfo = static_cast<VkImageCreateInfo>(imageInfo);
    if (vmaCreateImage(this->context->allocator, &vkImageInfo, &allocationInfo, &vkImage, &allocation, nullptr) !=
        VK_SUCCESS) {
        throw std::runtime_error("Failed to create offscreen render target image");
    }
    image = vk::Image(vkImage);

    imageView = this->context->device->createImageViewUnique(
        {{}, image, vk::ImageViewType::e2D, format, {}, {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}});

    images.push_back({image, imageView.get(), format, extent});

    auto commandBuffer = this->context->beginOneTimeCommandBuffer();
    vk::ImageMemoryBarrier imageMemoryBarrier{};
    imageMemoryBarrier.oldLayout = vk::ImageLayout::eUndefined;
    imageMemoryBarrier.newLayout = vk::ImageLayout::eGeneral;
    imageMemoryBarrier.image = image;
    imageMemoryBarrier.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
    imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eNoneKHR;
    imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eShaderWrite;
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    commandBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eComputeShader,
                                   vk::DependencyFlagBits::eByRegion, nullptr, nullptr, imageMemoryBarrier);
    this->context->endOneTimeCommandBuffer(std::move(commandBuffer), VulkanContext::Queue::COMPUTE);
}

OffscreenRenderTarget::~OffscreenRenderTarget() {
    images.clear();
    imageView.reset();
    if (image && allocation != nullptr) {
        vmaDestroyImage(context->allocator, static_cast<VkImage>(image), allocation);
    }
}
