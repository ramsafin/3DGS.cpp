#include "VulkanContext.h"

#include "Utils.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <set>
#include <spdlog/spdlog.h>
#include <sstream>
#include <unordered_map>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace {
constexpr uint32_t kRequiredApiVersion = VK_API_VERSION_1_2;
constexpr uint32_t kRequiredSubgroupSize = 32;
constexpr uint32_t kShaderWorkgroupSize = 256;

std::string joinReasons(const std::vector<std::string>& reasons) {
    std::ostringstream message;
    for (size_t i = 0; i < reasons.size(); ++i) {
        if (i > 0) {
            message << "; ";
        }
        message << reasons[i];
    }
    return message.str();
}
} // namespace

VkBool32 debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                       VkDebugUtilsMessageTypeFlagsEXT messageType,
                       const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
    const char* type = "???";
    if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT) {
        type = "GENERAL";
    } else if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
        type = "VALIDATION";
    } else if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
        type = "PERFORMANCE";
    }

    const char* severity = "???";
    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
        spdlog::debug("[{}]: {}", type, pCallbackData->pMessage);
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
        spdlog::info("[{}]: {}", type, pCallbackData->pMessage);
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        spdlog::warn("[{}]: {}", type, pCallbackData->pMessage);
    } else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        spdlog::critical("[{}]: {}", type, pCallbackData->pMessage);
    }

    return VK_FALSE;
}

VulkanContext::VulkanContext(const std::vector<std::string>& instance_extensions,
                             const std::vector<std::string>& device_extensions, bool validation_layers_enabled)
    : instanceExtensions(instance_extensions), deviceExtensions(device_extensions),
      validationLayersEnabled(validation_layers_enabled) {
#ifdef VKGS_RENDER_MODE_ONSCREEN
    deviceExtensions.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
#endif
#ifdef DEBUG
    deviceExtensions.push_back(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME);
#endif

    if (validation_layers_enabled) {
        instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    VULKAN_HPP_DEFAULT_DISPATCHER.init();
}

void VulkanContext::createInstance() {
    vk::ApplicationInfo appInfo = {"Vulkan Splatting", VK_MAKE_VERSION(1, 0, 0), "No Engine", VK_MAKE_VERSION(1, 0, 0),
                                   kRequiredApiVersion};

    std::vector<const char*> requiredLayers;
    if (validationLayersEnabled) {
        requiredLayers.push_back("VK_LAYER_KHRONOS_validation");
    }

    auto instanceExtensionsCharPtr = Utils::stringVectorToCharPtrVector(instanceExtensions);
    vk::StructureChain<vk::InstanceCreateInfo, vk::DebugUtilsMessengerCreateInfoEXT> createInfoChain = {
        {{},
         &appInfo,
         (uint32_t)requiredLayers.size(),
         requiredLayers.data(),
         (uint32_t)instanceExtensions.size(),
         instanceExtensionsCharPtr.data()},
        {{},
         vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning | vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
             vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose | vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo,
         vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
             vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance,
         debugCallback}};

    if (!validationLayersEnabled) {
        createInfoChain.unlink<vk::DebugUtilsMessengerCreateInfoEXT>();
    }

    instance = vk::createInstanceUnique(createInfoChain.get<vk::InstanceCreateInfo>());
    VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);
    spdlog::debug("Vulkan instance created");
}

bool VulkanContext::isDeviceSuitable(vk::PhysicalDevice device, std::optional<vk::SurfaceKHR> surface) {
    return getDeviceUnsuitabilityReasons(device, surface).empty();
}

