#ifndef VKGS_VULKAN_RENDER_IMAGE_VIEW_H
#define VKGS_VULKAN_RENDER_IMAGE_VIEW_H

#include <vulkan/vulkan.hpp>

namespace vkgs::vulkan {

struct RenderImageView {
    vk::Image image;
    vk::ImageView imageView;
    vk::Format format;
    vk::Extent2D extent;
};

} // namespace vkgs::vulkan

#endif // VKGS_VULKAN_RENDER_IMAGE_VIEW_H
