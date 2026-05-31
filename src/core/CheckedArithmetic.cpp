#include "CheckedArithmetic.h"

#include <limits>
#include <stdexcept>
#include <string>

namespace vkgs::core {

uint64_t checkedAdd(uint64_t left, uint64_t right, std::string_view context) {
    if (right > std::numeric_limits<uint64_t>::max() - left) {
        throw std::overflow_error(std::string(context) + " exceeds uint64_t");
    }
    return left + right;
}

uint64_t checkedMultiply(uint64_t left, uint64_t right, std::string_view context) {
    if (left != 0 && right > std::numeric_limits<uint64_t>::max() / left) {
        throw std::overflow_error(std::string(context) + " exceeds uint64_t");
    }
    return left * right;
}

uint32_t checkedNarrowToUint32(uint64_t value, std::string_view context) {
    if (value > std::numeric_limits<uint32_t>::max()) {
        throw std::overflow_error(std::string(context) + " exceeds uint32_t");
    }
    return static_cast<uint32_t>(value);
}

} // namespace vkgs::core