std::vector<std::string> VulkanContext::getDeviceUnsuitabilityReasons(vk::PhysicalDevice device,
                                                                      std::optional<vk::SurfaceKHR> surface) const {
    std::vector<std::string> reasons;
    auto properties = device.getProperties();
    if (properties.apiVersion < kRequiredApiVersion) {
        reasons.push_back("Vulkan 1.2 is required");
    }

    auto supportedExtensions = device.enumerateDeviceExtensionProperties();
    for (auto& extension : deviceExtensions) {
        if (std::find_if(supportedExtensions.begin(), supportedExtensions.end(),
                         [&extension](const vk::ExtensionProperties& supportedExtension) {
                             return strcmp(extension.c_str(), supportedExtension.extensionName) == 0;
                         }) == supportedExtensions.end()) {
            reasons.push_back("missing device extension " + extension);
        }
    }

    bool hasGraphicsQueue = false;
    bool hasComputeQueue = false;
    bool hasTimestampComputeQueue = false;
    bool hasUnifiedGraphicsComputeQueue = false;
    bool hasPresentQueue = false;
    const auto queueFamilies = device.getQueueFamilyProperties();
    for (uint32_t i = 0; i < queueFamilies.size(); ++i) {
        const auto& queueFamily = queueFamilies[i];
        const bool graphics = static_cast<bool>(queueFamily.queueFlags & vk::QueueFlagBits::eGraphics);
        const bool compute = static_cast<bool>(queueFamily.queueFlags & vk::QueueFlagBits::eCompute);
        const bool timestamps = queueFamily.timestampValidBits > 0;
        hasGraphicsQueue |= graphics;
        hasComputeQueue |= compute;
        hasTimestampComputeQueue |= compute && timestamps;
        hasUnifiedGraphicsComputeQueue |= graphics && compute && timestamps;
        if (surface.has_value() && device.getSurfaceSupportKHR(i, surface.value())) {
            hasPresentQueue = true;
        }
    }
    if (!hasGraphicsQueue) {
        reasons.push_back("missing graphics queue family");
    }
    if (!hasComputeQueue) {
        reasons.push_back("missing compute queue family");
    } else if (!hasTimestampComputeQueue) {
        reasons.push_back("compute queue family does not support timestamps");
    }
#ifdef VKGS_RENDER_MODE_ONSCREEN
    if (!hasUnifiedGraphicsComputeQueue) {
        reasons.push_back("on-screen mode requires one graphics+compute queue family with timestamp support");
    }
#endif

    if (properties.limits.maxComputeWorkGroupInvocations < kShaderWorkgroupSize ||
        properties.limits.maxComputeWorkGroupSize[0] < kShaderWorkgroupSize) {
        reasons.push_back("compute workgroup size 256 is not supported");
    }

    const auto offscreenFormatProperties = device.getFormatProperties(vk::Format::eR8G8B8A8Unorm);
    const auto requiredOffscreenFormatFeatures =
        vk::FormatFeatureFlagBits::eStorageImage | vk::FormatFeatureFlagBits::eTransferSrc;
    if ((offscreenFormatProperties.optimalTilingFeatures & requiredOffscreenFormatFeatures) !=
        requiredOffscreenFormatFeatures) {
        reasons.push_back("R8G8B8A8_UNORM optimal images require storage-image and transfer-source support");
    }

    if (surface.has_value()) {
        auto surfaceCapabilities = device.getSurfaceCapabilitiesKHR(surface.value());
        auto surfaceFormats = device.getSurfaceFormatsKHR(surface.value());
        auto presentModes = device.getSurfacePresentModesKHR(surface.value());

        if (surfaceFormats.empty() || presentModes.empty()) {
            reasons.push_back("surface has no supported format or present mode");
        }
        if (!hasPresentQueue) {
            reasons.push_back("missing queue family with presentation support");
        }
        const auto requiredSurfaceUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eStorage;
        if ((surfaceCapabilities.supportedUsageFlags & requiredSurfaceUsage) != requiredSurfaceUsage) {
            reasons.push_back("surface images require color-attachment and storage-image usage");
        }
    }

    if (properties.apiVersion >= kRequiredApiVersion) {
        vk::PhysicalDeviceVulkan12Features features12{};
#ifdef VKGS_RENDER_MODE_ONSCREEN
        vk::PhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingFeatures{};
        features12.pNext = &dynamicRenderingFeatures;
#endif
        vk::PhysicalDeviceFeatures2 features2{};
        features2.pNext = &features12;
        device.getFeatures2(&features2);

        if (!features2.features.shaderStorageImageWriteWithoutFormat) {
            reasons.push_back("shaderStorageImageWriteWithoutFormat is not supported");
        }
        if (!features2.features.shaderInt64) {
            reasons.push_back("shaderInt64 is not supported");
        }
        if (!features12.shaderSharedInt64Atomics) {
            reasons.push_back("shaderSharedInt64Atomics is not supported");
        }
#ifdef VKGS_RENDER_MODE_ONSCREEN
        if (!dynamicRenderingFeatures.dynamicRendering) {
            reasons.push_back("dynamic rendering is not supported");
        }
#endif

        vk::PhysicalDeviceSubgroupProperties subgroupProperties{};
        vk::PhysicalDeviceProperties2 properties2{};
        properties2.pNext = &subgroupProperties;
        device.getProperties2(&properties2);

        const auto requiredSubgroupOperations = vk::SubgroupFeatureFlagBits::eBasic |
                                                vk::SubgroupFeatureFlagBits::eArithmetic |
                                                vk::SubgroupFeatureFlagBits::eBallot;
        if (!(subgroupProperties.supportedStages & vk::ShaderStageFlagBits::eCompute)) {
            reasons.push_back("compute shaders do not support subgroup operations");
        }
        if ((subgroupProperties.supportedOperations & requiredSubgroupOperations) != requiredSubgroupOperations) {
            reasons.push_back("radix sort requires basic, arithmetic, and ballot subgroup operations");
        }
        if (subgroupProperties.subgroupSize != kRequiredSubgroupSize) {
            reasons.push_back("radix sort currently requires subgroup size 32");
        }
    }

    return reasons;
}

