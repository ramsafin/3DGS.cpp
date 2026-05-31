#include "args.hxx"
#include "spdlog/spdlog.h"

#include <3dgs/Viewer.hpp>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <limits>
#include <utility>

int main(int argc, char** argv) {
    spdlog::set_pattern("[%H:%M:%S] [%^%L%$] %v");

    args::ArgumentParser parser("Vulkan Splatting");
    args::HelpFlag helpFlag{parser, "help", "Display this help menu", {'h', "help"}};
    args::Flag validationLayersFlag{parser, "validation-layers", "Enable Vulkan validation layers", {"validation"}};
    args::Flag verboseFlag{parser, "verbose", "Enable verbose logging", {'v', "verbose"}};
    args::ValueFlag<uint32_t>
        physicalDeviceIdFlag{parser, "physical-device", "Select physical device by index", {'d', "device"}};
    args::Flag immediateSwapchainFlag{
        parser,
        "immediate-swapchain",
        "Set swapchain mode to immediate (VK_PRESENT_MODE_IMMEDIATE_KHR)",
        {'i', "immediate-swapchain"}
    };
    args::ValueFlag<uint32_t> widthFlag{parser, "width", "Set window width", {'w', "width"}};
    args::ValueFlag<uint32_t> heightFlag{parser, "height", "Set window height", {'h', "height"}};
    args::Flag noGuiFlag{parser, "no-gui", "Disable GUI", {"no-gui"}};
    args::Positional<std::string> scenePath{parser, "scene", "Path to scene file", "scene.ply"};

    try {
        parser.ParseCLI(argc, argv);
    } catch (const args::Completion& e) {
        std::cout << e.what();
        return 0;
    } catch (const args::Help&) {
        std::cout << parser;
        return 0;
    } catch (const args::ParseError& e) {
        std::cout << e.what() << '\n';
        std::cout << parser;
        return 1;
    }

    if (args::get(verboseFlag)) {
        spdlog::set_level(spdlog::level::debug);
    }

    vkgs::viewer::ViewerConfig config{};
    config.scene = args::get(scenePath);

    // check that the scene file exists
    if (!std::filesystem::exists(config.scene)) {
        spdlog::critical("File does not exist: {}", config.scene);
        return 0;
    }

    if (validationLayersFlag) {
        config.enableVulkanValidationLayers = args::get(validationLayersFlag);
    }

    if (physicalDeviceIdFlag) {
        const uint32_t deviceId = args::get(physicalDeviceIdFlag);
        if (deviceId > 255) {
            spdlog::critical("--device must be in range 0..255");
            return 1;
        }
        config.physicalDeviceId = std::make_optional<uint8_t>(static_cast<uint8_t>(deviceId));
    }

    if (immediateSwapchainFlag) {
        config.immediateSwapchain = args::get(immediateSwapchainFlag);
    }

    config.enableGui = !noGuiFlag;

    const auto width = widthFlag ? args::get(widthFlag) : 1280u;
    const auto height = heightFlag ? args::get(heightFlag) : 720u;
    if (width > static_cast<uint32_t>(std::numeric_limits<int>::max()) ||
        height > static_cast<uint32_t>(std::numeric_limits<int>::max())) {
        spdlog::critical("Window dimensions must fit in a signed 32-bit integer");
        return 1;
    }
    config.extent = {width, height};

    auto window = vkgs::viewer::makeGlfwWindow("Vulkan Splatting", static_cast<int>(width), static_cast<int>(height));

#ifndef DEBUG
    try {
#endif
        auto renderer = vkgs::viewer::Viewer(config, std::move(window));
        renderer.run();
#ifndef DEBUG
    } catch (const std::exception& e) {
        spdlog::critical(e.what());
        std::cout << e.what() << '\n';
        return 0;
    }
#endif
    return 0;
}
