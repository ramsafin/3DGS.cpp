#pragma once

#include <cstdint>
#include <string_view>

namespace vkgs::core {

uint64_t checkedAdd(uint64_t left, uint64_t right, std::string_view context);
uint64_t checkedMultiply(uint64_t left, uint64_t right, std::string_view context);
uint32_t checkedNarrowToUint32(uint64_t value, std::string_view context);

} // namespace vkgs::core
