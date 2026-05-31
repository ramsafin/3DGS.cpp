#pragma once

#include "render/GpuTypes.hpp"

#include <cstdint>
#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace vkgs::scene {

struct PlyProperty final {
    std::string type;
    std::string name;
};

struct PlyHeader final {
    std::string format;
    uint32_t numVertices = 0;
    uint32_t numFaces = 0;
    std::vector<PlyProperty> vertexProperties;
    std::vector<PlyProperty> faceProperties;
};

struct SceneBounds final {
    glm::vec3 center{0.0f};
    float radius = 1.0f;
};

struct GaussianSceneData final {
    PlyHeader header;
    std::vector<vkgs::render::SceneVertex> vertices;
    SceneBounds bounds;
};

} // namespace vkgs::scene
