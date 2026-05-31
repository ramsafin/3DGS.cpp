#pragma once

#include <3dgs/Types.hpp>

#include <cstdint>
#include <filesystem>
#include <span>

namespace vkgs::offscreen {

void writePpm(const std::filesystem::path& path, std::span<const uint8_t> rgba, Extent2D extent);

} // namespace vkgs::offscreen
