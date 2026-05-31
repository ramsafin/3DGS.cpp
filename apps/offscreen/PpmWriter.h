#ifndef VKGS_OFFSCREEN_PPM_WRITER_H
#define VKGS_OFFSCREEN_PPM_WRITER_H

#include <3dgs/Types.h>

#include <cstdint>
#include <filesystem>
#include <span>

namespace vkgs::offscreen {

void writePpm(const std::filesystem::path& path, std::span<const uint8_t> rgba, Extent2D extent);

} // namespace vkgs::offscreen

#endif // VKGS_OFFSCREEN_PPM_WRITER_H
