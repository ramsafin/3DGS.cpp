#include <vulkan/vulkan.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE

namespace {

constexpr uint32_t kRendererRequiredApiVersion = VK_API_VERSION_1_2;
constexpr uint32_t kRendererRequiredWorkgroupSize = 256;

struct CliOptions {
    bool verboseExtensions = false;
    std::optional<std::filesystem::path> jsonPath;
};

std::string versionString(uint32_t version) {
    std::ostringstream out;
    out << VK_VERSION_MAJOR(version) << '.' << VK_VERSION_MINOR(version) << '.' << VK_VERSION_PATCH(version);
    return out.str();
}

std::string yesNo(bool value) {
    return value ? "yes" : "no";
}

std::string supportedMissing(bool value) {
    return value ? "supported" : "missing";
}

std::string hexString(uint32_t value) {
    std::ostringstream out;
    out << "0x" << std::hex << std::setw(4) << std::setfill('0') << value;
    return out.str();
}

bool hasExtension(const std::vector<std::string>& extensions, std::string_view name) {
    return std::find(extensions.begin(), extensions.end(), name) != extensions.end();
}

std::string joinReasons(const std::vector<std::string>& reasons) {
    std::ostringstream out;
    for (size_t index = 0; index < reasons.size(); ++index) {
        if (index > 0) {
            out << "; ";
        }
        out << reasons[index];
    }
    return out.str();
}

CliOptions parseCli(int argc, char** argv) {
    CliOptions options{};
    for (int index = 1; index < argc; ++index) {
        const std::string arg = argv[index];
        if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [--verbose-extensions] [--json <path>]\n";
            std::exit(0);
        }
        if (arg == "--verbose-extensions") {
            options.verboseExtensions = true;
            continue;
        }
        if (arg == "--json") {
            if (++index >= argc) {
                throw std::runtime_error("--json requires an output path");
            }
            options.jsonPath = std::filesystem::path(argv[index]);
            continue;
        }
        throw std::runtime_error("Unknown argument: " + arg);
    }
    return options;
}

std::vector<std::string> enumerateInstanceExtensions() {
    std::vector<std::string> extensions;
    for (const auto& extension : vk::enumerateInstanceExtensionProperties()) {
        extensions.emplace_back(extension.extensionName.data());
    }
    std::sort(extensions.begin(), extensions.end());
    return extensions;
}

std::vector<std::string> enumerateDeviceExtensions(vk::PhysicalDevice device) {
    std::vector<std::string> extensions;
    for (const auto& extension : device.enumerateDeviceExtensionProperties()) {
        extensions.emplace_back(extension.extensionName.data());
    }
    std::sort(extensions.begin(), extensions.end());
    return extensions;
}

void printExtensionPresence(const std::vector<std::string>& extensions, std::string_view name) {
    std::cout << "    " << name << ": " << supportedMissing(hasExtension(extensions, name)) << '\n';
}

void printExtensionList(const std::vector<std::string>& extensions, std::string_view indent) {
    for (const auto& extension : extensions) {
        std::cout << indent << extension << '\n';
    }
}

bool hasOptimalFormatFeatures(vk::PhysicalDevice device, vk::Format format, vk::FormatFeatureFlags required) {
    const auto properties = device.getFormatProperties(format);
    return (properties.optimalTilingFeatures & required) == required;
}

void printFormatSupport(vk::PhysicalDevice device, vk::Format format, std::string_view name) {
    const auto properties = device.getFormatProperties(format);
    const auto storageTransfer = vk::FormatFeatureFlagBits::eStorageImage | vk::FormatFeatureFlagBits::eTransferSrc;
    std::cout << "    " << name << ":\n";
    std::cout << "      linear:  " << vk::to_string(properties.linearTilingFeatures) << '\n';
    std::cout << "      optimal: " << vk::to_string(properties.optimalTilingFeatures) << '\n';
    std::cout << "      buffer:  " << vk::to_string(properties.bufferFeatures) << '\n';
    std::cout << "      renderer storage+transfer-src: "
              << supportedMissing((properties.optimalTilingFeatures & storageTransfer) == storageTransfer) << '\n';
}

