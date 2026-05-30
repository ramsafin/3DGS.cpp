#ifndef VKGS_TEST_SUPPORT_H
#define VKGS_TEST_SUPPORT_H

#include "vulkan/VulkanContext.h"

#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace vkgs_test {

// Attempts to build a minimal headless Vulkan context with the same device
// features the renderer requests (shaderInt64 + shared 64-bit atomics). Returns
// nullptr if no usable Vulkan device/driver is available so callers can skip.
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
        pdf12.shaderSharedInt64Atomics = VK_TRUE;

        context->createLogicalDevice(pdf, pdf11, pdf12);
        context->createDescriptorPool(1);
        return context;
    } catch (const std::exception&) {
        return nullptr;
    }
}

// Native binary vertex layout consumed by GSScene::load (62 floats per vertex).
struct PlyVertexRecord {
    float position[3];
    float normal[3]; // must be zero; GSScene::load asserts this in debug builds
    float shs[48];
    float opacity;
    float scale[3];
    float rotation[4];
};
static_assert(sizeof(PlyVertexRecord) == 62 * sizeof(float), "PLY record layout mismatch");

// Writes a binary little-endian PLY whose payload matches GSScene's expected
// native struct, so the file can be loaded by the renderer.
inline void writeBinaryPly(const std::string& path, const std::vector<PlyVertexRecord>& vertices) {
    std::ofstream out(path, std::ios::binary);
    out << "ply\n";
    out << "format binary_little_endian 1.0\n";
    out << "element vertex " << vertices.size() << "\n";
    out << "property float x\n property float y\n property float z\n";
    out << "property float nx\n property float ny\n property float nz\n";
    out << "property float f_dc_0\n property float f_dc_1\n property float f_dc_2\n";
    for (int i = 0; i < 45; ++i) {
        out << "property float f_rest_" << i << "\n";
    }
    out << "property float opacity\n";
    out << "property float scale_0\n property float scale_1\n property float scale_2\n";
    out << "property float rot_0\n property float rot_1\n property float rot_2\n property float rot_3\n";
    out << "end_header\n";
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
