#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <string>
#include <unordered_map>

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

    bool frameSceneRequested = false;
    bool resetCameraRequested = false;

    glm::vec3 cameraPosition{0.0f};
    glm::quat cameraRotation{1.0f, 0.0f, 0.0f, 0.0f};
};
