#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

namespace vkgs::vulkan {

struct QueueFamilyCapabilities {
    bool graphics = false;
    bool compute = false;
    bool timestamps = false;
    bool present = false;
};

struct QueueSelection {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> computeFamily;
    std::optional<uint32_t> presentFamily;
};

struct DeviceRequirements {
    uint32_t apiVersion = VK_API_VERSION_1_2;
    uint32_t computeWorkgroupSize = 256;
    bool graphicsQueue = false;
    bool timestampComputeQueue = false;
    bool unifiedGraphicsComputeTimestampQueue = false;
    bool presentQueue = false;
    bool dynamicRendering = false;
    bool offscreenStorageTransferFormat = false;
    bool surfaceStorageColor = false;
    bool shaderInt64 = false;
    std::vector<std::string> extensions;
};

struct VulkanDeviceCapabilities {
    uint32_t apiVersion = 0;
    uint32_t maxComputeWorkgroupInvocations = 0;
    uint32_t maxComputeWorkgroupSizeX = 0;
    bool shaderStorageImageWriteWithoutFormat = false;
    bool shaderInt64 = false;
    bool shaderSharedInt64Atomics = false;
    uint32_t subgroupSize = 0;
    bool subgroupCompute = false;
    bool subgroupBasic = false;
    bool subgroupArithmetic = false;
    bool subgroupBallot = false;
    bool dynamicRendering = false;
    bool offscreenStorageTransferFormat = false;
    bool surfaceHasFormatAndPresentMode = false;
    bool surfaceStorageColor = false;
    std::vector<std::string> extensions;
    std::vector<QueueFamilyCapabilities> queueFamilies;
};

[[nodiscard]] QueueSelection
selectQueueFamilies(std::span<const QueueFamilyCapabilities> queueFamilies, const DeviceRequirements& requirements);

[[nodiscard]] std::vector<std::string>
getUnsuitabilityReasons(const DeviceRequirements& requirements, const VulkanDeviceCapabilities& capabilities);

} // namespace vkgs::vulkan