void VulkanContext::selectPhysicalDevice(std::optional<uint8_t> id, std::optional<vk::SurfaceKHR> surface) {
    if (surface.has_value()) {
        vk::UniqueSurfaceKHR surfaceUnique{surface.value(), *instance};
        this->surface = std::make_optional(std::move(surfaceUnique));
    }
    auto devices = instance->enumeratePhysicalDevices();

    spdlog::info("Available physical devices:");
    int ind = 0;
    for (auto& device : devices) {
        spdlog::info("[{}] {}", ind++, device.getProperties().deviceName);
    }

    if (surface.has_value()) {
        deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }

    if (id.has_value()) {
        if (devices.size() <= id.value()) {
            throw std::runtime_error("Invalid physical device id");
        }
        physicalDevice = devices[id.value()];
        const auto reasons = getDeviceUnsuitabilityReasons(physicalDevice, surface);
        if (!reasons.empty()) {
            throw std::runtime_error("Selected Vulkan physical device is unsuitable: " + joinReasons(reasons));
        }
        spdlog::info("Selected physical device (by index): {}", physicalDevice.getProperties().deviceName);
        return;
    }

    auto suitableDevices = std::vector<vk::PhysicalDevice>{};
    for (auto& device : devices) {
        const auto reasons = getDeviceUnsuitabilityReasons(device, surface);
        if (reasons.empty()) {
            suitableDevices.push_back(device);
        } else {
            spdlog::warn("Skipping Vulkan physical device '{}': {}", device.getProperties().deviceName,
                         joinReasons(reasons));
        }
    }

    if (suitableDevices.empty()) {
        throw std::runtime_error("No suitable Vulkan physical device found; see logged capability diagnostics");
    }

    physicalDevice = suitableDevices[0];
    for (auto& device : suitableDevices) {
        auto properties = device.getProperties();
        if (properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
            physicalDevice = device;
            break;
        }
    }

    spdlog::info("Selected physical device (automatically): {}", physicalDevice.getProperties().deviceName);
}

VulkanContext::QueueFamilyIndices VulkanContext::findQueueFamilies() {
    QueueFamilyIndices indices;
    auto queueFamilies = physicalDevice.getQueueFamilyProperties();

    // Prefer a single family that supports both graphics and compute so the
    // on-screen render+GUI command buffer can be recorded and submitted on one
    // graphics-capable queue (VKGS-004). The render path submits to the COMPUTE
    // queue and appends ImGui graphics work, which a compute-only queue cannot
    // execute.
    for (uint32_t i = 0; i < queueFamilies.size(); i++) {
        const auto flags = queueFamilies[i].queueFlags;
        if ((flags & vk::QueueFlagBits::eGraphics) && (flags & vk::QueueFlagBits::eCompute) &&
            queueFamilies[i].timestampValidBits > 0) {
            indices.graphicsFamily = i;
            indices.computeFamily = i;
            break;
        }
    }

    for (uint32_t i = 0; i < queueFamilies.size(); i++) {
        const auto flags = queueFamilies[i].queueFlags;
        if (!indices.graphicsFamily.has_value() && (flags & vk::QueueFlagBits::eGraphics)) {
            indices.graphicsFamily = i;
        }
        if (!indices.computeFamily.has_value() && (flags & vk::QueueFlagBits::eCompute) &&
            queueFamilies[i].timestampValidBits > 0) {
            indices.computeFamily = i;
        }
        if (surface.has_value() && !indices.presentFamily.has_value() &&
            physicalDevice.getSurfaceSupportKHR(i, *surface.value())) {
            indices.presentFamily = i;
        }
    }
    return indices;
}

void VulkanContext::createQueryPool() {
    vk::QueryPoolCreateInfo queryPoolCreateInfo = {};
    queryPoolCreateInfo.queryType = vk::QueryType::eTimestamp;
    queryPoolCreateInfo.queryCount = kTimestampQueryCount;
    queryPool = device->createQueryPoolUnique(queryPoolCreateInfo);

    auto commandBuffer = beginOneTimeCommandBuffer(Queue::GRAPHICS);
    commandBuffer->resetQueryPool(queryPool.get(), 0, kTimestampQueryCount);
    endOneTimeCommandBuffer(std::move(commandBuffer), Queue::GRAPHICS);
}

