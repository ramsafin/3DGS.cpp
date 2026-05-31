#include "CameraController.hpp"

#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>

namespace {
constexpr float kMinOrbitDistance = 1e-4f;

glm::vec3 worldUp() {
    return glm::vec3(0.0f, 1.0f, 0.0f);
}

bool isFinite(glm::vec3 value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool isFinite(glm::quat value) {
    return std::isfinite(value.w) && std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}
} // namespace

void CameraController::syncRotationToFocus() {
    const glm::vec3 toFocus = focus - position;
    if (glm::length(toFocus) < kMinOrbitDistance) {
        return;
    }

    glm::vec3 up = worldUp();
    if (std::abs(glm::dot(glm::normalize(toFocus), up)) > 0.99f) {
        up = glm::vec3(0.0f, 0.0f, 1.0f);
    }

    const glm::mat4 view = glm::lookAt(position, focus, up);
    rotation = glm::quat_cast(glm::mat3(glm::inverse(view)));
    orbitDistance = glm::length(toFocus);
}

void CameraController::syncFocusFromPose() {
    focus = position + rotation * glm::vec3(0.0f, 0.0f, -orbitDistance);
}

void CameraController::orbit(float dx, float dy, float sensitivity) {
    glm::vec3 offset = position - focus;
    const float radius = std::max(glm::length(offset), kMinOrbitDistance);
    if (glm::length(offset) < kMinOrbitDistance) {
        offset = glm::vec3(0.0f, 0.0f, radius);
    }

    const float yaw = dx * sensitivity;
    const float pitch = dy * sensitivity;

    offset = glm::vec3(glm::rotate(glm::mat4(1.0f), yaw, worldUp()) * glm::vec4(offset, 1.0f));

    glm::vec3 right = glm::cross(worldUp(), offset);
    if (glm::length(right) < kMinOrbitDistance) {
        right = glm::vec3(1.0f, 0.0f, 0.0f);
    } else {
        right = glm::normalize(right);
    }
    offset = glm::vec3(glm::rotate(glm::mat4(1.0f), pitch, right) * glm::vec4(offset, 1.0f));
    offset = glm::normalize(offset) * radius;

    position = focus + offset;
    syncRotationToFocus();
}

void CameraController::pan(float dx, float dy, float sensitivity) {
    const float scale = orbitDistance * sensitivity;
    const glm::vec3 right = rotation * glm::vec3(1.0f, 0.0f, 0.0f);
    const glm::vec3 up = rotation * glm::vec3(0.0f, 1.0f, 0.0f);
    const glm::vec3 delta = right * (-dx * scale) + up * (dy * scale);
    position += delta;
    focus += delta;
}

void CameraController::dolly(float scrollDelta, float sensitivity) {
    if (scrollDelta == 0.0f) {
        return;
    }

    const glm::vec3 toFocus = focus - position;
    if (glm::length(toFocus) < kMinOrbitDistance) {
        return;
    }

    const float scale = orbitDistance * sensitivity;
    const glm::vec3 forward = glm::normalize(toFocus);
    position += forward * (scrollDelta * scale);
    orbitDistance = std::max(glm::length(focus - position), kMinOrbitDistance);
}

void CameraController::fly(glm::vec3 localDirection, float speed) {
    if (glm::length(localDirection) < kMinOrbitDistance) {
        return;
    }

    localDirection = glm::normalize(localDirection);
    const glm::vec3 worldDelta = rotation * localDirection * speed;
    position += worldDelta;
    focus += worldDelta;
}

void CameraController::frameScene(glm::vec3 center, float radius, float fovDeg) {
    if (!isFinite(center) || !std::isfinite(radius) || radius < 0.0f) {
        throw std::runtime_error("Scene bounds must be finite and have a non-negative radius");
    }
    if (!std::isfinite(fovDeg) || fovDeg <= 0.0f || fovDeg >= 180.0f) {
        throw std::runtime_error("Camera FOV must be finite and in the range (0, 180) degrees");
    }

    focus = center;
    orbitDistance = std::max(radius / std::sin(glm::radians(fovDeg) * 0.5f), 0.1f);
    position = focus + glm::vec3(0.0f, 0.0f, orbitDistance);
    syncRotationToFocus();
    saveResetSnapshot();
}

void CameraController::setPose(glm::vec3 newPosition, glm::quat newRotation) {
    const float squaredLength = glm::dot(newRotation, newRotation);
    if (!isFinite(newPosition) || !isFinite(newRotation) || !std::isfinite(squaredLength) ||
        squaredLength < kMinOrbitDistance * kMinOrbitDistance) {
        throw std::runtime_error("Camera pose must contain a finite position and a non-zero finite quaternion");
    }

    position = newPosition;
    rotation = glm::normalize(newRotation);
    syncFocusFromPose();
}

void CameraController::saveResetSnapshot() {
    resetPosition = position;
    resetRotation = rotation;
    resetFocus = focus;
    resetOrbitDistance = orbitDistance;
    hasResetSnapshot = true;
}

void CameraController::reset() {
    if (!hasResetSnapshot) {
        return;
    }

    position = resetPosition;
    rotation = resetRotation;
    focus = resetFocus;
    orbitDistance = resetOrbitDistance;
}
