#include "Renderer.h"

#include <3dgs/3dgs.h>
#include <stdexcept>

#ifdef VKGS_ENABLE_GLFW
#include "vulkan/windowing/GLFWWindow.h"
std::shared_ptr<Window> VulkanSplatting::createGlfwWindow(std::string name, int width, int height) {
    return std::make_shared<GLFWWindow>(name, width, height);
}
#endif

void VulkanSplatting::start() {
    // Create the renderer
    renderer = std::make_shared<Renderer>(configuration);
    renderer->initialize();
    renderer->run();
}

void VulkanSplatting::initialize() {
    renderer = std::make_shared<Renderer>(configuration);
    renderer->initialize();
}

void VulkanSplatting::draw() {
    if (!renderer) {
        throw std::runtime_error("Renderer must be initialized before draw()");
    }
    renderer->draw();
}

std::vector<uint8_t> VulkanSplatting::readPixels() {
    if (!renderer) {
        throw std::runtime_error("Renderer must be initialized before readPixels()");
    }
    return renderer->readPixels();
}

void VulkanSplatting::setCameraPose(float px, float py, float pz, float qw, float qx, float qy, float qz) {
    if (!renderer) {
        throw std::runtime_error("Renderer must be initialized before setting camera pose");
    }
    renderer->setCameraPose(px, py, pz, qw, qx, qy, qz);
}

void VulkanSplatting::setCameraProjection(float fovDegrees, float nearPlane, float farPlane) {
    if (!renderer) {
        throw std::runtime_error("Renderer must be initialized before setting camera projection");
    }
    renderer->setCameraProjection(fovDegrees, nearPlane, farPlane);
}

void VulkanSplatting::logTranslation(float x, float y) {
#ifdef VKGS_RENDER_MODE_ONSCREEN
    configuration.window->logTranslation(x, y);
#endif
}

void VulkanSplatting::logMovement(float x, float y, float z) {
    if (!renderer) {
        throw std::runtime_error("Renderer must be initialized before logMovement()");
    }
    renderer->camera.translate(glm::vec3(x, y, z));
}

void VulkanSplatting::stop() {
    if (!renderer) {
        return;
    }
    renderer->stop();
}
