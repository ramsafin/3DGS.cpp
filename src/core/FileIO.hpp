#ifndef VKGS_CORE_FILE_IO_H
#define VKGS_CORE_FILE_IO_H

#include <filesystem>
#include <vector>

namespace vkgs::core {

std::vector<char> readBinaryFile(const std::filesystem::path& path);

} // namespace vkgs::core

#endif // VKGS_CORE_FILE_IO_H
