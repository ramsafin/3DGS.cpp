#include "OutputFilename.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

using vkgs::offscreen::formatOutputFilename;

TEST(OutputFilename, FormatsSupportedPlaceholders) {
    EXPECT_EQ(formatOutputFilename("frame_%04d.ppm", 7, "front"), "frame_0007.ppm");
    EXPECT_EQ(formatOutputFilename("{name}_{index}.ppm", 12, "front"), "front_12.ppm");
}

TEST(OutputFilename, RejectsUnsafePrintfSpecifier) {
    EXPECT_THROW(formatOutputFilename("frame_%s.ppm", 7, "front"), std::runtime_error);
    EXPECT_THROW(formatOutputFilename("frame_%n.ppm", 7, "front"), std::runtime_error);
}

TEST(OutputFilename, RejectsTraversal) {
    EXPECT_THROW(formatOutputFilename("../frame_%d.ppm", 7, "front"), std::runtime_error);
    EXPECT_THROW(formatOutputFilename("{name}.ppm", 7, "../front"), std::runtime_error);
}

TEST(OutputFilename, RejectsUnknownPlaceholder) {
    EXPECT_THROW(formatOutputFilename("frame_{unknown}.ppm", 7, "front"), std::runtime_error);
}
