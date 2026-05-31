#include "vulkan/Shader.h"

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>

TEST(Shader, RejectsEmptyEmbeddedModule) {
    alignas(4) const unsigned char bytes[4] = {};
    Shader shader(std::shared_ptr<VulkanContext>{}, "empty", bytes, 0);

    EXPECT_THROW(shader.load(), std::runtime_error);
}

TEST(Shader, RejectsNonWordSizedEmbeddedModule) {
    alignas(4) const unsigned char bytes[3] = {};
    Shader shader(std::shared_ptr<VulkanContext>{}, "partial_word", bytes, sizeof(bytes));

    EXPECT_THROW(shader.load(), std::runtime_error);
}