std::vector<std::string> rendererUnsuitabilityReasons(
    vk::PhysicalDevice device,
    const vk::PhysicalDeviceProperties& properties,
    const vk::PhysicalDeviceFeatures2& features2
) {
    std::vector<std::string> reasons;
    if (properties.apiVersion < kRendererRequiredApiVersion) {
        reasons.push_back("Vulkan 1.2 is required");
    }

    bool hasComputeQueue = false;
    for (const auto& queueFamily : device.getQueueFamilyProperties()) {
        hasComputeQueue |= static_cast<bool>(queueFamily.queueFlags & vk::QueueFlagBits::eCompute);
    }
    if (!hasComputeQueue) {
        reasons.push_back("missing compute queue family");
    }

    if (properties.limits.maxComputeWorkGroupInvocations < kRendererRequiredWorkgroupSize ||
        properties.limits.maxComputeWorkGroupSize[0] < kRendererRequiredWorkgroupSize) {
        reasons.push_back("compute workgroup size 256 is not supported");
    }
    if (!features2.features.shaderStorageImageWriteWithoutFormat) {
        reasons.push_back("shaderStorageImageWriteWithoutFormat is not supported");
    }
    const auto storageTransfer = vk::FormatFeatureFlagBits::eStorageImage | vk::FormatFeatureFlagBits::eTransferSrc;
    if (!hasOptimalFormatFeatures(device, vk::Format::eR8G8B8A8Unorm, storageTransfer)) {
        reasons.push_back("R8G8B8A8_UNORM optimal images require storage-image and transfer-source support");
    }
    return reasons;
}

void printQueueFamilies(vk::PhysicalDevice device) {
    const auto queueFamilies = device.getQueueFamilyProperties();
    std::cout << "  Queue families:\n";
    for (uint32_t index = 0; index < queueFamilies.size(); ++index) {
        const auto& queueFamily = queueFamilies[index];
        std::cout << "    [" << index << "] flags=" << vk::to_string(queueFamily.queueFlags)
                  << ", count=" << queueFamily.queueCount << ", timestampValidBits=" << queueFamily.timestampValidBits
                  << ", minImageTransferGranularity=(" << queueFamily.minImageTransferGranularity.width << ", "
                  << queueFamily.minImageTransferGranularity.height << ", "
                  << queueFamily.minImageTransferGranularity.depth << ")\n";
    }
}

void printLimits(const vk::PhysicalDeviceLimits& limits) {
    std::cout << "  Limits:\n";
    std::cout << "    maxComputeWorkGroupInvocations: " << limits.maxComputeWorkGroupInvocations << '\n';
    std::cout << "    maxComputeWorkGroupSize: [" << limits.maxComputeWorkGroupSize[0] << ", "
              << limits.maxComputeWorkGroupSize[1] << ", " << limits.maxComputeWorkGroupSize[2] << "]\n";
    std::cout << "    maxComputeWorkGroupCount: [" << limits.maxComputeWorkGroupCount[0] << ", "
              << limits.maxComputeWorkGroupCount[1] << ", " << limits.maxComputeWorkGroupCount[2] << "]\n";
    std::cout << "    maxStorageBufferRange: " << limits.maxStorageBufferRange << '\n';
    std::cout << "    maxPerStageDescriptorStorageBuffers: " << limits.maxPerStageDescriptorStorageBuffers << '\n';
    std::cout << "    maxPerStageDescriptorStorageImages: " << limits.maxPerStageDescriptorStorageImages << '\n';
    std::cout << "    timestampPeriod: " << limits.timestampPeriod << " ns\n";
}

