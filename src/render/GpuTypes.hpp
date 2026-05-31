#ifndef VKGS_RENDER_GPU_TYPES_H
#define VKGS_RENDER_GPU_TYPES_H

#include "GpuConstants.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <glm/glm.hpp>

namespace vkgs::render {

struct SceneVertex {
    glm::vec4 position;
    glm::vec4 scaleOpacity;
    glm::vec4 rotation;
    std::array<float, gpu::ShMaxCoeffs> shs;
};

struct Cov3DUpperRight {
    std::array<float, 6> values;
};

struct alignas(16) UniformBuffer {
    glm::vec4 cameraPosition;
    glm::mat4 projection;
    glm::mat4 view;
    uint32_t width;
    uint32_t height;
    float tanFovX;
    float tanFovY;
    float nearPlane;
};

struct VertexAttribute {
    glm::vec4 conicOpacity;
    glm::vec4 colorRadii;
    glm::uvec4 aabb;
    glm::vec2 uv;
    float depth;
    uint32_t magic;
};

struct RadixSortPushConstants {
    uint32_t numElements;
    uint32_t shift;
    uint32_t numWorkgroups;
    uint32_t numBlocksPerWorkgroup;
};

static_assert(sizeof(SceneVertex) == 240, "SceneVertex must match the GLSL std430 layout");
static_assert(offsetof(SceneVertex, position) == 0, "SceneVertex::position offset mismatch");
static_assert(offsetof(SceneVertex, scaleOpacity) == 16, "SceneVertex::scaleOpacity offset mismatch");
static_assert(offsetof(SceneVertex, rotation) == 32, "SceneVertex::rotation offset mismatch");
static_assert(offsetof(SceneVertex, shs) == 48, "SceneVertex::shs offset mismatch");

static_assert(sizeof(Cov3DUpperRight) == 24, "Cov3DUpperRight must contain six packed floats");

static_assert(sizeof(UniformBuffer) == 176, "UniformBuffer must match the GLSL std140 layout");
static_assert(offsetof(UniformBuffer, cameraPosition) == 0, "UniformBuffer::cameraPosition offset mismatch");
static_assert(offsetof(UniformBuffer, projection) == 16, "UniformBuffer::projection offset mismatch");
static_assert(offsetof(UniformBuffer, view) == 80, "UniformBuffer::view offset mismatch");
static_assert(offsetof(UniformBuffer, width) == 144, "UniformBuffer::width offset mismatch");
static_assert(offsetof(UniformBuffer, height) == 148, "UniformBuffer::height offset mismatch");
static_assert(offsetof(UniformBuffer, tanFovX) == 152, "UniformBuffer::tanFovX offset mismatch");
static_assert(offsetof(UniformBuffer, tanFovY) == 156, "UniformBuffer::tanFovY offset mismatch");
static_assert(offsetof(UniformBuffer, nearPlane) == 160, "UniformBuffer::nearPlane offset mismatch");

static_assert(sizeof(VertexAttribute) == 64, "VertexAttribute must match the GLSL std430 layout");
static_assert(offsetof(VertexAttribute, conicOpacity) == 0, "VertexAttribute::conicOpacity offset mismatch");
static_assert(offsetof(VertexAttribute, colorRadii) == 16, "VertexAttribute::colorRadii offset mismatch");
static_assert(offsetof(VertexAttribute, aabb) == 32, "VertexAttribute::aabb offset mismatch");
static_assert(offsetof(VertexAttribute, uv) == 48, "VertexAttribute::uv offset mismatch");
static_assert(offsetof(VertexAttribute, depth) == 56, "VertexAttribute::depth offset mismatch");
static_assert(offsetof(VertexAttribute, magic) == 60, "VertexAttribute::magic offset mismatch");

static_assert(sizeof(RadixSortPushConstants) == 16, "RadixSortPushConstants must match the GLSL push constants");

} // namespace vkgs::render

#endif // VKGS_RENDER_GPU_TYPES_H
