#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace vkgs::offscreen {

std::string formatOutputFilename(std::string_view pattern, size_t frameIndex, std::string_view frameName);

} // namespace vkgs::offscreen
