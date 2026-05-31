#ifndef VKGS_SESSION_OFFSCREEN_SESSION_H
#define VKGS_SESSION_OFFSCREEN_SESSION_H

#include <3dgs/OffscreenRenderer.hpp>

#include "Renderer.hpp"

namespace vkgs::session {

class OffscreenSession {
  public:
    explicit OffscreenSession(OffscreenConfig configuration);
    ~OffscreenSession();

    void render(const CameraPose& camera, std::optional<CameraProjection> projection);
    [[nodiscard]] std::vector<uint8_t> readPixels() const;

  private:
    Renderer renderer;
};

} // namespace vkgs::session

#endif // VKGS_SESSION_OFFSCREEN_SESSION_H
