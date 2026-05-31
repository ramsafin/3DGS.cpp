#pragma once

#include <filesystem>
#include <vector>

namespace vkgs::core {

std::vector<char> readBinaryFile(const std::filesystem::path& path);

} // namespace vkgs::core
