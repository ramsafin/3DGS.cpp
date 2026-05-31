#pragma once

#include <string>
#include <vector>

namespace vkgs::vulkan {

std::vector<const char*> toCStringPointers(const std::vector<std::string>& strings);

} // namespace vkgs::vulkan
