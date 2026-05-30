#include "GSScene.h"

#include <gtest/gtest.h>

#include <string>

namespace {

std::string fixture(const std::string& name) {
    return std::string(VKGS_FIXTURE_DIR) + "/" + name;
}

TEST(PlyParser, ParsesValidHeaderCounts) {
    GSScene scene(fixture("valid_tiny.ply"));
    scene.parseHeaderOnly();

    const auto& header = scene.getHeader();
    EXPECT_EQ(header.format, "binary_little_endian");
    EXPECT_EQ(scene.getNumVertices(), 3u);
}

TEST(PlyParser, RoutesPropertiesToVertexElement) {
    GSScene scene(fixture("valid_tiny.ply"));
    scene.parseHeaderOnly();

    // valid_tiny.ply declares 10 vertex properties and no face element.
    EXPECT_EQ(scene.getHeader().vertexProperties.size(), 10u);
    EXPECT_TRUE(scene.getHeader().faceProperties.empty());
    EXPECT_EQ(scene.getHeader().vertexProperties.front().name, "x");
}

TEST(PlyParser, ParsesZeroVertexHeader) {
    GSScene scene(fixture("zero_vertex.ply"));
    scene.parseHeaderOnly();

    EXPECT_EQ(scene.getNumVertices(), 0u);
}

TEST(PlyParser, MissingEndHeaderThrows) {
    GSScene scene(fixture("no_end_header.ply"));
    EXPECT_THROW(scene.parseHeaderOnly(), std::runtime_error);
}

TEST(PlyParser, MissingFileThrowsAtConstruction) {
    EXPECT_THROW(GSScene scene(fixture("does_not_exist.ply")), std::runtime_error);
}

} // namespace
