#ifndef GSSCENE_H
#define GSSCENE_H

#include "vulkan/Buffer.h"
#include "vulkan/VulkanContext.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <glm/glm.hpp>
#include <iostream>
#include <string>
#include <vector>

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

class GSScene {
  public:
    explicit GSScene(const std::string& filename) : filename(filename) {
        // check if file exists
        if (!std::filesystem::exists(filename)) {
            throw std::runtime_error("File does not exist: " + filename);
        }
    }

    void load(const std::shared_ptr<VulkanContext>& context);

    void loadTestScene(const std::shared_ptr<VulkanContext>& context);

    uint64_t getNumVertices() const {
        return header.numVertices;
    }

    // Parses only the ASCII PLY header (no Vulkan context / GPU upload required).
    // Used by CPU-only parser tests and by callers that need schema metadata.
    void parseHeaderOnly();

    const PlyHeader& getHeader() const {
        return header;
    }

    struct Vertex {
        glm::vec4 position;
        glm::vec4 scale_opacity;
        glm::vec4 rotation;
        float shs[48];
    };

    struct Cov3DUpperRight {
        float mat[6];
    };

    // CPU/GLSL ABI contract (VKGS-011). Mirrors `struct Vertex` in
    // src/shaders/common.glsl:35-40. Three vec4 (16 each) plus 48 floats.
    static_assert(sizeof(Vertex) == 240, "GSScene::Vertex must be 240 bytes to match GLSL std430 layout");
    static_assert(offsetof(Vertex, position) == 0, "Vertex::position offset mismatch");
    static_assert(offsetof(Vertex, scale_opacity) == 16, "Vertex::scale_opacity offset mismatch");
    static_assert(offsetof(Vertex, rotation) == 32, "Vertex::rotation offset mismatch");
    static_assert(offsetof(Vertex, shs) == 48, "Vertex::shs offset mismatch");

    // Six packed floats per splat, mirrors src/shaders/precomp_cov3d.comp:42-47.
    static_assert(sizeof(Cov3DUpperRight) == 24, "GSScene::Cov3DUpperRight must be 24 bytes (6 floats)");

    std::shared_ptr<Buffer> vertexBuffer;
    std::shared_ptr<Buffer> cov3DBuffer;

  private:
    std::string filename;
    PlyHeader header;

    void loadPlyHeader(std::ifstream& ifstream);

    // Validates that the parsed header matches the native binary vertex layout
    // this loader deserializes (VKGS-007). Throws on mismatch.
    void validatePlyLayout() const;

    static std::shared_ptr<Buffer> createBuffer(const std::shared_ptr<VulkanContext>& sharedPtr, size_t i);

    void precomputeCov3D(const std::shared_ptr<VulkanContext>& context);
};

#endif // GSSCENE_H
