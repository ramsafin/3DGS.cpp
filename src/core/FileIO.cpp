#include "FileIO.hpp"

#include <fstream>
#include <limits>

namespace vkgs::core {

std::vector<char> readBinaryFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return {};
    }

    file.seekg(0, std::ios::end);
    const auto end = static_cast<std::streamoff>(file.tellg());
    if (end < 0 || static_cast<uintmax_t>(end) > std::numeric_limits<size_t>::max() ||
        static_cast<uintmax_t>(end) > static_cast<uintmax_t>(std::numeric_limits<std::streamsize>::max())) {
        return {};
    }

    const auto fileSize = static_cast<size_t>(end);
    file.seekg(0, std::ios::beg);

    std::vector<char> result(fileSize);
    file.read(result.data(), static_cast<std::streamsize>(fileSize));
    if (!file) {
        return {};
    }
    return result;
}

} // namespace vkgs::core
