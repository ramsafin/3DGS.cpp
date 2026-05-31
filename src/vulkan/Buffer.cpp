#include "Buffer.hpp"

#include "BarrierBuilder.hpp"
#include "DescriptorSet.hpp"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

Buffer::Allocation Buffer::allocate(vk::DeviceSize allocationSize) const {
    if (allocationSize == 0) {
        throw std::runtime_error("Cannot allocate a zero-sized buffer");
    }

    auto bufferInfo = vk::BufferCreateInfo().setSize(allocationSize).setUsage(usage);
    std::vector<uint32_t> queueFamilyIndices;
    if (shared) {
        const auto compute = context->queues.find(VulkanContext::Queue::Type::COMPUTE);
        if (compute == context->queues.end()) {
            throw std::runtime_error("Cannot share buffer without a compute queue");
        }
        queueFamilyIndices.push_back(compute->second.queueFamily);

        const auto graphics = context->queues.find(VulkanContext::Queue::Type::GRAPHICS);
        if (graphics != context->queues.end() &&
            std::find(queueFamilyIndices.begin(), queueFamilyIndices.end(), graphics->second.queueFamily) ==
                queueFamilyIndices.end()) {
            queueFamilyIndices.push_back(graphics->second.queueFamily);
        }
    }
    if (queueFamilyIndices.size() > 1) {
        bufferInfo.setSharingMode(vk::SharingMode::eConcurrent)
            .setQueueFamilyIndexCount(static_cast<uint32_t>(queueFamilyIndices.size()))
            .setPQueueFamilyIndices(queueFamilyIndices.data());
    } else {
        bufferInfo.setSharingMode(vk::SharingMode::eExclusive);
    }

    auto vkBufferInfo = static_cast<VkBufferCreateInfo>(bufferInfo);

    VmaAllocationCreateInfo allocInfo = {};
    allocInfo.usage = vmaUsage;
    allocInfo.flags = flags;

    Allocation result;
    VkBuffer vkBuffer = VK_NULL_HANDLE;

    VkResult res;
    if (alignment != 0) {
        res = vmaCreateBufferWithAlignment(
            context->allocator,
            &vkBufferInfo,
            &allocInfo,
            alignment,
            &vkBuffer,
            &result.allocation,
            &result.info
        );
    } else {
        res =
            vmaCreateBuffer(context->allocator, &vkBufferInfo, &allocInfo, &vkBuffer, &result.allocation, &result.info);
    }
    if (res != VK_SUCCESS) {
        throw std::runtime_error(
            "Failed to create buffer '" + debugName + "' (" + std::to_string(allocationSize) + " bytes)"
        );
    }
    result.buffer = vk::Buffer(vkBuffer);

    if (context->validationLayersEnabled) {
        context->device->setDebugUtilsObjectNameEXT(
            vk::DebugUtilsObjectNameInfoEXT{
                vk::ObjectType::eBuffer,
                reinterpret_cast<uint64_t>(vkBuffer),
                debugName.c_str()
            }
        );
    }
    return result;
}

void Buffer::validateRange(
    vk::DeviceSize offset,
    vk::DeviceSize count,
    vk::DeviceSize limit,
    const std::string& context
) {
    if (offset > limit || count > limit - offset) {
        throw std::runtime_error(context + " range out of bounds");
    }
}

Buffer::Buffer(
    const std::shared_ptr<VulkanContext>& _context,
    vk::DeviceSize size,
    vk::BufferUsageFlags usage,
    VmaMemoryUsage vmaUsage,
    VmaAllocationCreateFlags flags,
    bool shared,
    vk::DeviceSize alignment,
    std::string debugName
)
    : context(_context)
    , size(size)
    , alignment(alignment)
    , shared(shared)
    , usage(usage)
    , vmaUsage(vmaUsage)
    , flags(flags)
    , buffer()
    , allocation(nullptr)
    , allocation_info()
    , debugName(std::move(debugName)) {
    auto created = allocate(size);
    buffer = created.buffer;
    allocation = created.allocation;
    allocation_info = created.info;
}

