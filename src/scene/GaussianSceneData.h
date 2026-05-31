#ifndef VKGS_SCENE_GAUSSIAN_SCENE_DATA_H
#define VKGS_SCENE_GAUSSIAN_SCENE_DATA_H

#include "render/GpuTypes.h"

#include <cstdint>
#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace vkgs::scene {

struct PlyProperty {
    std::string type;
    std::string name;
};

struct PlyHeader {
    std::string format;
    uint32_t numVertices = 0;
    uint32_t numFaces = 0;
    std::vector<PlyProperty> vertexProperties;
    std::vector<PlyProperty> faceProperties;
};

struct SceneBounds {
    glm::vec3 center{0.0f};
    float radius = 1.0f;
};

struct GaussianSceneData {
    PlyHeader header;
    std::vector<vkgs::render::SceneVertex> vertices;
    SceneBounds bounds;
};

} // namespace vkgs::scene

#endif // VKGS_SCENE_GAUSSIAN_SCENE_DATA_H