std::string sortKeyModeName(
    const vk::PhysicalDeviceFeatures2& features2,
    const vk::PhysicalDeviceVulkan12Features& features12,
    const vk::PhysicalDeviceSubgroupProperties& subgroupProperties
) {
    const auto requiredSubgroupOperations = vk::SubgroupFeatureFlagBits::eBasic |
                                            vk::SubgroupFeatureFlagBits::eArithmetic |
                                            vk::SubgroupFeatureFlagBits::eBallot;
    const bool fastUInt64 =
        features2.features.shaderInt64 && features12.shaderSharedInt64Atomics &&
        (subgroupProperties.supportedStages & vk::ShaderStageFlagBits::eCompute) &&
        (subgroupProperties.supportedOperations & requiredSubgroupOperations) == requiredSubgroupOperations &&
        subgroupProperties.subgroupSize == 32;
    if (fastUInt64) {
        return "uint64_fast_subgroup32";
    }
    if (features2.features.shaderInt64) {
        return "uint64_portable";
    }
    return "uint32_pair_portable";
}

nlohmann::json formatSupportJson(vk::PhysicalDevice device, vk::Format format) {
    const auto properties = device.getFormatProperties(format);
    const auto storageTransfer = vk::FormatFeatureFlagBits::eStorageImage | vk::FormatFeatureFlagBits::eTransferSrc;
    return {
        {"linear_tiling_features", vk::to_string(properties.linearTilingFeatures)},
        {"optimal_tiling_features", vk::to_string(properties.optimalTilingFeatures)},
        {"buffer_features", vk::to_string(properties.bufferFeatures)},
        {"renderer_storage_transfer_src", (properties.optimalTilingFeatures & storageTransfer) == storageTransfer}
    };
}

nlohmann::json queueFamiliesJson(vk::PhysicalDevice device) {
    nlohmann::json families = nlohmann::json::array();
    const auto queueFamilies = device.getQueueFamilyProperties();
    for (uint32_t index = 0; index < queueFamilies.size(); ++index) {
        const auto& queueFamily = queueFamilies[index];
        families.push_back(
            {{"index", index},
             {"flags", vk::to_string(queueFamily.queueFlags)},
             {"queue_count", queueFamily.queueCount},
             {"timestamp_valid_bits", queueFamily.timestampValidBits},
             {"min_image_transfer_granularity",
              {queueFamily.minImageTransferGranularity.width,
               queueFamily.minImageTransferGranularity.height,
               queueFamily.minImageTransferGranularity.depth}}}
        );
    }
    return families;
}

nlohmann::json limitsJson(const vk::PhysicalDeviceLimits& limits) {
    return {
        {"max_compute_work_group_invocations", limits.maxComputeWorkGroupInvocations},
        {"max_compute_work_group_size",
         {limits.maxComputeWorkGroupSize[0], limits.maxComputeWorkGroupSize[1], limits.maxComputeWorkGroupSize[2]}},
        {"max_compute_work_group_count",
         {limits.maxComputeWorkGroupCount[0], limits.maxComputeWorkGroupCount[1], limits.maxComputeWorkGroupCount[2]}},
        {"max_storage_buffer_range", limits.maxStorageBufferRange},
        {"max_per_stage_descriptor_storage_buffers", limits.maxPerStageDescriptorStorageBuffers},
        {"max_per_stage_descriptor_storage_images", limits.maxPerStageDescriptorStorageImages},
        {"timestamp_period_ns", limits.timestampPeriod}
    };
}

