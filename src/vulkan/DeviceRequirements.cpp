#include "DeviceRequirements.hpp"

#include <algorithm>

namespace vkgs::vulkan {

QueueSelection
selectQueueFamilies(std::span<const QueueFamilyCapabilities> queueFamilies, const DeviceRequirements& requirements) {
    QueueSelection selection;

    if (requirements.unifiedGraphicsComputeTimestampQueue) {
        for (uint32_t index = 0; index < queueFamilies.size(); ++index) {
            const auto& queue = queueFamilies[index];
            if (queue.graphics && queue.compute && queue.timestamps) {
                selection.graphicsFamily = index;
                selection.computeFamily = index;
                break;
            }
        }
    } else {
        for (uint32_t index = 0; index < queueFamilies.size(); ++index) {
            const auto& queue = queueFamilies[index];
            if (queue.compute && queue.timestamps) {
                selection.computeFamily = index;
                break;
            }
        }
    }

    for (uint32_t index = 0; index < queueFamilies.size(); ++index) {
        const auto& queue = queueFamilies[index];
        if (!selection.graphicsFamily.has_value() && queue.graphics) {
            selection.graphicsFamily = index;
        }
        if (!selection.computeFamily.has_value() && queue.compute) {
            selection.computeFamily = index;
        }
        if (!selection.presentFamily.has_value() && queue.present) {
            selection.presentFamily = index;
        }
    }
    return selection;
}

std::vector<std::string>
getUnsuitabilityReasons(const DeviceRequirements& requirements, const VulkanDeviceCapabilities& capabilities) {
    std::vector<std::string> reasons;
    if (capabilities.apiVersion < requirements.apiVersion) {
        reasons.push_back("Vulkan 1.2 is required");
    }
    for (const auto& requiredExtension : requirements.extensions) {
        if (std::find(capabilities.extensions.begin(), capabilities.extensions.end(), requiredExtension) ==
            capabilities.extensions.end()) {
            reasons.push_back("missing device extension " + requiredExtension);
        }
    }

    bool hasGraphicsQueue = false;
    bool hasComputeQueue = false;
    bool hasTimestampComputeQueue = false;
    bool hasUnifiedGraphicsComputeTimestampQueue = false;
    bool hasPresentQueue = false;
    for (const auto& queue : capabilities.queueFamilies) {
        hasGraphicsQueue |= queue.graphics;
        hasComputeQueue |= queue.compute;
        hasTimestampComputeQueue |= queue.compute && queue.timestamps;
        hasUnifiedGraphicsComputeTimestampQueue |= queue.graphics && queue.compute && queue.timestamps;
        hasPresentQueue |= queue.present;
    }
    if (!hasComputeQueue) {
        reasons.push_back("missing compute queue family");
    }
    if (requirements.graphicsQueue && !hasGraphicsQueue) {
        reasons.push_back("missing graphics queue family");
    }
    if (requirements.timestampComputeQueue && !hasTimestampComputeQueue) {
        reasons.push_back("compute queue family does not support timestamps");
    }
    if (requirements.unifiedGraphicsComputeTimestampQueue && !hasUnifiedGraphicsComputeTimestampQueue) {
        reasons.push_back("on-screen mode requires one graphics+compute queue family with timestamp support");
    }
    if (requirements.presentQueue && !hasPresentQueue) {
        reasons.push_back("missing queue family with presentation support");
    }

    if (capabilities.maxComputeWorkgroupInvocations < requirements.computeWorkgroupSize ||
        capabilities.maxComputeWorkgroupSizeX < requirements.computeWorkgroupSize) {
        reasons.push_back("compute workgroup size 256 is not supported");
    }
    if (!capabilities.shaderStorageImageWriteWithoutFormat) {
        reasons.push_back("shaderStorageImageWriteWithoutFormat is not supported");
    }
    if (!capabilities.shaderInt64) {
        reasons.push_back("shaderInt64 is not supported");
    }
    if (requirements.dynamicRendering && !capabilities.dynamicRendering) {
        reasons.push_back("dynamic rendering is not supported");
    }
    if (requirements.offscreenStorageTransferFormat && !capabilities.offscreenStorageTransferFormat) {
        reasons.push_back("R8G8B8A8_UNORM optimal images require storage-image and transfer-source support");
    }
    if (requirements.presentQueue && !capabilities.surfaceHasFormatAndPresentMode) {
        reasons.push_back("surface has no supported format or present mode");
    }
    if (requirements.surfaceStorageColor && !capabilities.surfaceStorageColor) {
        reasons.push_back("surface images require color-attachment and storage-image usage");
    }
    return reasons;
}

} // namespace vkgs::vulkan
