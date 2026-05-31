#include "vulkan/Shader.hpp"

#include <gtest/gtest.h>

#include <array>
#include <memory>
#include <stdexcept>

TEST(Shader, RejectsEmptyEmbeddedModule) {
    alignas(4) const std::array<unsigned char, 4> bytes{};
    Shader shader(std::shared_ptr<VulkanContext>{}, "empty", bytes.data(), 0);

    EXPECT_THROW(shader.load(), std::runtime_error);
}

TEST(Shader, RejectsNonWordSizedEmbeddedModule) {
    alignas(4) const std::array<unsigned char, 3> bytes{};
    Shader shader(std::shared_ptr<VulkanContext>{}, "partial_word", bytes.data(), sizeof(bytes));

    EXPECT_THROW(shader.load(), std::runtime_error);
}