nlohmann::json deviceJson(size_t index, vk::PhysicalDevice device) {
    vk::PhysicalDeviceVulkan12Features features12{};
    vk::PhysicalDeviceFeatures2 features2{};
    features2.pNext = &features12;
    device.getFeatures2(&features2);

    vk::PhysicalDeviceSubgroupProperties subgroupProperties{};
    vk::PhysicalDeviceProperties2 properties2{};
    properties2.pNext = &subgroupProperties;
    device.getProperties2(&properties2);

    const auto properties = properties2.properties;
    const auto extensions = enumerateDeviceExtensions(device);
    const auto reasons = rendererUnsuitabilityReasons(device, properties, features2);

    return {
        {"index", index},
        {"name", properties.deviceName.data()},
        {"type", vk::to_string(properties.deviceType)},
        {"vendor_id", properties.vendorID},
        {"vendor_id_hex", hexString(properties.vendorID)},
        {"device_id", properties.deviceID},
        {"device_id_hex", hexString(properties.deviceID)},
        {"api_version", versionString(properties.apiVersion)},
        {"api_version_raw", properties.apiVersion},
        {"driver_version", versionString(properties.driverVersion)},
        {"driver_version_raw", properties.driverVersion},
        {"extensions",
         {{"all", extensions},
          {"checks",
           {{VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME,
             hasExtension(extensions, VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME)},
            {VK_KHR_SWAPCHAIN_EXTENSION_NAME, hasExtension(extensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME)},
            {VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
             hasExtension(extensions, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME)}}}}},
        {"features",
         {{"shader_int64", static_cast<bool>(features2.features.shaderInt64)},
          {"shader_storage_image_write_without_format",
           static_cast<bool>(features2.features.shaderStorageImageWriteWithoutFormat)},
          {"shader_shared_int64_atomics", static_cast<bool>(features12.shaderSharedInt64Atomics)}}},
        {"subgroup",
         {{"size", subgroupProperties.subgroupSize},
          {"supported_stages", vk::to_string(subgroupProperties.supportedStages)},
          {"supported_operations", vk::to_string(subgroupProperties.supportedOperations)},
          {"quad_operations_in_all_stages", static_cast<bool>(subgroupProperties.quadOperationsInAllStages)}}},
        {"queue_families", queueFamiliesJson(device)},
        {"limits", limitsJson(properties.limits)},
        {"formats",
         {{"VK_FORMAT_R8G8B8A8_UNORM", formatSupportJson(device, vk::Format::eR8G8B8A8Unorm)},
          {"VK_FORMAT_B8G8R8A8_UNORM", formatSupportJson(device, vk::Format::eB8G8R8A8Unorm)},
          {"VK_FORMAT_R16G16B16A16_SFLOAT", formatSupportJson(device, vk::Format::eR16G16B16A16Sfloat)}}},
        {"renderer_compatibility", {{"compatible", reasons.empty()}, {"reasons", reasons}}},
        {"selected_sort_key_mode", sortKeyModeName(features2, features12, subgroupProperties)}
    };
}

nlohmann::json
probeJson(const std::vector<std::string>& instanceExtensions, std::span<const vk::PhysicalDevice> devices) {
    nlohmann::json root;
    root["probe"] = {
        {"instance_api_version", versionString(VK_API_VERSION_1_2)},
        {"instance_api_version_raw", VK_API_VERSION_1_2},
        {"renderer_required_api_version", versionString(kRendererRequiredApiVersion)},
        {"renderer_required_api_version_raw", kRendererRequiredApiVersion},
        {"renderer_required_workgroup_size", kRendererRequiredWorkgroupSize}
    };
    root["instance_extensions"] = instanceExtensions;
    root["devices"] = nlohmann::json::array();
    for (size_t index = 0; index < devices.size(); ++index) {
        root["devices"].push_back(deviceJson(index, devices[index]));
    }
    return root;
}

void writeJsonReport(const std::filesystem::path& path, const nlohmann::json& report) {
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("Failed to open JSON output path: " + path.string());
    }
    output << std::setw(2) << report << '\n';
}

