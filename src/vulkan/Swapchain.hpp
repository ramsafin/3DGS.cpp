#ifndef VULKAN_SPLATTING_SWAPCHAIN_H
#define VULKAN_SPLATTING_SWAPCHAIN_H

#include "RenderImageView.hpp"
#include "VulkanContext.hpp"
#include "Window.hpp"

#include <memory>

class Swapchain {
  public:
    Swapchain(const std::shared_ptr<VulkanContext>& context, const std::shared_ptr<Window>& window, bool immediate);

    vk::UniqueSwapchainKHR swapchain;
    vk::Extent2D swapchainExtent;
    std::vector<vk::UniqueImageView> swapchainImageViews;
    std::vector<vkgs::vulkan::RenderImageView> swapchainImages;
    std::vector<vk::UniqueSemaphore> imageAvailableSemaphores;
    vk::SurfaceFormatKHR surfaceFormat;
    vk::Format swapchainFormat;
    vk::PresentModeKHR presentMode;
    uint32_t imageCount;

    void recreate();

  private:
    std::shared_ptr<VulkanContext> context;
    std::shared_ptr<Window> window;

    bool immediate = false;

    void createSwapchain();

    void createSwapchainImages();
};

#endif // VULKAN_SPLATTING_SWAPCHAIN_H
