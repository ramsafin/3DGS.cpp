#include <3dgs/3dgs.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <optional>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using json = nlohmann::json;

struct CliOptions {
    fs::path configPath;
    std::optional<fs::path> outputDirectory;
    std::optional<uint8_t> physicalDeviceId;
    bool validation = false;
    bool verbose = false;
};

void printUsage(const char* program) {
    std::cout << "Usage: " << program
              << " --config <render.json> [--output <dir>] [--device <id>] [--validation] [--verbose]\n";
}

CliOptions parseCli(int argc, char** argv) {
    CliOptions options{};
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            std::exit(0);
        } else if (arg == "--config") {
            if (++i >= argc) {
                throw std::runtime_error("--config requires a path");
            }
            options.configPath = argv[i];
        } else if (arg == "--output") {
            if (++i >= argc) {
                throw std::runtime_error("--output requires a directory");
            }
            options.outputDirectory = fs::path(argv[i]);
        } else if (arg == "--device") {
            if (++i >= argc) {
                throw std::runtime_error("--device requires an index");
            }
            const auto deviceId = std::stoul(argv[i]);
            if (deviceId > 255) {
                throw std::runtime_error("--device must be in range 0..255");
            }
            options.physicalDeviceId = static_cast<uint8_t>(deviceId);
        } else if (arg == "--validation") {
            options.validation = true;
        } else if (arg == "--verbose") {
            options.verbose = true;
        } else {
            throw std::runtime_error("Unknown argument: " + arg);
        }
    }

    if (options.configPath.empty()) {
        throw std::runtime_error("Missing required --config argument");
    }
    return options;
}

json loadJson(const fs::path& path) {
    std::ifstream stream(path);
    if (!stream.is_open()) {
        throw std::runtime_error("Failed to open config: " + path.string());
    }
    json config;
    stream >> config;
    return config;
}

fs::path resolvePath(const fs::path& base, const fs::path& path) {
    if (path.is_absolute()) {
        return path;
    }
    return base / path;
}

std::array<float, 3> readVec3(const json& value, const std::string& field) {
    if (!value.is_array() || value.size() != 3) {
        throw std::runtime_error(field + " must be an array of 3 numbers");
    }
    return {value[0].get<float>(), value[1].get<float>(), value[2].get<float>()};
}

std::array<float, 4> readQuat(const json& value, const std::string& field) {
    if (!value.is_array() || value.size() != 4) {
        throw std::runtime_error(field + " must be an array of 4 numbers in [w, x, y, z] order");
    }
    return {value[0].get<float>(), value[1].get<float>(), value[2].get<float>(), value[3].get<float>()};
}

std::string formatFilename(std::string pattern, int frameIndex, const std::string& frameName) {
    auto replaceAll = [](std::string& text, const std::string& from, const std::string& to) {
        size_t pos = 0;
        while ((pos = text.find(from, pos)) != std::string::npos) {
            text.replace(pos, from.length(), to);
            pos += to.length();
        }
    };

    replaceAll(pattern, "{name}", frameName);
    replaceAll(pattern, "{index}", std::to_string(frameIndex));

    if (pattern.find('%') != std::string::npos) {
        std::vector<char> buffer(1024);
        const int written = std::snprintf(buffer.data(), buffer.size(), pattern.c_str(), frameIndex);
        if (written < 0) {
            throw std::runtime_error("Failed to format output filename");
        }
        if (static_cast<size_t>(written) >= buffer.size()) {
            buffer.resize(static_cast<size_t>(written) + 1);
            std::snprintf(buffer.data(), buffer.size(), pattern.c_str(), frameIndex);
        }
        return buffer.data();
    }

    return pattern;
}

void writePpm(const fs::path& path, const std::vector<uint8_t>& rgba, uint32_t width, uint32_t height) {
    const size_t expectedSize = static_cast<size_t>(width) * height * 4;
    if (rgba.size() != expectedSize) {
        throw std::runtime_error("Unexpected RGBA buffer size for PPM output");
    }

    if (path.has_parent_path()) {
        fs::create_directories(path.parent_path());
    }

    std::ofstream stream(path, std::ios::binary);
    if (!stream.is_open()) {
        throw std::runtime_error("Failed to open output file: " + path.string());
    }

    stream << "P6\n" << width << " " << height << "\n255\n";
    for (size_t i = 0; i < expectedSize; i += 4) {
        stream.put(static_cast<char>(rgba[i]));
        stream.put(static_cast<char>(rgba[i + 1]));
        stream.put(static_cast<char>(rgba[i + 2]));
    }
}

