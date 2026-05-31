#pragma once

#include <vulkan/vulkan.hpp>

namespace vkgs::vulkan {

struct RenderImageView {
    vk::Image image;
    vk::ImageView imageView;
    vk::Format format;
    vk::Extent2D extent;
};

} // namespace vkgs::vulkan
