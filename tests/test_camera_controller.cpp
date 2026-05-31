#include "CameraController.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>

TEST(CameraController, RejectsZeroQuaternion) {
    CameraController controller;
    EXPECT_THROW(controller.setPose(glm::vec3(0.0f), glm::quat(0.0f, 0.0f, 0.0f, 0.0f)), std::runtime_error);
}

TEST(CameraController, NormalizesPoseQuaternion) {
    CameraController controller;
    controller.setPose(glm::vec3(1.0f, 2.0f, 3.0f), glm::quat(2.0f, 0.0f, 0.0f, 0.0f));
    EXPECT_FLOAT_EQ(glm::length(controller.rotation), 1.0f);
}

TEST(CameraController, FramesSceneUsingFov) {
    CameraController controller;
    controller.frameScene(glm::vec3(0.0f), 1.0f, 60.0f);
    EXPECT_NEAR(controller.orbitDistance, 2.0f, 1e-5f);
}

TEST(CameraController, RejectsInvalidFrameFov) {
    CameraController controller;
    EXPECT_THROW(controller.frameScene(glm::vec3(0.0f), 1.0f, 180.0f), std::runtime_error);
}
