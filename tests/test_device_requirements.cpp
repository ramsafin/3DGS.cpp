#include "vulkan/DeviceRequirements.hpp"

#include <gtest/gtest.h>

#include <algorithm>

namespace {

vkgs::vulkan::VulkanDeviceCapabilities capableDevice() {
    vkgs::vulkan::VulkanDeviceCapabilities capabilities;
    capabilities.apiVersion = VK_API_VERSION_1_2;
    capabilities.maxComputeWorkgroupInvocations = 256;
    capabilities.maxComputeWorkgroupSizeX = 256;
    capabilities.shaderStorageImageWriteWithoutFormat = true;
    capabilities.shaderInt64 = true;
    capabilities.dynamicRendering = true;
    capabilities.offscreenStorageTransferFormat = true;
    capabilities.surfaceHasFormatAndPresentMode = true;
    capabilities.surfaceStorageColor = true;
    capabilities.queueFamilies.push_back({true, true, true, true});
    return capabilities;
}

bool contains(const std::vector<std::string>& reasons, const std::string& reason) {
    return std::find(reasons.begin(), reasons.end(), reason) != reasons.end();
}

} // namespace

TEST(DeviceRequirements, SelectsUnifiedQueueForViewer) {
    const std::vector<vkgs::vulkan::QueueFamilyCapabilities> queues = {
        {true, false, true, false}, {false, true, true, false}, {true, true, true, true}};
    vkgs::vulkan::DeviceRequirements requirements;
    requirements.unifiedGraphicsComputeTimestampQueue = true;

    const auto selected = vkgs::vulkan::selectQueueFamilies(queues, requirements);
    EXPECT_EQ(selected.graphicsFamily, 2u);
    EXPECT_EQ(selected.computeFamily, 2u);
    EXPECT_EQ(selected.presentFamily, 2u);
}

TEST(DeviceRequirements, PrefersTimestampComputeQueueForOffscreen) {
    const std::vector<vkgs::vulkan::QueueFamilyCapabilities> queues = {
        {false, true, false, false}, {false, true, true, false}};

    const auto selected = vkgs::vulkan::selectQueueFamilies(queues, {});
    EXPECT_EQ(selected.computeFamily, 1u);
}

TEST(DeviceRequirements, ReportsMissingViewerCapabilities) {
    auto capabilities = capableDevice();
    capabilities.queueFamilies = {{true, false, true, false}, {false, true, false, false}};
    capabilities.dynamicRendering = false;
    vkgs::vulkan::DeviceRequirements requirements;
    requirements.graphicsQueue = true;
    requirements.timestampComputeQueue = true;
    requirements.unifiedGraphicsComputeTimestampQueue = true;
    requirements.presentQueue = true;
    requirements.dynamicRendering = true;
    requirements.surfaceStorageColor = true;

    const auto reasons = vkgs::vulkan::getUnsuitabilityReasons(requirements, capabilities);
    EXPECT_TRUE(contains(reasons, "compute queue family does not support timestamps"));
    EXPECT_TRUE(contains(reasons, "on-screen mode requires one graphics+compute queue family with timestamp support"));
    EXPECT_TRUE(contains(reasons, "missing queue family with presentation support"));
    EXPECT_TRUE(contains(reasons, "dynamic rendering is not supported"));
}

TEST(DeviceRequirements, AcceptsCompleteCapabilities) {
    const auto reasons = vkgs::vulkan::getUnsuitabilityReasons({}, capableDevice());
    EXPECT_TRUE(reasons.empty());
}
