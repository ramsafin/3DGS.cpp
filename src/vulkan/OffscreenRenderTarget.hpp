#ifndef OFFSCREEN_RENDER_TARGET_H
#define OFFSCREEN_RENDER_TARGET_H

#include "VulkanContext.hpp"
#include "RenderImageView.hpp"

#include <memory>
#include <vector>

class OffscreenRenderTarget {
  public:
    OffscreenRenderTarget(std::shared_ptr<VulkanContext> context, uint32_t width, uint32_t height,
                          vk::Format format = vk::Format::eR8G8B8A8Unorm);

    OffscreenRenderTarget(const OffscreenRenderTarget&) = delete;
    OffscreenRenderTarget(OffscreenRenderTarget&&) = delete;
    OffscreenRenderTarget& operator=(const OffscreenRenderTarget&) = delete;
    OffscreenRenderTarget& operator=(OffscreenRenderTarget&&) = delete;

    ~OffscreenRenderTarget();

    vk::Extent2D extent;
    vk::Format format;
    std::vector<vkgs::vulkan::RenderImageView> images;

  private:
    std::shared_ptr<VulkanContext> context;
    vk::Image image;
    vk::UniqueImageView imageView;
    VmaAllocation allocation = nullptr;
};

#endif // OFFSCREEN_RENDER_TARGET_H