Buffer Buffer::createStagingBuffer(vk::DeviceSize size) {
    return Buffer(
        context,
        size,
        vk::BufferUsageFlagBits::eTransferSrc,
        VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        false
    );
}

void Buffer::upload(std::span<const std::byte> data, vk::DeviceSize offset) {
    const auto size = static_cast<vk::DeviceSize>(data.size_bytes());
    validateRange(offset, size, this->size, "Buffer upload");
    if (size == 0) {
        return;
    }

    if (vmaUsage == VMA_MEMORY_USAGE_GPU_ONLY || vmaUsage == VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE) {
        auto stagingBuffer = createStagingBuffer(size);
        memcpy(stagingBuffer.allocation_info.pMappedData, data.data(), size);
        stagingBuffer.flush();
        auto commandBuffer = context->beginOneTimeCommandBuffer();
        vk::BufferCopy copyRegion = {};
        copyRegion.setDstOffset(offset).setSize(size);
        commandBuffer->copyBuffer(stagingBuffer.buffer, buffer, 1, &copyRegion);
        context->endOneTimeCommandBuffer(std::move(commandBuffer), VulkanContext::Queue::COMPUTE);
    } else if (flags & VMA_ALLOCATION_CREATE_MAPPED_BIT) {
        memcpy(static_cast<char*>(allocation_info.pMappedData) + offset, data.data(), size);
        flush(offset, size);
    } else {
        throw std::runtime_error("Buffer is not mappable");
    }
}

void Buffer::uploadFrom(
    const std::shared_ptr<Buffer>& buffer,
    vk::DeviceSize srcOffset,
    vk::DeviceSize dstOffset,
    vk::DeviceSize count
) {
    if (count == VK_WHOLE_SIZE) {
        if (srcOffset > buffer->size) {
            throw std::runtime_error("Buffer upload source offset out of range");
        }
        count = buffer->size - srcOffset;
    }
    validateRange(srcOffset, count, buffer->size, "Buffer upload source");
    validateRange(dstOffset, count, size, "Buffer upload destination");
    if (count == 0) {
        return;
    }

    if (vmaUsage == VMA_MEMORY_USAGE_GPU_ONLY || vmaUsage == VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE) {
        if (buffer->flags & VMA_ALLOCATION_CREATE_MAPPED_BIT) {
            buffer->flush(srcOffset, count);
        }
        auto commandBuffer = context->beginOneTimeCommandBuffer();
        vk::BufferCopy copyRegion = {};
        copyRegion.setSrcOffset(srcOffset).setDstOffset(dstOffset).setSize(count);
        commandBuffer->copyBuffer(buffer->buffer, this->buffer, 1, &copyRegion);
        context->endOneTimeCommandBuffer(std::move(commandBuffer), VulkanContext::Queue::COMPUTE);
    } else if (flags & VMA_ALLOCATION_CREATE_MAPPED_BIT) {
        if (!(buffer->flags & VMA_ALLOCATION_CREATE_MAPPED_BIT)) {
            throw std::runtime_error("Buffer upload source is not mappable");
        }
        buffer->invalidate(srcOffset, count);
        memcpy(
            static_cast<char*>(allocation_info.pMappedData) + dstOffset,
            static_cast<const char*>(buffer->allocation_info.pMappedData) + srcOffset,
            count
        );
        flush(dstOffset, count);
    } else {
        throw std::runtime_error("Buffer is not mappable");
    }
}

