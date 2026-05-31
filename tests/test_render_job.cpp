#include "PpmWriter.h"
#include "RenderJob.h"

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace {

namespace fs = std::filesystem;
using json = nlohmann::json;

fs::path golden(const std::string& name) {
    return fs::path(VKGS_GOLDEN_DIR) / name;
}

void writeJson(const fs::path& path, const json& value) {
    std::ofstream stream(path);
    stream << value;
}

json validConfig() {
    return {{"scene", (fs::path(VKGS_FIXTURE_DIR) / "valid_tiny.ply").string()},
            {"frames",
             {{{"name", "front"},
               {"position", {1.0f, 2.0f, 3.0f}},
               {"rotation_quat", {1.0f, 0.0f, 0.0f, 0.0f}},
               {"fov_degrees", 60.0f}}}}};
}

} // namespace

TEST(RenderJob, LoadsDefaultsAndFrameOverride) {
    const auto path = golden("render_job.json");
    writeJson(path, validConfig());

    const auto job = vkgs::offscreen::loadRenderJob(path);
    EXPECT_EQ(job.renderer.extent.width, 1280u);
    EXPECT_EQ(job.renderer.extent.height, 720u);
    EXPECT_EQ(job.filenamePattern, "frame_%04d.ppm");
    ASSERT_EQ(job.frames.size(), 1u);
    EXPECT_EQ(job.frames[0].name, "front");
    EXPECT_FLOAT_EQ(job.frames[0].projection.horizontalFovDegrees, 60.0f);
    EXPECT_FLOAT_EQ(job.frames[0].camera.position[2], 3.0f);
}

TEST(RenderJob, RejectsEmptyFrames) {
    const auto path = golden("empty_render_job.json");
    auto config = validConfig();
    config["frames"] = json::array();
    writeJson(path, config);

    EXPECT_THROW((void)vkgs::offscreen::loadRenderJob(path), std::runtime_error);
}

TEST(PpmWriter, WritesRgbBytesFromRgbaInput) {
    const auto path = golden("single_pixel.ppm");
    const std::vector<uint8_t> rgba = {1, 2, 3, 255};
    vkgs::offscreen::writePpm(path, rgba, {1, 1});

    std::ifstream stream(path, std::ios::binary);
    const std::string bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    EXPECT_EQ(bytes, std::string("P6\n1 1\n255\n", 11) + std::string("\x01\x02\x03", 3));
}

TEST(PpmWriter, RejectsWrongBufferSize) {
    EXPECT_THROW(vkgs::offscreen::writePpm(golden("bad.ppm"), std::vector<uint8_t>{1, 2, 3}, {1, 1}),
                 std::runtime_error);
}
