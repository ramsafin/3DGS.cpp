#include "test_support.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#ifdef VKGS_RENDER_MODE_OFFSCREEN
#include <3dgs/OffscreenRenderer.hpp>

namespace fs = std::filesystem;

namespace {

std::string makeFixtureScene() {
    const fs::path path = fs::path(VKGS_GOLDEN_DIR) / "golden_scene.ply";
    std::vector<vkgs_test::PlyVertexRecord> vertices(16, vkgs_test::PlyVertexRecord{});

    // Deterministic splats placed in front of a camera at the origin looking
    // down -Z. Values are arbitrary but fixed so renders are reproducible.
    for (int i = 0; i < 16; ++i) {
        const float t = static_cast<float>(i);
        vertices[i].position[0] = (t - 8.0f) * 0.2f;
        vertices[i].position[1] = ((i % 4) - 2) * 0.2f;
        vertices[i].position[2] = -2.0f - (t * 0.05f);
        vertices[i].shs[0] = 0.5f;
        vertices[i].shs[1] = 0.3f;
        vertices[i].shs[2] = 0.2f;
        vertices[i].opacity = 1.0f;
        vertices[i].scale[0] = -3.0f;
        vertices[i].scale[1] = -3.0f;
        vertices[i].scale[2] = -3.0f;
        vertices[i].rotation[0] = 1.0f;
    }
    vkgs_test::writeBinaryPly(path.string(), vertices);
    return path.string();
}

uint64_t renderHash() {
    vkgs::OffscreenConfig config{};
    config.scene = makeFixtureScene();
    config.extent = {256, 256};
    config.enableVulkanValidationLayers = false;

    vkgs::OffscreenRenderer renderer(config);
    renderer.render({{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f}});
    auto pixels = renderer.readPixels();
    return vkgs_test::fnv1a(pixels);
}

bool deviceAvailable() {
    auto context = vkgs_test::makeHeadlessContext();
    return context != nullptr;
}

// Renders the same scene twice in one process; output must be bit-identical.
TEST(GoldenImage, RenderIsDeterministic) {
    if (!deviceAvailable()) {
        GTEST_SKIP() << "No Vulkan device available";
    }
    const uint64_t first = renderHash();
    const uint64_t second = renderHash();
    EXPECT_EQ(first, second);
}

// Compares against a per-machine golden hash recorded on first run, guarding
// against unintended output changes across revision stages.
TEST(GoldenImage, MatchesRecordedHash) {
    if (!deviceAvailable()) {
        GTEST_SKIP() << "No Vulkan device available";
    }
    const uint64_t hash = renderHash();

    const fs::path goldenPath = fs::path(VKGS_GOLDEN_DIR) / "simple_render.hash";
    if (!fs::exists(goldenPath)) {
        std::ofstream out(goldenPath);
        out << hash;
        GTEST_SKIP() << "Recorded new golden hash " << hash << " at " << goldenPath.string();
    }

    std::ifstream in(goldenPath);
    uint64_t golden = 0;
    in >> golden;
    EXPECT_EQ(hash, golden) << "Render output changed. Delete " << goldenPath.string()
                            << " to re-record if this change is intentional.";
}

} // namespace

#else

TEST(GoldenImage, RequiresOffscreenBuild) {
    GTEST_SKIP() << "Golden image test only runs in OFFSCREEN render mode";
}

#endif
