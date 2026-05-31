#include "scene/PlyReader.hpp"
#include "test_support.hpp"

#include <gtest/gtest.h>

#include <fstream>
#include <string>

namespace {

std::string fixture(const std::string& name) {
    return std::string(VKGS_FIXTURE_DIR) + "/" + name;
}

std::string golden(const std::string& name) {
    return std::string(VKGS_GOLDEN_DIR) + "/" + name;
}

void writeFixture(const std::string& path, const std::string& contents) {
    std::ofstream out(path, std::ios::binary);
    out << contents;
}

void replaceOnce(std::string& text, const std::string& from, const std::string& to) {
    const auto index = text.find(from);
    ASSERT_NE(index, std::string::npos);
    text.replace(index, from.size(), to);
}

TEST(PlyParser, ParsesValidHeaderCounts) {
    const auto header = vkgs::scene::PlyReader(fixture("valid_tiny.ply")).parseHeaderOnly();
    EXPECT_EQ(header.format, "binary_little_endian");
    EXPECT_EQ(header.numVertices, 3u);
}

TEST(PlyParser, RoutesPropertiesToVertexElement) {
    const auto header = vkgs::scene::PlyReader(fixture("valid_tiny.ply")).parseHeaderOnly();

    // valid_tiny.ply declares 10 vertex properties and no face element.
    EXPECT_EQ(header.vertexProperties.size(), 10u);
    EXPECT_TRUE(header.faceProperties.empty());
    EXPECT_EQ(header.vertexProperties.front().name, "x");
}

TEST(PlyParser, RoutesPropertiesToFaceElement) {
    const auto path = golden("face_properties.ply");
    writeFixture(path, "ply\nformat binary_little_endian 1.0\nelement vertex 1\nproperty float x\n"
                       "element face 1\nproperty list uchar int vertex_indices\nend_header\n");
    const auto header = vkgs::scene::PlyReader(path).parseHeaderOnly();

    EXPECT_EQ(header.vertexProperties.size(), 1u);
    ASSERT_EQ(header.faceProperties.size(), 1u);
    EXPECT_EQ(header.faceProperties.front().type, "list uchar int");
    EXPECT_EQ(header.faceProperties.front().name, "vertex_indices");
}

TEST(PlyParser, ValidatesCompleteDiskRecordSchema) {
    const auto path = golden("valid_complete.ply");
    vkgs_test::writeBinaryPly(path, std::vector<vkgs_test::PlyVertexRecord>(1));
    EXPECT_NO_THROW((void)vkgs::scene::PlyReader(path).validateHeaderOnly());
}

TEST(PlyParser, RejectsOversizedVertexCount) {
    const auto path = golden("oversized_count.ply");
    writeFixture(path, "ply\nformat binary_little_endian 1.0\nelement vertex 4294967296\nend_header\n");

    EXPECT_THROW((void)vkgs::scene::PlyReader(path).parseHeaderOnly(), std::runtime_error);
}

TEST(PlyParser, RejectsWrongFormat) {
    const auto path = golden("wrong_format.ply");
    writeFixture(path, vkgs_test::binaryPlyHeader(1, "ascii"));

    EXPECT_THROW((void)vkgs::scene::PlyReader(path).validateHeaderOnly(), std::runtime_error);
}

TEST(PlyParser, RejectsWrongPropertyType) {
    const auto path = golden("wrong_type.ply");
    auto header = vkgs_test::binaryPlyHeader(1);
    replaceOnce(header, "property float x", "property double x");
    writeFixture(path, header);

    EXPECT_THROW((void)vkgs::scene::PlyReader(path).validateHeaderOnly(), std::runtime_error);
}

TEST(PlyParser, RejectsReorderedProperty) {
    const auto path = golden("wrong_order.ply");
    auto header = vkgs_test::binaryPlyHeader(1);
    replaceOnce(header, "property float x", "property float temporary");
    replaceOnce(header, "property float y", "property float x");
    replaceOnce(header, "property float temporary", "property float y");
    writeFixture(path, header);

    EXPECT_THROW((void)vkgs::scene::PlyReader(path).validateHeaderOnly(), std::runtime_error);
}

TEST(PlyParser, ParsesZeroVertexHeader) {
    const auto header = vkgs::scene::PlyReader(fixture("zero_vertex.ply")).parseHeaderOnly();
    EXPECT_EQ(header.numVertices, 0u);
}

TEST(PlyParser, MissingEndHeaderThrows) {
    EXPECT_THROW((void)vkgs::scene::PlyReader(fixture("no_end_header.ply")).parseHeaderOnly(), std::runtime_error);
}

TEST(PlyParser, MissingFileThrowsAtConstruction) {
    EXPECT_THROW(vkgs::scene::PlyReader(fixture("does_not_exist.ply")), std::runtime_error);
}

TEST(PlyParser, ConvertsDiskRecordAndComputesBounds) {
    const auto path = golden("converted_record.ply");
    std::vector<vkgs_test::PlyVertexRecord> records(2);
    records[0].position[0] = -2.0f;
    records[0].rotation[3] = 2.0f;
    records[0].shs[3] = 11.0f;
    records[0].shs[18] = 22.0f;
    records[0].shs[33] = 33.0f;
    records[1].position[0] = 2.0f;
    records[1].rotation[3] = 1.0f;
    vkgs_test::writeBinaryPly(path, records);

    const auto data = vkgs::scene::PlyReader(path).read();
    ASSERT_EQ(data.vertices.size(), 2u);
    EXPECT_FLOAT_EQ(data.bounds.center.x, 0.0f);
    EXPECT_FLOAT_EQ(data.bounds.radius, 2.0f);
    EXPECT_FLOAT_EQ(data.vertices[0].rotation.w, 1.0f);
    EXPECT_FLOAT_EQ(data.vertices[0].shs[3], 11.0f);
    EXPECT_FLOAT_EQ(data.vertices[0].shs[4], 22.0f);
    EXPECT_FLOAT_EQ(data.vertices[0].shs[5], 33.0f);
}

TEST(PlyParser, RejectsZeroQuaternion) {
    const auto path = golden("zero_quaternion.ply");
    vkgs_test::writeBinaryPly(path, std::vector<vkgs_test::PlyVertexRecord>(1));

    EXPECT_THROW((void)vkgs::scene::PlyReader(path).read(), std::runtime_error);
}

} // namespace
