#include <3dgs/OffscreenRenderer.hpp>
#include "OutputFilename.hpp"
#include "PpmWriter.hpp"
#include "RenderJob.hpp"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

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

int main(int argc, char** argv) {
    spdlog::set_pattern("[%H:%M:%S] [%^%L%$] %v");

    try {
        auto cli = parseCli(argc, argv);
        if (cli.verbose) {
            spdlog::set_level(spdlog::level::debug);
        }

        const auto job = vkgs::offscreen::loadRenderJob(cli.configPath, cli.outputDirectory, cli.physicalDeviceId,
                                                        cli.validation);
        fs::create_directories(job.outputDirectory);
        vkgs::OffscreenRenderer renderer(job.renderer);
        for (size_t index = 0; index < job.frames.size(); ++index) {
            const auto& frame = job.frames[index];
            renderer.render(frame.camera, frame.projection);
            auto pixels = renderer.readPixels();
            const auto filename = vkgs::offscreen::formatOutputFilename(job.filenamePattern, index, frame.name);
            const auto outputPath = job.outputDirectory / filename;
            vkgs::offscreen::writePpm(outputPath, pixels, job.renderer.extent);
            spdlog::info("Wrote {}", outputPath.string());
        }

    } catch (const std::exception& e) {
        spdlog::critical(e.what());
        printUsage(argv[0]);
        return 1;
    }

    return 0;
}
