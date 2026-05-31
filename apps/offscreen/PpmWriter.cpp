#include "PpmWriter.hpp"

#include <fstream>
#include <stdexcept>

namespace vkgs::offscreen {

void writePpm(const std::filesystem::path& path, std::span<const uint8_t> rgba, Extent2D extent) {
    const size_t expectedSize = static_cast<size_t>(extent.width) * extent.height * 4;
    if (rgba.size() != expectedSize) {
        throw std::runtime_error("Unexpected RGBA buffer size for PPM output");
    }
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }

    std::ofstream stream(path, std::ios::binary);
    if (!stream.is_open()) {
        throw std::runtime_error("Failed to open output file: " + path.string());
    }
    stream << "P6\n" << extent.width << " " << extent.height << "\n255\n";
    for (size_t index = 0; index < expectedSize; index += 4) {
        stream.put(static_cast<char>(rgba[index]));
        stream.put(static_cast<char>(rgba[index + 1]));
        stream.put(static_cast<char>(rgba[index + 2]));
    }
}

} // namespace vkgs::offscreen
