#include "VulkanContext.h"

#include "StringList.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <set>
#include <spdlog/spdlog.h>
#include <sstream>
#include <unordered_map>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace {
constexpr uint32_t kRequiredSubgroupSize = 32;

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

bool supportsFastRadixSort(vk::PhysicalDevice device) {
    vk::PhysicalDeviceVulkan12Features features12{};
    vk::PhysicalDeviceFeatures2 features2{};
    features2.pNext = &features12;
    device.getFeatures2(&features2);

    vk::PhysicalDeviceSubgroupProperties subgroupProperties{};
    vk::PhysicalDeviceProperties2 properties2{};
    properties2.pNext = &subgroupProperties;
    device.getProperties2(&properties2);

    const auto requiredSubgroupOperations = vk::SubgroupFeatureFlagBits::eBasic |
                                            vk::SubgroupFeatureFlagBits::eArithmetic |
                                            vk::SubgroupFeatureFlagBits::eBallot;
    return features12.shaderSharedInt64Atomics &&
           (subgroupProperties.supportedStages & vk::ShaderStageFlagBits::eCompute) &&
           (subgroupProperties.supportedOperations & requiredSubgroupOperations) == requiredSubgroupOperations &&
           subgroupProperties.subgroupSize == kRequiredSubgroupSize;
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
                                   VK_API_VERSION_1_2};

    std::vector<const char*> requiredLayers;
    if (validationLayersEnabled) {
        requiredLayers.push_back("VK_LAYER_KHRONOS_validation");
    }

    auto instanceExtensionsCharPtr = vkgs::vulkan::toCStringPointers(instanceExtensions);
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
    return vkgs::vulkan::getUnsuitabilityReasons(getDeviceRequirements(surface.has_value()),
                                                 inspectDeviceCapabilities(device, surface));
}

vkgs::vulkan::DeviceRequirements VulkanContext::getDeviceRequirements(bool requirePresentation) const {
    vkgs::vulkan::DeviceRequirements requirements;
    requirements.extensions = deviceExtensions;
    requirements.presentQueue = requirePresentation;
    requirements.surfaceStorageColor = requirePresentation;
#ifdef VKGS_RENDER_MODE_ONSCREEN
    requirements.graphicsQueue = true;
    requirements.timestampComputeQueue = true;
    requirements.unifiedGraphicsComputeTimestampQueue = true;
    requirements.dynamicRendering = true;
#else
    requirements.offscreenStorageTransferFormat = true;
#endif
    return requirements;
}

vkgs::vulkan::VulkanDeviceCapabilities VulkanContext::inspectDeviceCapabilities(
    vk::PhysicalDevice device, std::optional<vk::SurfaceKHR> surface) const {
    vkgs::vulkan::VulkanDeviceCapabilities capabilities;
    const auto properties = device.getProperties();
    capabilities.apiVersion = properties.apiVersion;
    capabilities.maxComputeWorkgroupInvocations = properties.limits.maxComputeWorkGroupInvocations;
    capabilities.maxComputeWorkgroupSizeX = properties.limits.maxComputeWorkGroupSize[0];

    for (const auto& extension : device.enumerateDeviceExtensionProperties()) {
        capabilities.extensions.emplace_back(extension.extensionName.data());
    }
    const auto queueFamilies = device.getQueueFamilyProperties();
    capabilities.queueFamilies.reserve(queueFamilies.size());
    for (uint32_t index = 0; index < queueFamilies.size(); ++index) {
        const auto& queueFamily = queueFamilies[index];
        capabilities.queueFamilies.push_back(
            {static_cast<bool>(queueFamily.queueFlags & vk::QueueFlagBits::eGraphics),
             static_cast<bool>(queueFamily.queueFlags & vk::QueueFlagBits::eCompute), queueFamily.timestampValidBits > 0,
             surface.has_value() && device.getSurfaceSupportKHR(index, surface.value())});
    }

    if (properties.apiVersion >= VK_API_VERSION_1_2) {
        vk::PhysicalDeviceVulkan12Features features12{};
#ifdef VKGS_RENDER_MODE_ONSCREEN
        vk::PhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingFeatures{};
        features12.pNext = &dynamicRenderingFeatures;
#endif
        vk::PhysicalDeviceFeatures2 features2{};
        features2.pNext = &features12;
        device.getFeatures2(&features2);
        capabilities.shaderStorageImageWriteWithoutFormat = features2.features.shaderStorageImageWriteWithoutFormat;
        capabilities.shaderInt64 = features2.features.shaderInt64;
#ifdef VKGS_RENDER_MODE_ONSCREEN
        capabilities.dynamicRendering = dynamicRenderingFeatures.dynamicRendering;
#endif
    }

    const auto offscreenFormatProperties = device.getFormatProperties(vk::Format::eR8G8B8A8Unorm);
    const auto requiredOffscreenFormatFeatures =
        vk::FormatFeatureFlagBits::eStorageImage | vk::FormatFeatureFlagBits::eTransferSrc;
    capabilities.offscreenStorageTransferFormat =
        (offscreenFormatProperties.optimalTilingFeatures & requiredOffscreenFormatFeatures) ==
        requiredOffscreenFormatFeatures;

    if (surface.has_value()) {
        const auto surfaceCapabilities = device.getSurfaceCapabilitiesKHR(surface.value());
        const auto surfaceFormats = device.getSurfaceFormatsKHR(surface.value());
        const auto presentModes = device.getSurfacePresentModesKHR(surface.value());
        capabilities.surfaceHasFormatAndPresentMode = !surfaceFormats.empty() && !presentModes.empty();
        const auto requiredSurfaceUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eStorage;
        capabilities.surfaceStorageColor =
            (surfaceCapabilities.supportedUsageFlags & requiredSurfaceUsage) == requiredSurfaceUsage;
    }
    return capabilities;
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
        updateSelectedDeviceCapabilities();
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
    updateSelectedDeviceCapabilities();
}

