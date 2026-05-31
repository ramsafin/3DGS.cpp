#ifndef VULKAN_SPLATTING_BUFFER_H
#define VULKAN_SPLATTING_BUFFER_H

#include "VulkanContext.hpp"
#include "vk_mem_alloc.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

class DescriptorSet;

class Buffer : public std::enable_shared_from_this<Buffer> {
  public:
    Buffer(
        const std::shared_ptr<VulkanContext>& context,
        vk::DeviceSize size,
        vk::BufferUsageFlags usage,
        VmaMemoryUsage vmaUsage,
        VmaAllocationCreateFlags flags,
        bool concurrentSharing = false,
        VkDeviceSize alignment = 0,
        std::string debugName = "Unnamed"
    );

    Buffer(const Buffer&) = delete;

    Buffer(Buffer&&) = delete;

    Buffer& operator=(const Buffer&) = delete;

    Buffer& operator=(Buffer&&) = delete;

    ~Buffer();

    void realloc(vk::DeviceSize newSize);

    void boundToDescriptorSet(
        std::weak_ptr<DescriptorSet> descriptorSet,
        uint32_t set,
        uint32_t binding,
        vk::DescriptorType type
    );

    static std::shared_ptr<Buffer>
    uniform(std::shared_ptr<VulkanContext> context, vk::DeviceSize size, bool concurrentSharing = false);

    static std::shared_ptr<Buffer> staging(std::shared_ptr<VulkanContext> context, vk::DeviceSize size);

    static std::shared_ptr<Buffer> storage(
        std::shared_ptr<VulkanContext> context,
        vk::DeviceSize size,
        bool concurrentSharing = false,
        vk::DeviceSize alignment = 0,
        std::string debugName = "Unnamed Storage Buffer"
    );

    void upload(std::span<const std::byte> data, vk::DeviceSize offset = 0);

    template <typename T, size_t Extent>
        requires std::is_trivially_copyable_v<T>
    void upload(std::span<T, Extent> data, vk::DeviceSize offset = 0) {
        upload(std::as_bytes(data), offset);
    }

    template <typename T>
        requires std::is_trivially_copyable_v<T>
    void uploadObject(const T& value, vk::DeviceSize offset = 0) {
        upload(std::as_bytes(std::span(&value, size_t{1})), offset);
    }

    void uploadFrom(
        const std::shared_ptr<Buffer>& buffer,
        vk::DeviceSize srcOffset = 0,
        vk::DeviceSize dstOffset = 0,
        vk::DeviceSize count = VK_WHOLE_SIZE
    );

    std::vector<char> download();

    void downloadTo(
        const std::shared_ptr<Buffer>& buffer,
        vk::DeviceSize srcOffset = 0,
        vk::DeviceSize dstOffset = 0,
        vk::DeviceSize count = VK_WHOLE_SIZE
    );

    void assertEquals(std::span<const std::byte> expected);

    template <typename T>
    T readOne(vk::DeviceSize offset = 0) {
        validateRange(offset, sizeof(T), size, "Buffer read");
        if (vmaUsage == VMA_MEMORY_USAGE_GPU_ONLY || vmaUsage == VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE) {
            const auto stagingBuffer = Buffer::staging(context, sizeof(T));
            downloadTo(stagingBuffer, offset, 0, sizeof(T));
            T result;
            std::memcpy(&result, stagingBuffer->allocation_info.pMappedData, sizeof(T));
            return result;
        } else if (flags & VMA_ALLOCATION_CREATE_MAPPED_BIT) {
            invalidate(offset, sizeof(T));
            T result;
            std::memcpy(&result, static_cast<const char*>(allocation_info.pMappedData) + offset, sizeof(T));
            return result;
        } else {
            throw std::runtime_error("Buffer is not mappable");
        }
    }

    void computeWriteReadBarrier(vk::CommandBuffer commandBuffer);
    void computeReadWriteBarrier(vk::CommandBuffer commandBuffer);
    void computeWriteWriteBarrier(vk::CommandBuffer commandBuffer);

    // Orders a compute-shader write before a subsequent transfer (copy) read.
    void computeToTransferReadBarrier(vk::CommandBuffer commandBuffer);
    // Orders a transfer (copy) write before a subsequent compute-shader read.
    void transferToComputeReadBarrier(vk::CommandBuffer commandBuffer);

    // Flush host writes / invalidate host caches for mapped allocations
    // (VKGS-013). No-ops on coherent memory, so always safe to call.
    void flush(vk::DeviceSize offset = 0, vk::DeviceSize size = VK_WHOLE_SIZE);
    void invalidate(vk::DeviceSize offset = 0, vk::DeviceSize size = VK_WHOLE_SIZE);

    vk::DeviceSize size;
    vk::BufferUsageFlags usage;
    vk::DeviceSize alignment;
    bool shared;

    vk::Buffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo allocation_info;

    VmaMemoryUsage vmaUsage;
    VmaAllocationCreateFlags flags;

  private:
    struct Allocation {
        vk::Buffer buffer;
        VmaAllocation allocation = nullptr;
        VmaAllocationInfo info{};
    };

    [[nodiscard]] Allocation allocate(vk::DeviceSize allocationSize) const;

    static void
    validateRange(vk::DeviceSize offset, vk::DeviceSize count, vk::DeviceSize limit, const std::string& context);

    Buffer createStagingBuffer(vk::DeviceSize size);
    std::shared_ptr<VulkanContext> context;

    std::vector<std::tuple<std::weak_ptr<DescriptorSet>, uint32_t, uint32_t, vk::DescriptorType>> boundDescriptorSets;
    std::string debugName;
};

#endif // VULKAN_SPLATTING_BUFFER_H
