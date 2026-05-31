#include "OffscreenSession.h"

#include "render/RendererConfiguration.h"

#include <utility>

namespace vkgs::session {
namespace {

render::RendererConfiguration toRendererConfiguration(OffscreenConfig configuration) {
    render::RendererConfiguration result;
    result.scene = std::move(configuration.scene);
    result.width = configuration.extent.width;
    result.height = configuration.extent.height;
    result.fov = configuration.projection.horizontalFovDegrees;
    result.nearPlane = configuration.projection.nearPlane;
    result.farPlane = configuration.projection.farPlane;
    result.enableVulkanValidationLayers = configuration.enableVulkanValidationLayers;
    result.physicalDeviceId = configuration.physicalDeviceId;
    return result;
}

} // namespace

OffscreenSession::OffscreenSession(OffscreenConfig configuration)
    : renderer(toRendererConfiguration(std::move(configuration))) {
    renderer.initialize();
}

OffscreenSession::~OffscreenSession() {
    renderer.stop();
}

void OffscreenSession::render(const CameraPose& camera, std::optional<CameraProjection> projection) {
    if (projection.has_value()) {
        renderer.setCameraProjection(projection->horizontalFovDegrees, projection->nearPlane, projection->farPlane);
    }
    renderer.setCameraPose(camera.position[0], camera.position[1], camera.position[2], camera.rotation[0],
                           camera.rotation[1], camera.rotation[2], camera.rotation[3]);
    renderer.draw();
}

std::vector<uint8_t> OffscreenSession::readPixels() const {
    return renderer.readPixels();
}

} // namespace vkgs::session