void printDevice(size_t index, vk::PhysicalDevice device, bool verboseExtensions) {
    vk::PhysicalDeviceVulkan12Features features12{};
    vk::PhysicalDeviceFeatures2 features2{};
    features2.pNext = &features12;
    device.getFeatures2(&features2);

    vk::PhysicalDeviceSubgroupProperties subgroupProperties{};
    vk::PhysicalDeviceProperties2 properties2{};
    properties2.pNext = &subgroupProperties;
    device.getProperties2(&properties2);

    const auto properties = properties2.properties;
    const auto extensions = enumerateDeviceExtensions(device);

    std::cout << "Device [" << index << "] " << properties.deviceName.data() << '\n';
    std::cout << "  Type: " << vk::to_string(properties.deviceType) << '\n';
    std::cout << "  Vendor ID: " << hexString(properties.vendorID) << '\n';
    std::cout << "  Device ID: " << hexString(properties.deviceID) << '\n';
    std::cout << "  API version: " << versionString(properties.apiVersion) << '\n';
    std::cout << "  Driver version: " << versionString(properties.driverVersion) << " (raw " << properties.driverVersion
              << ")\n";

    std::cout << "  Extension checks:\n";
    printExtensionPresence(extensions, VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME);
    printExtensionPresence(extensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    printExtensionPresence(extensions, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
    if (verboseExtensions) {
        std::cout << "  All device extensions:\n";
        printExtensionList(extensions, "    ");
    }

    std::cout << "  Feature checks:\n";
    std::cout << "    shaderInt64: " << yesNo(features2.features.shaderInt64) << '\n';
    std::cout << "    shaderStorageImageWriteWithoutFormat: "
              << yesNo(features2.features.shaderStorageImageWriteWithoutFormat) << '\n';
    std::cout << "    shaderSharedInt64Atomics: " << yesNo(features12.shaderSharedInt64Atomics) << '\n';
    std::cout << "  Selected sort key mode: " << sortKeyModeName(features2, features12, subgroupProperties) << '\n';

    std::cout << "  Subgroup properties:\n";
    std::cout << "    subgroupSize: " << subgroupProperties.subgroupSize << '\n';
    std::cout << "    supportedStages: " << vk::to_string(subgroupProperties.supportedStages) << '\n';
    std::cout << "    supportedOperations: " << vk::to_string(subgroupProperties.supportedOperations) << '\n';
    std::cout << "    quadOperationsInAllStages: " << yesNo(subgroupProperties.quadOperationsInAllStages) << '\n';

    printQueueFamilies(device);
    printLimits(properties.limits);

    std::cout << "  Off-screen format checks:\n";
    printFormatSupport(device, vk::Format::eR8G8B8A8Unorm, "VK_FORMAT_R8G8B8A8_UNORM");
    printFormatSupport(device, vk::Format::eB8G8R8A8Unorm, "VK_FORMAT_B8G8R8A8_UNORM");
    printFormatSupport(device, vk::Format::eR16G16B16A16Sfloat, "VK_FORMAT_R16G16B16A16_SFLOAT");

    const auto reasons = rendererUnsuitabilityReasons(device, properties, features2);
    std::cout << "  Current off-screen renderer compatibility: ";
    if (reasons.empty()) {
        std::cout << "compatible\n";
    } else {
        std::cout << "not compatible (" << joinReasons(reasons) << ")\n";
    }
    std::cout << '\n';
}

} // namespace

int main(int argc, char** argv) {
    try {
        const auto cli = parseCli(argc, argv);

        VULKAN_HPP_DEFAULT_DISPATCHER.init();

        std::cout << "Probe instance API version: " << versionString(VK_API_VERSION_1_2) << '\n';

        const auto instanceExtensions = enumerateInstanceExtensions();
        std::cout << "Instance extensions: " << instanceExtensions.size() << '\n';
        if (cli.verboseExtensions) {
            printExtensionList(instanceExtensions, "  ");
        }

        const vk::ApplicationInfo appInfo{
            "3DGS Vulkan Capability Probe",
            VK_MAKE_VERSION(1, 0, 0),
            "No Engine",
            VK_MAKE_VERSION(1, 0, 0),
            VK_API_VERSION_1_2
        };
        const vk::InstanceCreateInfo createInfo{{}, &appInfo};
        const auto instance = vk::createInstanceUnique(createInfo);
        VULKAN_HPP_DEFAULT_DISPATCHER.init(*instance);

        const auto devices = instance->enumeratePhysicalDevices();
        std::cout << "Physical devices: " << devices.size() << "\n\n";
        for (size_t index = 0; index < devices.size(); ++index) {
            printDevice(index, devices[index], cli.verboseExtensions);
        }
        if (cli.jsonPath.has_value()) {
            writeJsonReport(cli.jsonPath.value(), probeJson(instanceExtensions, devices));
            std::cout << "JSON report written to: " << cli.jsonPath->string() << '\n';
        }
        return devices.empty() ? 2 : 0;
    } catch (const std::exception& error) {
        std::cerr << "vkgs_caps failed: " << error.what() << '\n';
        return 1;
    }
}
