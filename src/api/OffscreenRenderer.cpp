#include <3dgs/OffscreenRenderer.hpp>

#include "session/OffscreenSession.hpp"

#include <utility>

namespace vkgs {

class OffscreenRenderer::Impl {
  public:
    explicit Impl(OffscreenConfig configuration)
        : session(std::move(configuration)) {
    }

    session::OffscreenSession session;
};

OffscreenRenderer::OffscreenRenderer(OffscreenConfig configuration)
    : impl(std::make_unique<Impl>(std::move(configuration))) {
}

OffscreenRenderer::~OffscreenRenderer() = default;
OffscreenRenderer::OffscreenRenderer(OffscreenRenderer&&) noexcept = default;
OffscreenRenderer& OffscreenRenderer::operator=(OffscreenRenderer&&) noexcept = default;

void OffscreenRenderer::render(const CameraPose& camera, std::optional<CameraProjection> projection) {
    impl->session.render(camera, projection);
}

std::vector<uint8_t> OffscreenRenderer::readPixels() const {
    return impl->session.readPixels();
}

} // namespace vkgs
