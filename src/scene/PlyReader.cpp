#include "PlyReader.hpp"

#include "GpuConstants.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace vkgs::scene {
namespace {

struct PlyVertexRecord final {
    std::array<float, 3> position;
    std::array<float, 3> normal;
    std::array<float, 48> shs;
    float opacity;
    std::array<float, 3> scale;
    std::array<float, 4> rotation;
};

static_assert(sizeof(float) == 4, "PLY loader requires 32-bit floats");
static_assert(sizeof(PlyVertexRecord) == 62 * sizeof(float), "PLY disk vertex must contain exactly 62 floats");
static_assert(alignof(PlyVertexRecord) == alignof(float), "PLY disk vertex alignment mismatch");

SceneBounds calculateBounds(const std::vector<vkgs::render::SceneVertex>& vertices) {
    glm::vec3 minPosition(std::numeric_limits<float>::max());
    glm::vec3 maxPosition(std::numeric_limits<float>::lowest());
    for (const auto& vertex : vertices) {
        const glm::vec3 position(vertex.position);
        minPosition = glm::min(minPosition, position);
        maxPosition = glm::max(maxPosition, position);
    }

    SceneBounds bounds;
    bounds.center = (minPosition + maxPosition) * 0.5f;
    bounds.radius = glm::length(maxPosition - minPosition) * 0.5f;
    if (bounds.radius < 1e-4f) {
        bounds.radius = 1.0f;
    }
    return bounds;
}

vkgs::render::SceneVertex convertVertex(const PlyVertexRecord& diskVertex) {
    vkgs::render::SceneVertex vertex = {};
    const glm::vec3 position(diskVertex.position[0], diskVertex.position[1], diskVertex.position[2]);
    vertex.position = glm::vec4(position, 1.0f);
    vertex.scaleOpacity = glm::vec4(
        glm::exp(glm::vec3(diskVertex.scale[0], diskVertex.scale[1], diskVertex.scale[2])),
        1.0f / (1.0f + std::exp(-diskVertex.opacity))
    );

    const glm::vec4
        rotation(diskVertex.rotation[0], diskVertex.rotation[1], diskVertex.rotation[2], diskVertex.rotation[3]);
    if (glm::length(rotation) == 0.0f) {
        throw std::runtime_error("PLY vertex contains a zero quaternion");
    }
    vertex.rotation = glm::normalize(rotation);

    vertex.shs[0] = diskVertex.shs[0];
    vertex.shs[1] = diskVertex.shs[1];
    vertex.shs[2] = diskVertex.shs[2];
    for (uint32_t coefficient = 1; coefficient < gpu::ShCoeffVectors; ++coefficient) {
        vertex.shs[coefficient * 3 + 0] = diskVertex.shs[(coefficient - 1) + 3];
        vertex.shs[coefficient * 3 + 1] = diskVertex.shs[(coefficient - 1) + gpu::ShCoeffVectors + 2];
        vertex.shs[coefficient * 3 + 2] = diskVertex.shs[(coefficient - 1) + gpu::ShCoeffVectors * 2 + 1];
    }
    return vertex;
}

} // namespace

PlyReader::PlyReader(std::filesystem::path filename)
    : filename(std::move(filename)) {
    if (!std::filesystem::exists(this->filename)) {
        throw std::runtime_error("File does not exist: " + this->filename.string());
    }
}

GaussianSceneData PlyReader::read() const {
    std::ifstream plyFile(filename, std::ios::binary);
    auto header = loadHeader(plyFile);
    validateLayout(header);
    if (header.numVertices == 0) {
        throw std::runtime_error("Scene contains no vertices: " + filename.string());
    }

    std::vector<vkgs::render::SceneVertex> vertices;
    vertices.reserve(header.numVertices);
    for (uint32_t index = 0; index < header.numVertices; ++index) {
        PlyVertexRecord diskVertex{};
        plyFile.read(reinterpret_cast<char*>(&diskVertex), sizeof(diskVertex));
        if (!plyFile) {
            throw std::runtime_error("Unexpected end of PLY vertex data in " + filename.string());
        }
        vertices.push_back(convertVertex(diskVertex));
    }

    auto bounds = calculateBounds(vertices);
    return GaussianSceneData{std::move(header), std::move(vertices), bounds};
}