int main(int argc, char** argv) {
    spdlog::set_pattern("[%H:%M:%S] [%^%L%$] %v");

    try {
        auto cli = parseCli(argc, argv);
        if (cli.verbose) {
            spdlog::set_level(spdlog::level::debug);
        }

        const auto configPath = fs::absolute(cli.configPath);
        const auto configBase = configPath.parent_path();
        const auto config = loadJson(configPath);

        const auto scenePath = resolvePath(configBase, config.at("scene").get<std::string>());
        if (!fs::exists(scenePath)) {
            throw std::runtime_error("Scene file does not exist: " + scenePath.string());
        }

        const auto render = config.value("render", json::object());
        const auto output = config.value("output", json::object());
        const auto vulkan = config.value("vulkan", json::object());
        const auto frames = config.at("frames");
        if (!frames.is_array() || frames.empty()) {
            throw std::runtime_error("frames must be a non-empty array");
        }

        const uint32_t width = render.value("width", 1280u);
        const uint32_t height = render.value("height", 720u);
        if (width == 0 || height == 0) {
            throw std::runtime_error("render.width and render.height must be positive");
        }
        const std::string outputFormat = output.value("format", "ppm");
        if (outputFormat != "ppm") {
            throw std::runtime_error("Only ppm output is currently supported");
        }

        fs::path outputDirectory =
            cli.outputDirectory.value_or(resolvePath(configBase, output.value("directory", "renders")));
        fs::create_directories(outputDirectory);

        VulkanSplatting::RendererConfiguration rendererConfig{};
        rendererConfig.enableVulkanValidationLayers = cli.validation || vulkan.value("validation", false);
        rendererConfig.scene = scenePath.string();
        rendererConfig.width = width;
        rendererConfig.height = height;
        rendererConfig.fov = render.value("fov_degrees", 45.0f);
        rendererConfig.near = render.value("near", 0.2f);
        rendererConfig.far = render.value("far", 1000.0f);

        if (cli.physicalDeviceId.has_value()) {
            rendererConfig.physicalDeviceId = cli.physicalDeviceId;
        } else if (vulkan.contains("physical_device") && !vulkan.at("physical_device").is_null()) {
            const auto deviceId = vulkan.at("physical_device").get<uint32_t>();
            if (deviceId > 255) {
                throw std::runtime_error("vulkan.physical_device must be in range 0..255");
            }
            rendererConfig.physicalDeviceId = static_cast<uint8_t>(deviceId);
        }

        VulkanSplatting renderer(rendererConfig);
        renderer.initialize();
        renderer.setCameraProjection(rendererConfig.fov, rendererConfig.near, rendererConfig.far);

        const std::string filenamePattern = output.value("filename_pattern", "frame_%04d.ppm");
        for (size_t i = 0; i < frames.size(); ++i) {
            const auto& frame = frames[i];
            const auto position = readVec3(frame.at("position"), "frames[].position");
            const auto rotation = readQuat(frame.at("rotation_quat"), "frames[].rotation_quat");

            renderer.setCameraProjection(frame.value("fov_degrees", rendererConfig.fov),
                                         frame.value("near", rendererConfig.near),
                                         frame.value("far", rendererConfig.far));

            renderer.setCameraPose(position[0], position[1], position[2], rotation[0], rotation[1], rotation[2],
                                   rotation[3]);
            renderer.draw();
            auto pixels = renderer.readPixels();

            const auto frameName = frame.value("name", std::to_string(i));
            const auto filename = formatFilename(filenamePattern, static_cast<int>(i), frameName);
            const auto outputPath = outputDirectory / filename;
            writePpm(outputPath, pixels, width, height);
            spdlog::info("Wrote {}", outputPath.string());
        }

        renderer.stop();
    } catch (const std::exception& e) {
        spdlog::critical(e.what());
        printUsage(argv[0]);
        return 1;
    }

    return 0;
}
