#pragma once

#include <3dgs/OffscreenRenderer.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace vkgs::offscreen {

struct RenderFrame {
    std::string name;
    CameraPose camera;
    CameraProjection projection;
};

struct RenderJob {
    OffscreenConfig renderer;
    std::filesystem::path outputDirectory;
    std::string filenamePattern;
    std::vector<RenderFrame> frames;
};

[[nodiscard]] RenderJob loadRenderJob(
    const std::filesystem::path& configPath,
    std::optional<std::filesystem::path> outputDirectoryOverride = std::nullopt,
    std::optional<uint8_t> physicalDeviceOverride = std::nullopt,
    bool enableValidation = false
);

} // namespace vkgs::offscreen
