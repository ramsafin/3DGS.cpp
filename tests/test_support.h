#ifndef VKGS_TEST_SUPPORT_H
#define VKGS_TEST_SUPPORT_H

#include "vulkan/VulkanContext.h"

#include <cstdint>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace vkgs_test {

// Attempts to build a minimal headless Vulkan context with the same device
// features the renderer requests. The fast radix feature is enabled only when
// the selected device advertises it. Returns nullptr if no usable Vulkan
// device/driver is available so callers can skip.
inline std::shared_ptr<VulkanContext> makeHeadlessContext() {
    try {
        auto context = std::make_shared<VulkanContext>(std::vector<std::string>{}, std::vector<std::string>{}, false);
        context->createInstance();
        context->selectPhysicalDevice(std::nullopt, std::nullopt);

        vk::PhysicalDeviceFeatures pdf{};
        vk::PhysicalDeviceVulkan11Features pdf11{};
        vk::PhysicalDeviceVulkan12Features pdf12{};
        pdf.shaderStorageImageWriteWithoutFormat = VK_TRUE;
        pdf.shaderInt64 = VK_TRUE;
        pdf12.shaderSharedInt64Atomics =
            context->getRadixSortMode() == VulkanContext::RadixSortMode::FastSubgroup32;

        context->createLogicalDevice(pdf, pdf11, pdf12);
        context->createDescriptorPool(1);
        return context;
    } catch (const std::exception&) {
        return nullptr;
    }
}

// Explicit binary vertex layout consumed by PlyReader (62 floats per vertex).
struct PlyVertexRecord {
    float position[3];
    float normal[3];
    float shs[48];
    float opacity;
    float scale[3];
    float rotation[4];
};
static_assert(sizeof(PlyVertexRecord) == 62 * sizeof(float), "PLY record layout mismatch");

inline std::string binaryPlyHeader(uint64_t numVertices, const std::string& format = "binary_little_endian") {
    std::ostringstream out;
    out << "ply\n";
    out << "format " << format << " 1.0\n";
    out << "element vertex " << numVertices << "\n";
    out << "property float x\nproperty float y\nproperty float z\n";
    out << "property float nx\nproperty float ny\nproperty float nz\n";
    out << "property float f_dc_0\nproperty float f_dc_1\nproperty float f_dc_2\n";
    for (int i = 0; i < 45; ++i) {
        out << "property float f_rest_" << i << "\n";
    }
    out << "property float opacity\n";
    out << "property float scale_0\nproperty float scale_1\nproperty float scale_2\n";
    out << "property float rot_0\nproperty float rot_1\nproperty float rot_2\nproperty float rot_3\n";
    out << "end_header\n";
    return out.str();
}

// Writes a binary little-endian PLY whose payload matches PlyReader's expected
// disk record, so the file can be loaded by the renderer.
inline void writeBinaryPly(const std::string& path, const std::vector<PlyVertexRecord>& vertices) {
    std::ofstream out(path, std::ios::binary);
    out << binaryPlyHeader(vertices.size());
    out.write(reinterpret_cast<const char*>(vertices.data()),
              static_cast<std::streamsize>(vertices.size() * sizeof(PlyVertexRecord)));
}

// FNV-1a 64-bit hash for deterministic image regression comparisons.
inline uint64_t fnv1a(const std::vector<uint8_t>& data) {
    uint64_t hash = 1469598103934665603ull;
    for (uint8_t byte : data) {
        hash ^= byte;
        hash *= 1099511628211ull;
    }
    return hash;
}

} // namespace vkgs_test

#endif // VKGS_TEST_SUPPORT_H
