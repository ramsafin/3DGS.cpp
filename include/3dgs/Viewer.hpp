#pragma once

#include "Types.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace vkgs::viewer {

struct InputState {
    std::array<bool, 3> mouseButtons{};
    std::array<bool, 7> keys{};
    std::array<double, 2> cursorTranslation{};
    double scrollDelta = 0.0;
};

struct ViewerConfig {
    std::string scene;
    Extent2D extent;
    CameraProjection projection;
    bool enableVulkanValidationLayers = false;
    std::optional<uint8_t> physicalDeviceId;
    bool immediateSwapchain = false;
    bool enableGui = true;
};

class WindowAdapter {
  public:
    ~WindowAdapter();

    WindowAdapter(const WindowAdapter&) = delete;
    WindowAdapter& operator=(const WindowAdapter&) = delete;
    WindowAdapter(WindowAdapter&&) noexcept;
    WindowAdapter& operator=(WindowAdapter&&) noexcept;

  private:
    class Impl;
    explicit WindowAdapter(std::unique_ptr<Impl> impl);

    std::unique_ptr<Impl> impl;
    friend class Viewer;
    friend std::unique_ptr<WindowAdapter> makeGlfwWindow(std::string name, int width, int height);
};

[[nodiscard]] std::unique_ptr<WindowAdapter> makeGlfwWindow(std::string name, int width, int height);

class Viewer {
  public:
    Viewer(ViewerConfig configuration, std::unique_ptr<WindowAdapter> window);
    ~Viewer();

    Viewer(const Viewer&) = delete;
    Viewer& operator=(const Viewer&) = delete;
    Viewer(Viewer&&) noexcept;
    Viewer& operator=(Viewer&&) noexcept;

    void draw();
    void run();
    void stop();

  private:
    class Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace vkgs::viewer
