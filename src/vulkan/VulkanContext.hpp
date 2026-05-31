#pragma once

#include "DeviceRequirements.hpp"
#include "vk_mem_alloc.h"

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include <vulkan/vulkan.hpp>

class VulkanContext {
  public:
    using QueueFamilyIndices = vkgs::vulkan::QueueSelection;

    // Number of timestamp queries the timestamp pool can hold. Centralized so
    // the pool size, the reset ranges, and the QueryManager id allocator stay in
    // agreement (VKGS-026).
    static constexpr uint32_t kTimestampQueryCount = 20;

    enum class RadixSortMode {
        FastSubgroup32,
        Portable
    };

    struct Queue {
        enum Type {
            GRAPHICS,
            COMPUTE,
            PRESENT
        };

        std::set<Type> types;
        uint32_t queueFamily;
        uint32_t queueIndex;
        vk::Queue queue;
    };

    VulkanContext(
        const std::vector<std::string>& instance_extensions,
        const std::vector<std::string>& device_extensions,
        bool validation_layers_enabled
    );

    VulkanContext(const VulkanContext&) = delete;

    VulkanContext(VulkanContext&&) = delete;

    VulkanContext& operator=(const VulkanContext&) = delete;

    VulkanContext& operator=(VulkanContext&&) = delete;

    void createInstance();

    bool isDeviceSuitable(vk::PhysicalDevice device, std::optional<vk::SurfaceKHR> surface = std::nullopt);

    std::vector<std::string> getDeviceUnsuitabilityReasons(
        vk::PhysicalDevice device,
        std::optional<vk::SurfaceKHR> surface = std::nullopt
    ) const;

    void selectPhysicalDevice(
        std::optional<uint8_t> id = std::nullopt,
        std::optional<vk::SurfaceKHR> surface = std::nullopt
    );

    VulkanContext::QueueFamilyIndices findQueueFamilies();

    void createQueryPool();

    void createLogicalDevice(
        vk::PhysicalDeviceFeatures deviceFeatures,
        vk::PhysicalDeviceVulkan11Features deviceFeatures11,
        vk::PhysicalDeviceVulkan12Features deviceFeatures12
    );

    void createDescriptorPool(uint8_t framesInFlight);

    [[nodiscard]] bool supportsTimestampQueries() const {
        return timestampQueriesSupported;
    }

    [[nodiscard]] RadixSortMode getRadixSortMode() const {
        return radixSortMode;
    }

    vk::UniqueCommandBuffer beginOneTimeCommandBuffer(Queue::Type queue = Queue::COMPUTE);

    void endOneTimeCommandBuffer(vk::UniqueCommandBuffer&& commandBuffer, Queue::Type queue);

    virtual ~VulkanContext();

    vk::UniqueInstance instance;
    vk::PhysicalDevice physicalDevice;
    std::optional<vk::UniqueSurfaceKHR> surface;
    vk::UniqueDevice device;
    std::unordered_map<Queue::Type, Queue> queues;
    VmaAllocator allocator = nullptr;

    vk::UniqueDescriptorPool descriptorPool;
    vk::UniqueQueryPool queryPool;

    bool validationLayersEnabled;

  private:
    std::vector<std::string> instanceExtensions;
    std::vector<std::string> deviceExtensions;

    // One transient command pool per queue family. One-time command buffers must
    // be allocated from a pool created for the family of the submission queue
    // (VKGS-001); using a single graphics-family pool is invalid when graphics
    // and compute families differ.
    std::unordered_map<uint32_t, vk::UniqueCommandPool> oneTimeCommandPools;
    bool timestampQueriesSupported = false;
    RadixSortMode radixSortMode = RadixSortMode::Portable;

    void setupVma();

    void updateSelectedDeviceCapabilities();

    [[nodiscard]] vkgs::vulkan::DeviceRequirements getDeviceRequirements(bool requirePresentation) const;

    [[nodiscard]] vkgs::vulkan::VulkanDeviceCapabilities
    inspectDeviceCapabilities(vk::PhysicalDevice device, std::optional<vk::SurfaceKHR> surface) const;

    vk::CommandPool getOneTimePool(uint32_t queueFamily);
};
