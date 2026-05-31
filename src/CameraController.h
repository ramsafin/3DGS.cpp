#ifndef CAMERACONTROLLER_H
#define CAMERACONTROLLER_H

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

class CameraController {
  public:
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 focus{0.0f, 0.0f, 0.0f};
    float orbitDistance = 1.0f;

    void orbit(float dx, float dy, float sensitivity);
    void pan(float dx, float dy, float sensitivity);
    void dolly(float scrollDelta, float sensitivity);
    void fly(glm::vec3 localDirection, float speed);
    void frameScene(glm::vec3 center, float radius, float fovDeg);
    void setPose(glm::vec3 newPosition, glm::quat newRotation);
    void reset();

    void syncRotationToFocus();
    void syncFocusFromPose();

  private:
    bool hasResetSnapshot = false;
    glm::vec3 resetPosition{};
    glm::quat resetRotation{};
    glm::vec3 resetFocus{};
    float resetOrbitDistance = 1.0f;

    void saveResetSnapshot();
};

#endif // CAMERACONTROLLER_H
