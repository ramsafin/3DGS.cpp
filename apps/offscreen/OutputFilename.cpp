#include "OutputFilename.h"

#include <filesystem>
#include <stdexcept>

namespace vkgs::offscreen {
namespace {

constexpr size_t kMaxZeroPaddingWidth = 64;

void appendPaddedIndex(std::string& output, size_t frameIndex, size_t width) {
    auto index = std::to_string(frameIndex);
    if (index.size() < width) {
        output.append(width - index.size(), '0');
    }
    output.append(index);
}

void validateFilename(const std::string& filename) {
    const std::filesystem::path path(filename);
    if (filename.empty() || filename == "." || filename == ".." || path.is_absolute() || path.has_parent_path() ||
        filename.find('/') != std::string::npos || filename.find('\\') != std::string::npos) {
        throw std::runtime_error("Output filename pattern must resolve to a single relative filename");
    }
}

} // namespace

std::string formatOutputFilename(std::string_view pattern, size_t frameIndex, std::string_view frameName) {
    std::string output;
    output.reserve(pattern.size() + frameName.size());

    for (size_t i = 0; i < pattern.size();) {
        if (pattern.substr(i).starts_with("{index}")) {
            appendPaddedIndex(output, frameIndex, 0);
            i += std::string_view("{index}").size();
            continue;
        }
        if (pattern.substr(i).starts_with("{name}")) {
            output.append(frameName);
            i += std::string_view("{name}").size();
            continue;
        }
        if (pattern[i] == '{' || pattern[i] == '}') {
            throw std::runtime_error("Unsupported output filename placeholder");
        }
        if (pattern[i] != '%') {
            output.push_back(pattern[i++]);
            continue;
        }

        ++i;
        size_t width = 0;
        if (i < pattern.size() && pattern[i] == '0') {
            ++i;
            const size_t digitsStart = i;
            while (i < pattern.size() && pattern[i] >= '0' && pattern[i] <= '9') {
                width = width * 10 + static_cast<size_t>(pattern[i] - '0');
                if (width > kMaxZeroPaddingWidth) {
                    throw std::runtime_error("Output filename zero-padding width is too large");
                }
                ++i;
            }
            if (i == digitsStart) {
                throw std::runtime_error("Expected a width after %0 in output filename pattern");
            }
        }
        if (i >= pattern.size() || pattern[i] != 'd') {
            throw std::runtime_error("Only %d and %0Nd integer placeholders are supported in output filenames");
        }
        ++i;
        appendPaddedIndex(output, frameIndex, width);
    }

    validateFilename(output);
    return output;
}

} // namespace vkgs::offscreen
