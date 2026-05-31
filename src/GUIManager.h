#ifndef GUIMANAGER_H
#define GUIMANAGER_H
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class GUIManager {
  public:
    GUIManager();

    ~GUIManager();

    static void init();

    void buildGui();

    static void pushTextMetric(const std::string& name, float value);

    static void pushMetric(const std::string& name, float value);

    static void pushMetric(const std::unordered_map<std::string, float>& name);

    static bool wantCaptureMouse();

    static bool wantCaptureKeyboard();

    bool frameRotationToggleRequested = false;
    bool frameSceneRequested = false;
    bool resetCameraRequested = false;
    bool viewRotated180 = false;

    glm::vec3 cameraPosition{0.0f};
    glm::quat cameraRotation{1.0f, 0.0f, 0.0f, 0.0f};
};

#endif // GUIMANAGER_H
