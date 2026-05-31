#ifndef VKGS_OFFSCREEN_RENDERER_H
#define VKGS_OFFSCREEN_RENDERER_H

#include "Types.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace vkgs {

struct OffscreenConfig {
    std::string scene;
    Extent2D extent;
    CameraProjection projection;
    bool enableVulkanValidationLayers = false;
    std::optional<uint8_t> physicalDeviceId;
};

class OffscreenRenderer {
  public:
    explicit OffscreenRenderer(OffscreenConfig configuration);
    ~OffscreenRenderer();

    OffscreenRenderer(const OffscreenRenderer&) = delete;
    OffscreenRenderer& operator=(const OffscreenRenderer&) = delete;
    OffscreenRenderer(OffscreenRenderer&&) noexcept;
    OffscreenRenderer& operator=(OffscreenRenderer&&) noexcept;

    void render(const CameraPose& camera, std::optional<CameraProjection> projection = std::nullopt);
    [[nodiscard]] std::vector<uint8_t> readPixels() const;

  private:
    class Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace vkgs

#endif // VKGS_OFFSCREEN_RENDERER_H
