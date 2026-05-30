#ifndef VULKANSPLATTING_H
#define VULKANSPLATTING_H

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

class Window;
class Renderer;

class VulkanSplatting {
  public:
    struct RendererConfiguration {
        bool enableVulkanValidationLayers = false;
        std::optional<uint8_t> physicalDeviceId = std::nullopt;
        bool immediateSwapchain = false;
        std::string scene;

        float fov = 45.0f;
        float near = 0.2f;
        float far = 1000.0f;
        bool enableGui = false;
        uint32_t width = 1280;
        uint32_t height = 720;

#ifdef VKGS_RENDER_MODE_ONSCREEN
        std::shared_ptr<Window> window;
#endif
    };

    explicit VulkanSplatting(RendererConfiguration configuration) : configuration(configuration) {}

#ifdef VKGS_ENABLE_GLFW
    static std::shared_ptr<Window> createGlfwWindow(std::string name, int width, int height);
#endif

    void start();

    void initialize();

    void draw();

    std::vector<uint8_t> readPixels();

    void setCameraPose(float px, float py, float pz, float qw, float qx, float qy, float qz);

    void setCameraProjection(float fovDegrees, float nearPlane, float farPlane);

    void logTranslation(float x, float y);

    void logMovement(float x, float y, float z);

    void stop();

  private:
    RendererConfiguration configuration;
    std::shared_ptr<Renderer> renderer;
};

#endif // VULKANSPLATTING_H