void Buffer::downloadTo(
    const std::shared_ptr<Buffer>& buffer,
    vk::DeviceSize srcOffset,
    vk::DeviceSize dstOffset,
    vk::DeviceSize count
) {
    if (count == VK_WHOLE_SIZE) {
        if (dstOffset > buffer->size) {
            throw std::runtime_error("Buffer download destination offset out of range");
        }
        count = buffer->size - dstOffset;
    }
    validateRange(srcOffset, count, this->size, "Buffer download source");
    validateRange(dstOffset, count, buffer->size, "Buffer download destination");
    if (count == 0) {
        return;
    }

    if (vmaUsage == VMA_MEMORY_USAGE_GPU_ONLY || vmaUsage == VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE) {
        auto commandBuffer = context->beginOneTimeCommandBuffer();
        vk::BufferCopy copyRegion = {};
        copyRegion.setSrcOffset(srcOffset).setDstOffset(dstOffset).setSize(count);
        commandBuffer->copyBuffer(this->buffer, buffer->buffer, 1, &copyRegion);
        context->endOneTimeCommandBuffer(std::move(commandBuffer), VulkanContext::Queue::COMPUTE);
        // Make the freshly copied bytes visible to host reads of the destination.
        if (buffer->flags & VMA_ALLOCATION_CREATE_MAPPED_BIT) {
            buffer->invalidate(dstOffset, count);
        }
    } else if (flags & VMA_ALLOCATION_CREATE_MAPPED_BIT) {
        if (!(buffer->flags & VMA_ALLOCATION_CREATE_MAPPED_BIT)) {
            throw std::runtime_error("Buffer download destination is not mappable");
        }
        invalidate(srcOffset, count);
        memcpy(
            static_cast<char*>(buffer->allocation_info.pMappedData) + dstOffset,
            static_cast<char*>(allocation_info.pMappedData) + srcOffset,
            count
        );
        buffer->flush(dstOffset, count);
    } else {
        throw std::runtime_error("Buffer is not mappable");
    }
}

void Buffer::flush(vk::DeviceSize offset, vk::DeviceSize size) {
    vmaFlushAllocation(context->allocator, allocation, offset, size);
}

void Buffer::invalidate(vk::DeviceSize offset, vk::DeviceSize size) {
    vmaInvalidateAllocation(context->allocator, allocation, offset, size);
}

Buffer::~Buffer() {
    if (allocation != nullptr) {
        vmaDestroyBuffer(context->allocator, static_cast<VkBuffer>(buffer), allocation);
        spdlog::debug("Buffer destroyed");
    }
}

void Buffer::realloc(vk::DeviceSize newSize) {
    for (auto& [descriptorSet, set, binding, type] : boundDescriptorSets) {
        if (auto descriptor = descriptorSet.lock(); descriptor && set >= descriptor->descriptorSets.size()) {
            throw std::runtime_error("Descriptor backlink index out of range for binding " + std::to_string(binding));
        }
    }

    auto created = allocate(newSize);
    const auto oldBuffer = buffer;
    const auto oldAllocation = allocation;
    size = newSize;
    buffer = created.buffer;
    allocation = created.allocation;
    allocation_info = created.info;

    // Descriptor buffer offsets are relative to the start of the VkBuffer, not
    // the backing VMA allocation (VKGS-012). Always bind from offset zero.
    vk::DescriptorBufferInfo bufferInfo(buffer, 0, size);

    std::vector<vk::WriteDescriptorSet> writeDescriptorSets;
    for (auto& [descriptorSet, set, binding, type] : boundDescriptorSets) {
        if (auto descriptor = descriptorSet.lock()) {
            writeDescriptorSets
                .emplace_back(descriptor->descriptorSets[set].get(), binding, 0, 1, type, nullptr, &bufferInfo);
        }
    }
    if (!writeDescriptorSets.empty()) {
        context->device->updateDescriptorSets(writeDescriptorSets, nullptr);
    }
    vmaDestroyBuffer(context->allocator, static_cast<VkBuffer>(oldBuffer), oldAllocation);
}

void Buffer::boundToDescriptorSet(
    std::weak_ptr<DescriptorSet> descriptorSet,
    uint32_t set,
    uint32_t binding,
    vk::DescriptorType type
) {
    boundDescriptorSets.push_back({descriptorSet, set, binding, type});
}

std::shared_ptr<Buffer>
Buffer::uniform(std::shared_ptr<VulkanContext> context, vk::DeviceSize size, bool concurrentSharing) {
    return std::make_shared<Buffer>(
        std::move(context),
        size,
        vk::BufferUsageFlagBits::eUniformBuffer,
        VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
        concurrentSharing
    );
}

