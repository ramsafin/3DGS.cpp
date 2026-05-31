#ifndef VKGS_RENDER_RENDERER_CONFIGURATION_H
#define VKGS_RENDER_RENDERER_CONFIGURATION_H

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

class Window;

namespace vkgs::render {

struct RendererConfiguration {
    bool enableVulkanValidationLayers = false;
    std::optional<uint8_t> physicalDeviceId;
    bool immediateSwapchain = false;
    std::string scene;
    float fov = 45.0f;
    float nearPlane = 0.2f;
    float farPlane = 1000.0f;
    bool enableGui = false;
    uint32_t width = 1280;
    uint32_t height = 720;
    std::shared_ptr<Window> window;
};

} // namespace vkgs::render

#endif // VKGS_RENDER_RENDERER_CONFIGURATION_H
