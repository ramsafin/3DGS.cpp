#include "RenderJob.hpp"

#include <array>
#include <fstream>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>

namespace vkgs::offscreen {
namespace {

namespace fs = std::filesystem;
using json = nlohmann::json;

json loadJson(const fs::path& path) {
    std::ifstream stream(path);
    if (!stream.is_open()) {
        throw std::runtime_error("Failed to open config: " + path.string());
    }
    json config;
    stream >> config;
    return config;
}

fs::path resolvePath(const fs::path& base, const fs::path& path) {
    return path.is_absolute() ? path : base / path;
}

std::array<float, 3> readVec3(const json& value, const std::string& field) {
    if (!value.is_array() || value.size() != 3) {
        throw std::runtime_error(field + " must be an array of 3 numbers");
    }
    return {value[0].get<float>(), value[1].get<float>(), value[2].get<float>()};
}

std::array<float, 4> readQuaternion(const json& value, const std::string& field) {
    if (!value.is_array() || value.size() != 4) {
        throw std::runtime_error(field + " must be an array of 4 numbers in [w, x, y, z] order");
    }
    return {value[0].get<float>(), value[1].get<float>(), value[2].get<float>(), value[3].get<float>()};
}

} // namespace

RenderJob loadRenderJob(const std::filesystem::path& configPath,
                        std::optional<std::filesystem::path> outputDirectoryOverride,
                        std::optional<uint8_t> physicalDeviceOverride, bool enableValidation) {
    const auto absoluteConfigPath = fs::absolute(configPath);
    const auto configBase = absoluteConfigPath.parent_path();
    const auto config = loadJson(absoluteConfigPath);
    const auto render = config.value("render", json::object());
    const auto output = config.value("output", json::object());
    const auto vulkan = config.value("vulkan", json::object());
    const auto frames = config.at("frames");
    if (!frames.is_array() || frames.empty()) {
        throw std::runtime_error("frames must be a non-empty array");
    }

    RenderJob job;
    job.renderer.scene = resolvePath(configBase, config.at("scene").get<std::string>()).string();
    if (!fs::exists(job.renderer.scene)) {
        throw std::runtime_error("Scene file does not exist: " + job.renderer.scene);
    }
    job.renderer.extent = {render.value("width", 1280u), render.value("height", 720u)};
    if (job.renderer.extent.width == 0 || job.renderer.extent.height == 0) {
        throw std::runtime_error("render.width and render.height must be positive");
    }
    job.renderer.projection = {render.value("fov_degrees", 45.0f), render.value("near", 0.2f),
                               render.value("far", 1000.0f)};
    job.renderer.enableVulkanValidationLayers = enableValidation || vulkan.value("validation", false);
    if (physicalDeviceOverride.has_value()) {
        job.renderer.physicalDeviceId = physicalDeviceOverride;
    } else if (vulkan.contains("physical_device") && !vulkan.at("physical_device").is_null()) {
        const auto deviceId = vulkan.at("physical_device").get<uint32_t>();
        if (deviceId > 255) {
            throw std::runtime_error("vulkan.physical_device must be in range 0..255");
        }
        job.renderer.physicalDeviceId = static_cast<uint8_t>(deviceId);
    }

    const std::string outputFormat = output.value("format", "ppm");
    if (outputFormat != "ppm") {
        throw std::runtime_error("Only ppm output is currently supported");
    }
    job.outputDirectory =
        outputDirectoryOverride.value_or(resolvePath(configBase, output.value("directory", "renders")));
    job.filenamePattern = output.value("filename_pattern", "frame_%04d.ppm");

    job.frames.reserve(frames.size());
    for (size_t index = 0; index < frames.size(); ++index) {
        const auto& frame = frames[index];
        RenderFrame renderFrame;
        renderFrame.name = frame.value("name", std::to_string(index));
        renderFrame.camera.position = readVec3(frame.at("position"), "frames[].position");
        renderFrame.camera.rotation = readQuaternion(frame.at("rotation_quat"), "frames[].rotation_quat");
        renderFrame.projection = {frame.value("fov_degrees", job.renderer.projection.horizontalFovDegrees),
                                  frame.value("near", job.renderer.projection.nearPlane),
                                  frame.value("far", job.renderer.projection.farPlane)};
        job.frames.push_back(std::move(renderFrame));
    }
    return job;
}

} // namespace vkgs::offscreen
