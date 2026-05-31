#pragma once

#include "Swapchain.hpp"
#include "VulkanContext.hpp"
#include "Window.hpp"

#include <functional>

class GLFWWindow;

class ImguiManager {
  public:
    ImguiManager(
        std::shared_ptr<VulkanContext> context,
        std::shared_ptr<Swapchain> swapchain,
        std::shared_ptr<GLFWWindow> window
    );

    void createCommandPool();

    void setStyle();

    void init();

    void onSwapchainRecreated();
    void immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);

    void draw(vk::CommandBuffer commandBuffer, uint32_t currentImageIndex, std::function<void(void)> imguiFunction);

    ~ImguiManager();

  private:
    std::shared_ptr<VulkanContext> context;
    std::shared_ptr<Swapchain> swapchain;
    std::shared_ptr<GLFWWindow> window;
    vk::UniqueCommandPool commandPool;
    vk::UniqueCommandBuffer commandBuffer;
    vk::UniqueFence fence;
    vk::UniqueDescriptorPool descriptorPool;

    void initVulkanBackend();
};
