#include "GLFWWindow.h"
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/ext/matrix_transform.hpp>
#include <stdexcept>

GLFWWindow::GLFWWindow(std::string name, int width, int height) {
    if (glfwInit() != GLFW_TRUE) {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    window = glfwCreateWindow(width, height, name.c_str(), nullptr, nullptr);
    if (window == nullptr) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwSetWindowUserPointer(static_cast<GLFWwindow*>(window), this);
    glfwSetScrollCallback(static_cast<GLFWwindow*>(window), scrollCallback);
}

GLFWWindow::~GLFWWindow() {
    if (window != nullptr) {
        glfwDestroyWindow(static_cast<GLFWwindow*>(window));
        window = nullptr;
    }
    glfwTerminate();
}

VkSurfaceKHR GLFWWindow::createSurface(std::shared_ptr<VulkanContext> context) {
    if (glfwCreateWindowSurface(context->instance.get(), static_cast<GLFWwindow*>(window), nullptr, &surface) !=
        VK_SUCCESS) {
        throw std::runtime_error("failed to create window surface!");
    }
    return surface;
}

std::array<bool, 3> GLFWWindow::getMouseButton() {
    return {glfwGetMouseButton(static_cast<GLFWwindow*>(window), GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS,
            glfwGetMouseButton(static_cast<GLFWwindow*>(window), GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS,
            glfwGetMouseButton(static_cast<GLFWwindow*>(window), GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS};
}

std::vector<std::string> GLFWWindow::getRequiredInstanceExtensions() {
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    auto extensions = std::vector<std::string>{};
    for (uint32_t i = 0; i < glfwExtensionCount; i++) {
        extensions.emplace_back(glfwExtensions[i]);
    }
    return extensions;
}

std::pair<uint32_t, uint32_t> GLFWWindow::getFramebufferSize() const {
    int width, height;
    glfwGetFramebufferSize(static_cast<GLFWwindow*>(window), &width, &height);
    return {width, height};
}

std::array<double, 2> GLFWWindow::getCursorTranslation() {
    double x, y;
    glfwGetCursorPos(static_cast<GLFWwindow*>(window), &x, &y);
    const auto translation = std::array<double, 2>{x - lastX, y - lastY};
    lastX = x;
    lastY = y;
    return translation;
}

std::array<bool, 7> GLFWWindow::getKeys() {
    return {glfwGetKey(static_cast<GLFWwindow*>(window), GLFW_KEY_W) == GLFW_PRESS,
            glfwGetKey(static_cast<GLFWwindow*>(window), GLFW_KEY_A) == GLFW_PRESS,
            glfwGetKey(static_cast<GLFWwindow*>(window), GLFW_KEY_S) == GLFW_PRESS,
            glfwGetKey(static_cast<GLFWwindow*>(window), GLFW_KEY_D) == GLFW_PRESS,
            glfwGetKey(static_cast<GLFWwindow*>(window), GLFW_KEY_SPACE) == GLFW_PRESS,
            glfwGetKey(static_cast<GLFWwindow*>(window), GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS,
            glfwGetKey(static_cast<GLFWwindow*>(window), GLFW_KEY_ESCAPE) == GLFW_PRESS};
}

void GLFWWindow::mouseCapture(bool capture) {
    if (capture) {
        glfwSetInputMode(static_cast<GLFWwindow*>(window), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    } else {
        glfwSetInputMode(static_cast<GLFWwindow*>(window), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}

void GLFWWindow::scrollCallback(GLFWwindow* window, double /*xoffset*/, double yoffset) {
    auto* self = static_cast<GLFWWindow*>(glfwGetWindowUserPointer(window));
    if (self != nullptr) {
        self->scrollDeltaY += yoffset;
    }
}

double GLFWWindow::getScrollDelta() {
    const double delta = scrollDeltaY;
    scrollDeltaY = 0.0;
    return delta;
}

bool GLFWWindow::tick() {
    glfwPollEvents();
    return !glfwWindowShouldClose(static_cast<GLFWwindow*>(window));
}