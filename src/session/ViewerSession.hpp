#ifndef VKGS_SESSION_VIEWER_SESSION_H
#define VKGS_SESSION_VIEWER_SESSION_H

#include <3dgs/Viewer.hpp>

#include "Renderer.hpp"

#include <memory>

class Window;

namespace vkgs::session {

class ViewerSession {
  public:
    ViewerSession(viewer::ViewerConfig configuration, std::shared_ptr<Window> window);
    ~ViewerSession();

    void draw();
    void run();
    void stop();

  private:
    Renderer renderer;
};

} // namespace vkgs::session

#endif // VKGS_SESSION_VIEWER_SESSION_H
