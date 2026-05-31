#ifndef VKGS_VULKAN_STRING_LIST_H
#define VKGS_VULKAN_STRING_LIST_H

#include <string>
#include <vector>

namespace vkgs::vulkan {

std::vector<const char*> toCStringPointers(const std::vector<std::string>& strings);

} // namespace vkgs::vulkan

#endif // VKGS_VULKAN_STRING_LIST_H
