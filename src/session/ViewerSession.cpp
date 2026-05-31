#include "ViewerSession.hpp"

#include "render/RendererConfiguration.hpp"
#include "vulkan/Window.hpp"

#include <utility>

namespace vkgs::session {
namespace {

render::RendererConfiguration toRendererConfiguration(viewer::ViewerConfig configuration,
                                                      std::shared_ptr<Window> window) {
    render::RendererConfiguration result;
    result.scene = std::move(configuration.scene);
    result.width = configuration.extent.width;
    result.height = configuration.extent.height;
    result.fov = configuration.projection.horizontalFovDegrees;
    result.nearPlane = configuration.projection.nearPlane;
    result.farPlane = configuration.projection.farPlane;
    result.enableVulkanValidationLayers = configuration.enableVulkanValidationLayers;
    result.physicalDeviceId = configuration.physicalDeviceId;
    result.immediateSwapchain = configuration.immediateSwapchain;
    result.enableGui = configuration.enableGui;
    result.window = std::move(window);
    return result;
}

} // namespace

ViewerSession::ViewerSession(viewer::ViewerConfig configuration, std::shared_ptr<Window> window)
    : renderer(toRendererConfiguration(std::move(configuration), std::move(window))) {
    renderer.initialize();
}

ViewerSession::~ViewerSession() {
    renderer.stop();
}

void ViewerSession::draw() {
    renderer.draw();
}

void ViewerSession::run() {
    renderer.run();
}

void ViewerSession::stop() {
    renderer.stop();
}

} // namespace vkgs::session