void VulkanContext::createLogicalDevice(vk::PhysicalDeviceFeatures deviceFeatures,
                                        vk::PhysicalDeviceVulkan11Features deviceFeatures11,
                                        vk::PhysicalDeviceVulkan12Features deviceFeatures12) {
    QueueFamilyIndices indices = findQueueFamilies();
    if (!indices.graphicsFamily.has_value() || !indices.computeFamily.has_value()) {
        throw std::runtime_error("Selected device lacks required graphics and/or compute queue families");
    }
    if (surface.has_value() && !indices.presentFamily.has_value()) {
        throw std::runtime_error("Selected device cannot present to the target surface");
    }

    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.computeFamily.value()};
    if (indices.presentFamily.has_value()) {
        uniqueQueueFamilies.insert(indices.presentFamily.value());
    }

    float queuePriority = 1.0f;
    for (auto queueFamily : uniqueQueueFamilies) {
        queueCreateInfos.push_back({{}, queueFamily, 1, &queuePriority});
    }

    auto deviceExtensionsCharPtr = Utils::stringVectorToCharPtrVector(deviceExtensions);

    vk::DeviceCreateInfo createInfo = {
        {},      (uint32_t)queueCreateInfos.size(),        queueCreateInfos.data(),        0,
        nullptr, (uint32_t)deviceExtensionsCharPtr.size(), deviceExtensionsCharPtr.data(), &deviceFeatures};
    createInfo.pNext = &deviceFeatures11;
    deviceFeatures11.pNext = &deviceFeatures12;

#ifdef VKGS_RENDER_MODE_ONSCREEN
    vk::PhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingFeatures{true};
    deviceFeatures12.pNext = &dynamicRenderingFeatures;
#endif

    device = physicalDevice.createDeviceUnique(createInfo);

    for (auto unique_queue_family : uniqueQueueFamilies) {
        auto queue = device->getQueue(unique_queue_family, 0);
        std::set<Queue::Type> types;
        if (unique_queue_family == indices.graphicsFamily.value()) {
            types.insert(Queue::Type::GRAPHICS);
        }
        if (unique_queue_family == indices.computeFamily.value()) {
            types.insert(Queue::Type::COMPUTE);
        }
        if (indices.presentFamily.has_value() && unique_queue_family == indices.presentFamily.value()) {
            types.insert(Queue::Type::PRESENT);
        }

        for (auto type : types) {
            queues[type] = Queue{types, unique_queue_family, 0, queue};
        }
    }

    spdlog::debug("Logical device created");

    // Create VMA
    setupVma();
    createQueryPool();
}

vk::CommandPool VulkanContext::getOneTimePool(uint32_t queueFamily) {
    auto it = oneTimeCommandPools.find(queueFamily);
    if (it == oneTimeCommandPools.end()) {
        vk::CommandPoolCreateInfo poolInfo{vk::CommandPoolCreateFlagBits::eTransient, queueFamily};
        it = oneTimeCommandPools.emplace(queueFamily, device->createCommandPoolUnique(poolInfo)).first;
    }
    return it->second.get();
}

vk::UniqueCommandBuffer VulkanContext::beginOneTimeCommandBuffer(Queue::Type queue) {
    auto info = vk::CommandBufferAllocateInfo()
                    .setCommandPool(getOneTimePool(queues[queue].queueFamily))
                    .setLevel(vk::CommandBufferLevel::ePrimary)
                    .setCommandBufferCount(1);
    auto commandBuffer = std::move(device->allocateCommandBuffersUnique(info)[0]);

    vk::CommandBufferBeginInfo beginInfo = {};
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;
    commandBuffer->begin(beginInfo);
    return commandBuffer;
}

void VulkanContext::endOneTimeCommandBuffer(vk::UniqueCommandBuffer&& commandBuffer, Queue::Type queue) {
    commandBuffer->end();
    vk::SubmitInfo submitInfo = {};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &*commandBuffer;
    queues[queue].queue.submit(submitInfo, nullptr);
    queues[queue].queue.waitIdle();
}

void VulkanContext::setupVma() {
    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = physicalDevice;
    allocatorInfo.device = *device;
    allocatorInfo.instance = *instance;
    // allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocatorInfo, &allocator);
}

void VulkanContext::createDescriptorPool(uint8_t framesInFlight) {
    // get max number of descriptor sets from physical device
    std::vector<vk::DescriptorPoolSize> poolSizes = {
        {vk::DescriptorType::eUniformBuffer, static_cast<uint32_t>(framesInFlight * 10)},
        {vk::DescriptorType::eStorageBuffer, static_cast<uint32_t>(framesInFlight * 50)},
        {vk::DescriptorType::eStorageImage, static_cast<uint32_t>(framesInFlight * 10)}};

    vk::DescriptorPoolCreateInfo poolInfo{vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 100,
                                          static_cast<uint32_t>(poolSizes.size()), poolSizes.data()};

    descriptorPool = device->createDescriptorPoolUnique(poolInfo);
}

VulkanContext::~VulkanContext() {
    if (allocator != nullptr) {
        vmaDestroyAllocator(allocator);
    }
}