std::shared_ptr<Buffer> Buffer::staging(std::shared_ptr<VulkanContext> context, vk::DeviceSize size) {
    return std::make_shared<Buffer>(
        context,
        size,
        vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst,
        VMA_MEMORY_USAGE_AUTO,
        VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        false
    );
}

std::shared_ptr<Buffer> Buffer::storage(
    std::shared_ptr<VulkanContext> context,
    vk::DeviceSize size,
    bool concurrentSharing,
    vk::DeviceSize alignment,
    std::string debugName
) {
    return std::make_shared<Buffer>(
        context,
        size,
        vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst |
            vk::BufferUsageFlagBits::eTransferSrc,
        VMA_MEMORY_USAGE_GPU_ONLY,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        concurrentSharing,
        alignment,
        debugName
    );
}

void Buffer::assertEquals(std::span<const std::byte> expected) {
    const auto length = static_cast<vk::DeviceSize>(expected.size_bytes());
    if (length > size) {
        throw std::runtime_error("Buffer overflow");
    }

    if (vmaUsage == VMA_MEMORY_USAGE_GPU_ONLY || vmaUsage == VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE) {
        auto stagingBuffer = Buffer::staging(context, length);
        downloadTo(stagingBuffer);
        if (memcmp(expected.data(), stagingBuffer->allocation_info.pMappedData, length) != 0) {
            throw std::runtime_error("Buffer content does not match");
        }
    } else if (flags & VMA_ALLOCATION_CREATE_MAPPED_BIT) {
        invalidate(0, length);
        if (memcmp(expected.data(), allocation_info.pMappedData, length) != 0) {
            throw std::runtime_error("Buffer content does not match");
        }
    } else {
        throw std::runtime_error("Buffer is not mappable");
    }
}

void Buffer::computeWriteReadBarrier(vk::CommandBuffer commandBuffer) {
    vkgs::vulkan::BarrierBuilder()
        .queueFamilyIndex(context->queues[VulkanContext::Queue::COMPUTE].queueFamily)
        .addBufferBarrier(shared_from_this(), vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderRead)
        .build(commandBuffer, vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader);
}

void Buffer::computeReadWriteBarrier(vk::CommandBuffer commandBuffer) {
    vkgs::vulkan::BarrierBuilder()
        .queueFamilyIndex(context->queues[VulkanContext::Queue::COMPUTE].queueFamily)
        .addBufferBarrier(shared_from_this(), vk::AccessFlagBits::eShaderRead, vk::AccessFlagBits::eShaderWrite)
        .build(commandBuffer, vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader);
}

void Buffer::computeWriteWriteBarrier(vk::CommandBuffer commandBuffer) {
    vkgs::vulkan::BarrierBuilder()
        .queueFamilyIndex(context->queues[VulkanContext::Queue::COMPUTE].queueFamily)
        .addBufferBarrier(shared_from_this(), vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eShaderWrite)
        .build(commandBuffer, vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eComputeShader);
}

void Buffer::computeToTransferReadBarrier(vk::CommandBuffer commandBuffer) {
    vkgs::vulkan::BarrierBuilder()
        .queueFamilyIndex(context->queues[VulkanContext::Queue::COMPUTE].queueFamily)
        .addBufferBarrier(shared_from_this(), vk::AccessFlagBits::eShaderWrite, vk::AccessFlagBits::eTransferRead)
        .build(commandBuffer, vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer);
}

void Buffer::transferToComputeReadBarrier(vk::CommandBuffer commandBuffer) {
    vkgs::vulkan::BarrierBuilder()
        .queueFamilyIndex(context->queues[VulkanContext::Queue::COMPUTE].queueFamily)
        .addBufferBarrier(shared_from_this(), vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderRead)
        .build(commandBuffer, vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader);
}

std::vector<char> Buffer::download() {
    auto stagingBuffer = Buffer::staging(context, size);
    downloadTo(stagingBuffer);
    return {
        (char*)stagingBuffer->allocation_info.pMappedData,
        ((char*)stagingBuffer->allocation_info.pMappedData) + size
    };
}