PlyHeader PlyReader::parseHeaderOnly() const {
    std::ifstream plyFile(filename, std::ios::binary);
    return loadHeader(plyFile);
}

PlyHeader PlyReader::validateHeaderOnly() const {
    auto header = parseHeaderOnly();
    validateLayout(header);
    return header;
}

namespace {

enum class CurrentElement : uint8_t {
    None,
    Vertex,
    Face
};

} // namespace

PlyHeader PlyReader::loadHeader(std::ifstream& plyFile) const {
    if (!plyFile.is_open()) {
        throw std::runtime_error("Could not open file: " + filename.string());
    }

    PlyHeader header;
    std::string line;
    bool sawMagic = false;
    bool headerEnd = false;
    auto current = CurrentElement::None;

    while (std::getline(plyFile, line)) {
        std::istringstream input(line);
        std::string token;
        input >> token;

        if (token == "ply") {
            sawMagic = true;
        } else if (token == "format") {
            std::string version;
            input >> header.format >> version;
            if (!input || version != "1.0") {
                throw std::runtime_error("Unsupported PLY format declaration: " + line);
            }
        } else if (token == "comment") {
            continue;
        } else if (token == "element") {
            std::string elementName;
            long long count = -1;
            input >> elementName >> count;
            if (!input || count < 0 || static_cast<unsigned long long>(count) > std::numeric_limits<uint32_t>::max()) {
                throw std::runtime_error("Malformed element declaration in PLY header: " + line);
            }
            if (elementName == "vertex") {
                current = CurrentElement::Vertex;
                header.numVertices = static_cast<uint32_t>(count);
            } else if (elementName == "face") {
                current = CurrentElement::Face;
                header.numFaces = static_cast<uint32_t>(count);
            } else {
                current = CurrentElement::None;
            }
        } else if (token == "property") {
            PlyProperty property;
            input >> property.type;
            if (property.type == "list") {
                std::string countType;
                std::string elementType;
                input >> countType >> elementType >> property.name;
                property.type += ' ';
                property.type += countType;
                property.type += ' ';
                property.type += elementType;
            } else {
                input >> property.name;
            }
            if (!input) {
                throw std::runtime_error("Malformed property declaration in PLY header: " + line);
            }
            if (current == CurrentElement::Vertex) {
                header.vertexProperties.push_back(std::move(property));
            } else if (current == CurrentElement::Face) {
                header.faceProperties.push_back(std::move(property));
            }
        } else if (token == "end_header") {
            headerEnd = true;
            break;
        }
    }

    if (!sawMagic) {
        throw std::runtime_error("File is not a PLY (missing magic): " + filename.string());
    }
    if (!headerEnd) {
        throw std::runtime_error("Could not find end of header");
    }
    return header;
}

void PlyReader::validateLayout(const PlyHeader& header) {
    if (header.format != "binary_little_endian") {
        throw std::runtime_error(
            "Unsupported PLY format '" + header.format + "'; only binary_little_endian is supported"
        );
    }

    static constexpr size_t kExpectedVertexProperties = 62;
    if (header.vertexProperties.size() != kExpectedVertexProperties) {
        throw std::runtime_error(
            "Unexpected PLY vertex property count: got " + std::to_string(header.vertexProperties.size()) +
            ", expected " + std::to_string(kExpectedVertexProperties)
        );
    }

    std::vector<std::string> expectedNames = {"x", "y", "z", "nx", "ny", "nz", "f_dc_0", "f_dc_1", "f_dc_2"};
    for (uint32_t index = 0; index < 45; ++index) {
        expectedNames.push_back("f_rest_" + std::to_string(index));
    }
    expectedNames.insert(
        expectedNames.end(),
        {"opacity", "scale_0", "scale_1", "scale_2", "rot_0", "rot_1", "rot_2", "rot_3"}
    );

    for (size_t index = 0; index < header.vertexProperties.size(); ++index) {
        const auto& property = header.vertexProperties[index];
        if (property.type != "float") {
            throw std::runtime_error("PLY vertex property '" + property.name + "' must be of type float");
        }
        if (property.name != expectedNames[index]) {
            throw std::runtime_error(
                "Unexpected PLY vertex property order at index " + std::to_string(index) + ": got '" + property.name +
                "', expected '" + expectedNames[index] + "'"
            );
        }
    }
}

} // namespace vkgs::scene