VulkanContext::QueueFamilyIndices VulkanContext::findQueueFamilies() {
    const auto queueFamilies = physicalDevice.getQueueFamilyProperties();
    std::vector<vkgs::vulkan::QueueFamilyCapabilities> capabilities;
    capabilities.reserve(queueFamilies.size());
    for (uint32_t index = 0; index < queueFamilies.size(); ++index) {
        capabilities.push_back(
            {static_cast<bool>(queueFamilies[index].queueFlags & vk::QueueFlagBits::eGraphics),
             static_cast<bool>(queueFamilies[index].queueFlags & vk::QueueFlagBits::eCompute),
             queueFamilies[index].timestampValidBits > 0,
             surface.has_value() && physicalDevice.getSurfaceSupportKHR(index, *surface.value())});
    }
    return vkgs::vulkan::selectQueueFamilies(capabilities, getDeviceRequirements(surface.has_value()));
}

void VulkanContext::updateSelectedDeviceCapabilities() {
    const auto indices = findQueueFamilies();
    if (!indices.computeFamily.has_value()) {
        throw std::runtime_error("Selected device lacks a required compute queue family");
    }

    const auto queueFamilies = physicalDevice.getQueueFamilyProperties();
    timestampQueriesSupported = queueFamilies[indices.computeFamily.value()].timestampValidBits > 0;
    radixSortMode = supportsFastRadixSort(physicalDevice) ? RadixSortMode::FastSubgroup32 : RadixSortMode::Portable;

    spdlog::info("Timestamp metrics: {}", timestampQueriesSupported ? "enabled" : "disabled");
    spdlog::info("Radix sort mode: {}",
                 radixSortMode == RadixSortMode::FastSubgroup32 ? "fast subgroup-32" : "portable");
}

void VulkanContext::createQueryPool() {
    if (!timestampQueriesSupported) {
        return;
    }

    vk::QueryPoolCreateInfo queryPoolCreateInfo = {};
    queryPoolCreateInfo.queryType = vk::QueryType::eTimestamp;
    queryPoolCreateInfo.queryCount = kTimestampQueryCount;
    queryPool = device->createQueryPoolUnique(queryPoolCreateInfo);

    auto commandBuffer = beginOneTimeCommandBuffer(Queue::COMPUTE);
    commandBuffer->resetQueryPool(queryPool.get(), 0, kTimestampQueryCount);
    endOneTimeCommandBuffer(std::move(commandBuffer), Queue::COMPUTE);
}

void VulkanContext::createLogicalDevice(vk::PhysicalDeviceFeatures deviceFeatures,
                                        vk::PhysicalDeviceVulkan11Features deviceFeatures11,
                                        vk::PhysicalDeviceVulkan12Features deviceFeatures12) {
    QueueFamilyIndices indices = findQueueFamilies();
    if (!indices.computeFamily.has_value()) {
        throw std::runtime_error("Selected device lacks a required compute queue family");
    }
#ifdef VKGS_RENDER_MODE_ONSCREEN
    if (!indices.graphicsFamily.has_value()) {
        throw std::runtime_error("Selected device lacks a required graphics queue family");
    }
#endif
    if (surface.has_value() && !indices.presentFamily.has_value()) {
        throw std::runtime_error("Selected device cannot present to the target surface");
    }

    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {indices.computeFamily.value()};
    if (indices.graphicsFamily.has_value()) {
        uniqueQueueFamilies.insert(indices.graphicsFamily.value());
    }
    if (indices.presentFamily.has_value()) {
        uniqueQueueFamilies.insert(indices.presentFamily.value());
    }

    float queuePriority = 1.0f;
    for (auto queueFamily : uniqueQueueFamilies) {
        queueCreateInfos.push_back({{}, queueFamily, 1, &queuePriority});
    }

    auto deviceExtensionsCharPtr = vkgs::vulkan::toCStringPointers(deviceExtensions);

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
        if (indices.graphicsFamily.has_value() && unique_queue_family == indices.graphicsFamily.value()) {
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
    if (vmaCreateAllocator(&allocatorInfo, &allocator) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create VMA allocator");
    }
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
