#include <3dgs/Viewer.h>

#include "session/ViewerSession.h"
#include "vulkan/Window.h"
#include "vulkan/windowing/GLFWWindow.h"

#include <utility>

namespace vkgs::viewer {
class WindowAdapter::Impl {
  public:
    explicit Impl(std::shared_ptr<Window> window) : window(std::move(window)) {}

    std::shared_ptr<Window> window;
};

WindowAdapter::WindowAdapter(std::unique_ptr<Impl> impl) : impl(std::move(impl)) {}
WindowAdapter::~WindowAdapter() = default;
WindowAdapter::WindowAdapter(WindowAdapter&&) noexcept = default;
WindowAdapter& WindowAdapter::operator=(WindowAdapter&&) noexcept = default;

std::unique_ptr<WindowAdapter> makeGlfwWindow(std::string name, int width, int height) {
    return std::unique_ptr<WindowAdapter>(
        new WindowAdapter(std::make_unique<WindowAdapter::Impl>(std::make_shared<GLFWWindow>(std::move(name), width,
                                                                                            height))));
}

class Viewer::Impl {
  public:
    Impl(ViewerConfig configuration, std::unique_ptr<WindowAdapter> window)
        : window(std::move(window)),
          session(std::move(configuration), this->window->impl->window) {}

    std::unique_ptr<WindowAdapter> window;
    session::ViewerSession session;
};

Viewer::Viewer(ViewerConfig configuration, std::unique_ptr<WindowAdapter> window)
    : impl(std::make_unique<Impl>(std::move(configuration), std::move(window))) {}

Viewer::~Viewer() = default;
Viewer::Viewer(Viewer&&) noexcept = default;
Viewer& Viewer::operator=(Viewer&&) noexcept = default;

void Viewer::draw() {
    impl->session.draw();
}

void Viewer::run() {
    impl->session.run();
}

void Viewer::stop() {
    impl->session.stop();
}

} // namespace vkgs::viewer
