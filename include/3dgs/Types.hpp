#ifndef VKGS_TYPES_H
#define VKGS_TYPES_H

#include <array>
#include <cstdint>

namespace vkgs {

struct Extent2D {
    uint32_t width = 1280;
    uint32_t height = 720;
};

struct CameraPose {
    std::array<float, 3> position{0.0f, 0.0f, 0.0f};
    std::array<float, 4> rotation{1.0f, 0.0f, 0.0f, 0.0f};
};

struct CameraProjection {
    float horizontalFovDegrees = 45.0f;
    float nearPlane = 0.2f;
    float farPlane = 1000.0f;
};

} // namespace vkgs

#endif // VKGS_TYPES_H
