#include "Swapchain.hpp"

#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

#include <vk_enum_string_helper.h>

Swapchain::Swapchain(
    const std::shared_ptr<VulkanContext>& context,
    const std::shared_ptr<Window>& window,
    bool immediate
)
    : context(context)
    , window(window)
    , immediate(immediate) {
    createSwapchain();
    createSwapchainImages();
}

void Swapchain::createSwapchain() {
    auto physicalDevice = context->physicalDevice;

    auto capabilities = physicalDevice.getSurfaceCapabilitiesKHR(*context->surface.value());
    auto formats = physicalDevice.getSurfaceFormatsKHR(*context->surface.value());
    auto presentModes = physicalDevice.getSurfacePresentModesKHR(*context->surface.value());

    auto [width, height] = window->getFramebufferSize();

    surfaceFormat = formats[0];
    for (const auto& availableFormat : formats) {
        if (availableFormat.format == vk::Format::eB8G8R8A8Unorm &&
            availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
            surfaceFormat = availableFormat;
            break;
        }
    }
    spdlog::debug("Surface format: {}", string_VkFormat(static_cast<VkFormat>(surfaceFormat.format)));

    presentMode = vk::PresentModeKHR::eFifo;
    for (const auto& availablePresentMode : presentModes) {
        if (immediate && availablePresentMode == vk::PresentModeKHR::eImmediate) {
            presentMode = availablePresentMode;
            break;
        }

        if (availablePresentMode == vk::PresentModeKHR::eMailbox) {
            presentMode = availablePresentMode;
            break;
        }
    }
    spdlog::debug("Present mode: {}", string_VkPresentModeKHR(static_cast<VkPresentModeKHR>(presentMode)));

    auto extent = capabilities.currentExtent;
    if (capabilities.currentExtent.width == UINT32_MAX || capabilities.currentExtent.width == 0) {
        extent.width = std::clamp(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        extent.height = std::clamp(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    }

    spdlog::debug(
        "Swapchain extent range: {}x{} - {}x{}",
        capabilities.minImageExtent.width,
        capabilities.minImageExtent.height,
        capabilities.maxImageExtent.width,
        capabilities.maxImageExtent.height
    );

    // Request one extra image for triple buffering, clamped to the device limit
    // (maxImageCount == 0 means "no upper bound") (VKGS-018).
    imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    vk::SwapchainCreateInfoKHR createInfo = {};
    createInfo.surface = *context->surface.value();
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eStorage;

    // Deduplicate by the real queue-family index, not the Queue::Type enum key
    // (VKGS-003). Concurrent sharing requires distinct family indices.
    std::vector<uint32_t> uniqueQueueFamilies;
    for (auto& queue : context->queues) {
        if (std::find(uniqueQueueFamilies.begin(), uniqueQueueFamilies.end(), queue.second.queueFamily) ==
            uniqueQueueFamilies.end()) {
            uniqueQueueFamilies.push_back(queue.second.queueFamily);
        }
    }

    if (uniqueQueueFamilies.size() > 1) {
        createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
        createInfo.queueFamilyIndexCount = (uint32_t)uniqueQueueFamilies.size();
        createInfo.pQueueFamilyIndices = uniqueQueueFamilies.data();
    } else {
        createInfo.imageSharingMode = vk::SharingMode::eExclusive;
    }

    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    // Reuse resources from the previous swapchain during recreation (VKGS-017).
    createInfo.oldSwapchain = swapchain ? swapchain.get() : vk::SwapchainKHR{};

    spdlog::debug("Swapchain extent: {}x{}. Preferred extent: {}x{}", extent.width, extent.height, width, height);
    swapchainExtent = extent;
    swapchainFormat = surfaceFormat.format;

    swapchain = context->device->createSwapchainKHRUnique(createInfo);
    spdlog::debug("Swapchain created");
}

void Swapchain::createSwapchainImages() {
    // Reset per-image state so recreation does not accumulate stale image views
    // and acquire semaphores (VKGS-017).
    swapchainImages.clear();
    swapchainImageViews.clear();
    imageAvailableSemaphores.clear();

    auto images = context->device->getSwapchainImagesKHR(*swapchain);

    for (auto& image : images) {
        auto imageView = context->device->createImageViewUnique(
            {{}, image, vk::ImageViewType::e2D, swapchainFormat, {}, {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1}}
        );
        swapchainImages.push_back({image, imageView.get(), swapchainFormat, swapchainExtent});
        swapchainImageViews.push_back(std::move(imageView));
    }

    for (int i = 0; i < swapchainImages.size(); i++) {
        imageAvailableSemaphores.emplace_back(context->device->createSemaphoreUnique({}));
    }
}

void Swapchain::recreate() {
    context->device->waitIdle();

    // Keep the old swapchain alive across createSwapchain so it can be passed as
    // oldSwapchain; the unique handle frees it once the new one is built. Image
    // views and semaphores are reset inside createSwapchainImages (VKGS-017).
    createSwapchain();
    createSwapchainImages();
    spdlog::debug("Swapchain recreated");
}
