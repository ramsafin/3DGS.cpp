#include "StringList.hpp"

namespace vkgs::vulkan {

std::vector<const char*> toCStringPointers(const std::vector<std::string>& strings) {
    std::vector<const char*> result;
    result.reserve(strings.size());
    for (const auto& string : strings) {
        result.push_back(string.c_str());
    }
    return result;
}

} // namespace vkgs::vulkan
