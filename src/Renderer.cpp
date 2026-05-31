#include "Renderer.h"

#include "render/GpuTypes.h"
#include "render/PassSizing.h"
#include "render/RenderConstants.h"
#include "scene/PlyReader.h"
#include <fstream>

#ifdef VKGS_RENDER_MODE_ONSCREEN
#include "vulkan/Swapchain.h"
#include "vulkan/windowing/GLFWWindow.h"
#endif

#include "GpuConstants.h"
#include "shaders.h"
#include "vulkan/BarrierBuilder.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <cmath>
#include <memory>
#include <spdlog/spdlog.h>
#include <utility>

namespace sizing = vkgs::render;

void Renderer::initialize() {
    initializeVulkan();
    createGui();
    loadSceneToGPU();
    createPreprocessPipeline();
    createPrefixSumPipeline();
    createRadixSortPipeline();
    createPreprocessSortPipeline();
    createTileBoundaryPipeline();
    createRenderPipeline();
    createCommandPool();
    recordPreprocessCommandBuffer();
}

void Renderer::handleInput() {
#ifdef VKGS_RENDER_MODE_ONSCREEN
    constexpr float kOrbitSensitivity = 0.005f;
    constexpr float kPanSensitivity = 0.002f;
    constexpr float kDollySensitivity = 0.1f;
    constexpr float kFlySpeedFactor = 0.01f;

    if (!configuration.enableGui || !guiManager.wantCaptureMouse()) {
        const auto& mouse = window->getMouseButton();
        const auto translation = window->getCursorTranslation();
        const double scroll = window->getScrollDelta();

        if (mouse[1]) {
            camera.controller.orbit(static_cast<float>(translation[0]), static_cast<float>(translation[1]),
                                    kOrbitSensitivity);
        } else if (mouse[2]) {
            camera.controller.pan(static_cast<float>(translation[0]), static_cast<float>(translation[1]),
                                  kPanSensitivity);
        }

        if (scroll != 0.0) {
            camera.controller.dolly(static_cast<float>(scroll), kDollySensitivity);
        }
    }

    if (!configuration.enableGui || !guiManager.wantCaptureKeyboard()) {
        const auto keys = window->getKeys();
        glm::vec3 direction = glm::vec3(0.0f, 0.0f, 0.0f);
        if (keys[0]) {
            direction += glm::vec3(0.0f, 0.0f, -1.0f);
        }
        if (keys[1]) {
            direction += glm::vec3(-1.0f, 0.0f, 0.0f);
        }
        if (keys[2]) {
            direction += glm::vec3(0.0f, 0.0f, 1.0f);
        }
        if (keys[3]) {
            direction += glm::vec3(1.0f, 0.0f, 0.0f);
        }
        if (keys[4]) {
            direction += glm::vec3(0.0f, 1.0f, 0.0f);
        }
        if (keys[5]) {
            direction += glm::vec3(0.0f, -1.0f, 0.0f);
        }
        if (direction != glm::vec3(0.0f, 0.0f, 0.0f)) {
            const float flySpeed = std::max(camera.controller.orbitDistance * kFlySpeedFactor, 0.01f);
            camera.controller.fly(direction, flySpeed);
        }
    }
#endif
}

void Renderer::toggleFrameRotation180() {
    const auto roll180 = glm::angleAxis(glm::pi<float>(), glm::vec3(0.0f, 0.0f, -1.0f));
    camera.controller.rotation = glm::normalize(camera.controller.rotation * roll180);
    camera.controller.syncFocusFromPose();
    framesRotated180 = !framesRotated180;
    guiManager.viewRotated180 = framesRotated180;
}

void Renderer::processGuiCameraRequests() {
#ifdef VKGS_RENDER_MODE_ONSCREEN
    if (guiManager.frameRotationToggleRequested) {
        guiManager.frameRotationToggleRequested = false;
        toggleFrameRotation180();
    }
    if (guiManager.frameSceneRequested) {
        guiManager.frameSceneRequested = false;
        if (scene != nullptr) {
            const auto& bounds = scene->getBounds();
            camera.controller.frameScene(bounds.center, bounds.radius, camera.fov);
        }
    }
    if (guiManager.resetCameraRequested) {
        guiManager.resetCameraRequested = false;
        camera.controller.reset();
    }
#endif
}

void Renderer::retrieveTimestamps() {
    if (!context->supportsTimestampQueries() || queryManager.nextId == 0) {
        return;
    }

    std::vector<uint64_t> timestamps(queryManager.nextId);
    auto res = context->device->getQueryPoolResults(
        context->queryPool.get(), 0, queryManager.nextId, timestamps.size() * sizeof(uint64_t), timestamps.data(),
        sizeof(uint64_t), vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWait);
    if (res != vk::Result::eSuccess) {
        throw std::runtime_error("Failed to retrieve timestamps");
    }

    auto metrics = queryManager.parseResults(timestamps);
    for (auto& metric : metrics) {
        if (configuration.enableGui) {
            const auto timestampPeriod = context->physicalDevice.getProperties().limits.timestampPeriod;
            guiManager.pushMetric(metric.first,
                                  static_cast<float>(timestampTicksToMilliseconds(metric.second, timestampPeriod)));
        }
    }
}

void Renderer::resetTimestampQueries(vk::CommandBuffer commandBuffer) {
    if (context->supportsTimestampQueries()) {
        commandBuffer.resetQueryPool(context->queryPool.get(), 0, VulkanContext::kTimestampQueryCount);
    }
}

void Renderer::writeTimestamp(vk::CommandBuffer commandBuffer, const std::string& name) {
    if (context->supportsTimestampQueries()) {
        commandBuffer.writeTimestamp(vk::PipelineStageFlagBits::eComputeShader, context->queryPool.get(),
                                     queryManager.registerQuery(name));
    }
}

void Renderer::recreateSwapchain() {
#ifdef VKGS_RENDER_MODE_ONSCREEN
    const auto [framebufferWidth, framebufferHeight] = window->getFramebufferSize();
    if (framebufferWidth == 0 || framebufferHeight == 0) {
        return;
    }

    auto oldExtent = swapchain->swapchainExtent;
    spdlog::debug("Recreating swapchain");
    swapchain->recreate();
    if (swapchain->swapchainExtent != oldExtent) {
        auto [width, height] = getRenderExtent();
        tileBoundaryBuffer->realloc(sizing::tileBoundaryBytes(width, height));
        recordPreprocessCommandBuffer();
    }
    createRenderPipeline();
    if (imguiManager != nullptr) {
        imguiManager->onSwapchainRecreated();
    }
#endif
}

void Renderer::initializeVulkan() {
    spdlog::debug("Initializing Vulkan");
#ifdef VKGS_RENDER_MODE_ONSCREEN
    window = configuration.window;
    context = std::make_shared<VulkanContext>(window->getRequiredInstanceExtensions(), std::vector<std::string>{},
                                              configuration.enableVulkanValidationLayers);
#else
    context = std::make_shared<VulkanContext>(std::vector<std::string>{}, std::vector<std::string>{},
                                              configuration.enableVulkanValidationLayers);
#endif

    context->createInstance();
#ifdef VKGS_RENDER_MODE_ONSCREEN
    auto surface = static_cast<vk::SurfaceKHR>(window->createSurface(context));
    context->selectPhysicalDevice(configuration.physicalDeviceId, surface);
#else
    context->selectPhysicalDevice(configuration.physicalDeviceId, std::nullopt);
#endif

    vk::PhysicalDeviceFeatures pdf{};
    vk::PhysicalDeviceVulkan11Features pdf11{};
    vk::PhysicalDeviceVulkan12Features pdf12{};
    pdf.shaderStorageImageWriteWithoutFormat = true;
    pdf.shaderInt64 = true;
    pdf12.shaderSharedInt64Atomics = context->getRadixSortMode() == VulkanContext::RadixSortMode::FastSubgroup32;

    context->createLogicalDevice(pdf, pdf11, pdf12);
    context->createDescriptorPool(1);
    if (context->supportsTimestampQueries()) {
        queryManager.setCapacity(VulkanContext::kTimestampQueryCount);
    }

#ifdef VKGS_RENDER_MODE_ONSCREEN
    swapchain = std::make_shared<Swapchain>(context, window, configuration.immediateSwapchain);
#else
    offscreenRenderTarget = std::make_shared<OffscreenRenderTarget>(context, configuration.width, configuration.height);
#endif

    for (uint32_t i = 0; i < vkgs::render::kFramesInFlight; i++) {
        inflightFences.emplace_back(
            context->device->createFenceUnique(vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled)));
    }

#ifdef VKGS_RENDER_MODE_ONSCREEN
    renderFinishedSemaphores.resize(vkgs::render::kFramesInFlight);
    for (uint32_t i = 0; i < vkgs::render::kFramesInFlight; i++) {
        renderFinishedSemaphores[i] = context->device->createSemaphoreUnique(vk::SemaphoreCreateInfo());
    }
#endif
}

void Renderer::loadSceneToGPU() {
    spdlog::debug("Loading scene to GPU");
    scene = std::make_unique<vkgs::scene::GpuScene>(vkgs::scene::PlyReader(configuration.scene).read());
    scene->upload(context);

    const auto& bounds = scene->getBounds();
    camera.controller.frameScene(bounds.center, bounds.radius, camera.fov);

    // reset descriptor pool
    context->device->resetDescriptorPool(context->descriptorPool.get());
}

void Renderer::createPreprocessPipeline() {
    spdlog::debug("Creating preprocess pipeline");
    uniformBuffer = Buffer::uniform(context, sizeof(vkgs::render::UniformBuffer));
    vertexAttributeBuffer =
        Buffer::storage(context, sizing::bytesFor(scene->getNumVertices(), sizeof(vkgs::render::VertexAttribute),
                                                  "Projected vertex buffer size"),
                        false);
    tileOverlapBuffer =
        Buffer::storage(context, sizing::bytesFor(scene->getNumVertices(), sizeof(uint32_t), "Tile overlap buffer size"),
                        false);

    preprocessPipeline = std::make_shared<ComputePipeline>(
        context, std::make_shared<Shader>(context, "preprocess", SPV_PREPROCESS, SPV_PREPROCESS_len));
    inputSet = std::make_shared<DescriptorSet>(context, vkgs::render::kFramesInFlight);
    inputSet->bindBufferToDescriptorSet(0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                        scene->vertexBuffer);
    inputSet->bindBufferToDescriptorSet(1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                        scene->cov3DBuffer);
    inputSet->build();
    preprocessPipeline->addDescriptorSet(0, inputSet);

    auto uniformOutputSet = std::make_shared<DescriptorSet>(context, vkgs::render::kFramesInFlight);
    uniformOutputSet->bindBufferToDescriptorSet(0, vk::DescriptorType::eUniformBuffer,
                                                vk::ShaderStageFlagBits::eCompute, uniformBuffer);
    uniformOutputSet->bindBufferToDescriptorSet(1, vk::DescriptorType::eStorageBuffer,
                                                vk::ShaderStageFlagBits::eCompute, vertexAttributeBuffer);
    uniformOutputSet->bindBufferToDescriptorSet(2, vk::DescriptorType::eStorageBuffer,
                                                vk::ShaderStageFlagBits::eCompute, tileOverlapBuffer);
    uniformOutputSet->build();

    preprocessPipeline->addDescriptorSet(1, uniformOutputSet);
    preprocessPipeline->build();
}

Renderer::Renderer(vkgs::render::RendererConfiguration configuration) : configuration(std::move(configuration)) {
    // Render dimensions feed buffer sizes, dispatch counts, and FOV math; zero
    // values cause divide-by-zero and zero-sized allocations (VKGS-016).
    if (this->configuration.width == 0 || this->configuration.height == 0) {
        throw std::runtime_error("Render dimensions must be non-zero");
    }
    validateCameraProjection(this->configuration.fov, this->configuration.nearPlane, this->configuration.farPlane);
    camera.fov = this->configuration.fov;
    camera.nearPlane = this->configuration.nearPlane;
    camera.farPlane = this->configuration.farPlane;
}

void Renderer::setCameraPose(float px, float py, float pz, float qw, float qx, float qy, float qz) {
    camera.controller.setPose(glm::vec3(px, py, pz), glm::quat(qw, qx, qy, qz));
}

void Renderer::setCameraProjection(float fovDegrees, float nearPlane, float farPlane) {
    validateCameraProjection(fovDegrees, nearPlane, farPlane);
    camera.fov = fovDegrees;
    camera.nearPlane = nearPlane;
    camera.farPlane = farPlane;
}

void Renderer::validateCameraProjection(float fovDegrees, float nearPlane, float farPlane) {
    if (!std::isfinite(fovDegrees) || fovDegrees <= 0.0f || fovDegrees >= 180.0f) {
        throw std::runtime_error("Camera FOV must be finite and in the range (0, 180) degrees");
    }
    if (!std::isfinite(nearPlane) || nearPlane <= 0.0f) {
        throw std::runtime_error("Camera near plane must be finite and positive");
    }
    if (!std::isfinite(farPlane) || farPlane <= nearPlane) {
        throw std::runtime_error("Camera far plane must be finite and greater than the near plane");
    }
}

void Renderer::createGui() {
#ifdef VKGS_RENDER_MODE_ONSCREEN
    if (!configuration.enableGui) {
        return;
    }

    spdlog::debug("Creating GUI");

    auto glfwWindow = std::dynamic_pointer_cast<GLFWWindow>(window);
    if (glfwWindow == nullptr) {
        throw std::runtime_error("The bundled ImGui overlay requires a GLFW window");
    }
    imguiManager = std::make_shared<ImguiManager>(context, swapchain, std::move(glfwWindow));
    imguiManager->init();
    guiManager.init();
#endif
}

vk::Extent2D Renderer::getRenderExtent() const {
#ifdef VKGS_RENDER_MODE_ONSCREEN
    return swapchain->swapchainExtent;
#else
    return offscreenRenderTarget->extent;
#endif
}

std::span<const vkgs::vulkan::RenderImageView> Renderer::getRenderImages() const {
#ifdef VKGS_RENDER_MODE_ONSCREEN
    return swapchain->swapchainImages;
#else
    return offscreenRenderTarget->images;
#endif
}

const vkgs::vulkan::RenderImageView& Renderer::getCurrentRenderImage() const {
    return getRenderImages()[currentImageIndex];
}

void Renderer::createPrefixSumPipeline() {
    spdlog::debug("Creating prefix sum pipeline");
    const auto prefixSumBytes = sizing::bytesFor(scene->getNumVertices(), sizeof(uint32_t), "Prefix sum buffer size");
    prefixSumPingBuffer = Buffer::storage(context, prefixSumBytes, false);
    prefixSumPongBuffer = Buffer::storage(context, prefixSumBytes, false);
    totalSumBufferHost = Buffer::staging(context, sizeof(uint32_t));

    prefixSumPipeline = std::make_shared<ComputePipeline>(
        context, std::make_shared<Shader>(context, "prefix_sum", SPV_PREFIX_SUM, SPV_PREFIX_SUM_len));
    auto descriptorSet = std::make_shared<DescriptorSet>(context, vkgs::render::kFramesInFlight);
    descriptorSet->bindBufferToDescriptorSet(0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                             prefixSumPingBuffer);
    descriptorSet->bindBufferToDescriptorSet(1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                             prefixSumPongBuffer);
    descriptorSet->build();

    prefixSumPipeline->addDescriptorSet(0, descriptorSet);
    prefixSumPipeline->addPushConstant(vk::ShaderStageFlagBits::eCompute, 0, sizeof(uint32_t));
    prefixSumPipeline->build();
}

void Renderer::createRadixSortPipeline() {
    spdlog::debug("Creating radix sort pipeline");
    const auto capacity = sizing::sortCapacity(scene->getNumVertices(), sortBufferSizeMultiplier);
    sortKBufferEven =
        Buffer::storage(context, sizing::bytesFor(capacity, sizeof(uint64_t), "Even sort key buffer size"), false, 0,
                        "sortKBufferEven");
    sortKBufferOdd =
        Buffer::storage(context, sizing::bytesFor(capacity, sizeof(uint64_t), "Odd sort key buffer size"), false, 0,
                        "sortKBufferOdd");
    sortVBufferEven =
        Buffer::storage(context, sizing::bytesFor(capacity, sizeof(uint32_t), "Even sort payload buffer size"), false, 0,
                        "sortVBufferEven");
    sortVBufferOdd =
        Buffer::storage(context, sizing::bytesFor(capacity, sizeof(uint32_t), "Odd sort payload buffer size"), false, 0,
                        "sortVBufferOdd");

    auto numWorkgroups = sizing::radixSortWorkgroupCount(capacity, numRadixSortBlocksPerWorkgroup);

    sortHistBuffer = Buffer::storage(context, sizing::sortHistogramBytes(numWorkgroups), false);

    sortHistPipeline =
        std::make_shared<ComputePipeline>(context, std::make_shared<Shader>(context, "hist", SPV_HIST, SPV_HIST_len));
    if (context->getRadixSortMode() == VulkanContext::RadixSortMode::FastSubgroup32) {
        sortPipeline =
            std::make_shared<ComputePipeline>(context, std::make_shared<Shader>(context, "sort", SPV_SORT, SPV_SORT_len));
    } else {
        sortPipeline = std::make_shared<ComputePipeline>(
            context, std::make_shared<Shader>(context, "sort_portable", SPV_SORT_PORTABLE, SPV_SORT_PORTABLE_len));
    }

    auto descriptorSet = std::make_shared<DescriptorSet>(context, vkgs::render::kFramesInFlight);
    descriptorSet->bindBufferToDescriptorSet(0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                             sortKBufferEven);
    descriptorSet->bindBufferToDescriptorSet(0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                             sortKBufferOdd);
    descriptorSet->bindBufferToDescriptorSet(1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                             sortHistBuffer);
    descriptorSet->build();
    sortHistPipeline->addDescriptorSet(0, descriptorSet);
    sortHistPipeline->addPushConstant(vk::ShaderStageFlagBits::eCompute, 0,
                                      sizeof(vkgs::render::RadixSortPushConstants));
    sortHistPipeline->build();

    descriptorSet = std::make_shared<DescriptorSet>(context, vkgs::render::kFramesInFlight);
    descriptorSet->bindBufferToDescriptorSet(0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                             sortKBufferEven);
    descriptorSet->bindBufferToDescriptorSet(0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                             sortKBufferOdd);
    descriptorSet->bindBufferToDescriptorSet(1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                             sortKBufferOdd);
    descriptorSet->bindBufferToDescriptorSet(1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                             sortKBufferEven);
    descriptorSet->bindBufferToDescriptorSet(2, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                             sortVBufferEven);
    descriptorSet->bindBufferToDescriptorSet(2, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                             sortVBufferOdd);
    descriptorSet->bindBufferToDescriptorSet(3, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                             sortVBufferOdd);
    descriptorSet->bindBufferToDescriptorSet(3, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                             sortVBufferEven);
    descriptorSet->bindBufferToDescriptorSet(4, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                             sortHistBuffer);
    descriptorSet->build();
    sortPipeline->addDescriptorSet(0, descriptorSet);
    sortPipeline->addPushConstant(vk::ShaderStageFlagBits::eCompute, 0, sizeof(vkgs::render::RadixSortPushConstants));
    sortPipeline->build();
}

void Renderer::createPreprocessSortPipeline() {
    spdlog::debug("Creating preprocess sort pipeline");
    preprocessSortPipeline = std::make_shared<ComputePipeline>(
        context, std::make_shared<Shader>(context, "preprocess_sort", SPV_PREPROCESS_SORT, SPV_PREPROCESS_SORT_len));
    auto descriptorSet = std::make_shared<DescriptorSet>(context, vkgs::render::kFramesInFlight);
    descriptorSet->bindBufferToDescriptorSet(0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                             vertexAttributeBuffer);
    descriptorSet->bindBufferToDescriptorSet(1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                             prefixSumPingBuffer);
    descriptorSet->bindBufferToDescriptorSet(1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                             prefixSumPongBuffer);
    descriptorSet->bindBufferToDescriptorSet(2, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                             sortKBufferEven);
    descriptorSet->bindBufferToDescriptorSet(3, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                             sortVBufferEven);
    descriptorSet->build();

    preprocessSortPipeline->addDescriptorSet(0, descriptorSet);
    preprocessSortPipeline->addPushConstant(vk::ShaderStageFlagBits::eCompute, 0, sizeof(uint32_t));
    preprocessSortPipeline->build();
}

void Renderer::createTileBoundaryPipeline() {
    spdlog::debug("Creating tile boundary pipeline");
    auto [width, height] = getRenderExtent();
    tileBoundaryBuffer = Buffer::storage(context, sizing::tileBoundaryBytes(width, height), false);

    tileBoundaryPipeline = std::make_shared<ComputePipeline>(
        context, std::make_shared<Shader>(context, "tile_boundary", SPV_TILE_BOUNDARY, SPV_TILE_BOUNDARY_len));
    auto descriptorSet = std::make_shared<DescriptorSet>(context, vkgs::render::kFramesInFlight);
    descriptorSet->bindBufferToDescriptorSet(0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                             sortKBufferEven);
    // descriptorSet->bindBufferToDescriptorSet(0, vk::DescriptorType::eStorageBuffer,
    // vk::ShaderStageFlagBits::eCompute,
    //                                          sortKBufferOdd);
    descriptorSet->bindBufferToDescriptorSet(1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                             tileBoundaryBuffer);
    descriptorSet->build();

    tileBoundaryPipeline->addDescriptorSet(0, descriptorSet);
    tileBoundaryPipeline->addPushConstant(vk::ShaderStageFlagBits::eCompute, 0, sizeof(uint32_t));
    tileBoundaryPipeline->build();
}

void Renderer::createRenderPipeline() {
    spdlog::debug("Creating render pipeline");
    renderPipeline = std::make_shared<ComputePipeline>(
        context, std::make_shared<Shader>(context, "render", SPV_RENDER, SPV_RENDER_len));
    auto inputSet = std::make_shared<DescriptorSet>(context, vkgs::render::kFramesInFlight);
    inputSet->bindBufferToDescriptorSet(0, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                        vertexAttributeBuffer);
    inputSet->bindBufferToDescriptorSet(1, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                        tileBoundaryBuffer);
    inputSet->bindBufferToDescriptorSet(2, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
                                        sortVBufferEven);
    // inputSet->bindBufferToDescriptorSet(2, vk::DescriptorType::eStorageBuffer, vk::ShaderStageFlagBits::eCompute,
    //                                     sortKBufferOdd);
    inputSet->build();

    auto outputSet = std::make_shared<DescriptorSet>(context, 1);
    for (auto& image : getRenderImages()) {
        outputSet->bindImageToDescriptorSet(0, vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute,
                                            image);
    }
    outputSet->build();
    renderPipeline->addDescriptorSet(0, inputSet);
    renderPipeline->addDescriptorSet(1, outputSet);
    renderPipeline->addPushConstant(vk::ShaderStageFlagBits::eCompute, 0, sizeof(uint32_t) * 2);
    renderPipeline->build();
}

void Renderer::draw() {
    auto ret = context->device->waitForFences(inflightFences[0].get(), VK_TRUE, UINT64_MAX);
    if (ret != vk::Result::eSuccess) {
        throw std::runtime_error("Failed to wait for fence");
    }
#ifdef VKGS_RENDER_MODE_ONSCREEN
    auto res =
        context->device->acquireNextImageKHR(swapchain->swapchain.get(), UINT64_MAX,
                                             swapchain->imageAvailableSemaphores[0].get(), nullptr, &currentImageIndex);
    if (res == vk::Result::eErrorOutOfDateKHR) {
        recreateSwapchain();
        return;
    } else if (res != vk::Result::eSuccess && res != vk::Result::eSuboptimalKHR) {
        throw std::runtime_error("Failed to acquire swapchain image");
    }
#else
    currentImageIndex = 0;
#endif
    context->device->resetFences(inflightFences[0].get());

    // Retry loop: recordRenderCommandBuffer returns false when it had to grow
    // the sort buffers and re-record preprocessing, requiring another pass
    // (VKGS-022, formerly a goto).
    vk::SubmitInfo submitInfo;
    while (true) {
        handleInput();

        updateUniforms();

        submitInfo = vk::SubmitInfo{}.setCommandBuffers(preprocessCommandBuffer.get());
        context->queues[VulkanContext::Queue::COMPUTE].queue.submit(submitInfo, inflightFences[0].get());

        ret = context->device->waitForFences(inflightFences[0].get(), VK_TRUE, UINT64_MAX);
        if (ret != vk::Result::eSuccess) {
            throw std::runtime_error("Failed to wait for fence");
        }
        context->device->resetFences(inflightFences[0].get());

        if (!recordRenderCommandBuffer(0)) {
            continue;
        }
        break;
    }
#ifdef VKGS_RENDER_MODE_ONSCREEN
    vk::PipelineStageFlags waitStage = vk::PipelineStageFlagBits::eComputeShader;
    submitInfo = vk::SubmitInfo{}
                     .setWaitSemaphores(swapchain->imageAvailableSemaphores[0].get())
                     .setCommandBuffers(renderCommandBuffer.get())
                     .setSignalSemaphores(renderFinishedSemaphores[0].get())
                     .setWaitDstStageMask(waitStage);
#else
    submitInfo = vk::SubmitInfo{}.setCommandBuffers(renderCommandBuffer.get());
#endif
    context->queues[VulkanContext::Queue::COMPUTE].queue.submit(submitInfo, inflightFences[0].get());

#ifdef VKGS_RENDER_MODE_ONSCREEN
    vk::PresentInfoKHR presentInfo{};
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderFinishedSemaphores[0].get();
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain->swapchain.get();
    presentInfo.pImageIndices = &currentImageIndex;

    try {
        ret = context->queues[VulkanContext::Queue::PRESENT].queue.presentKHR(presentInfo);
    } catch (vk::OutOfDateKHRError& e) {
        recreateSwapchain();
        return;
    }

    if (ret == vk::Result::eErrorOutOfDateKHR || ret == vk::Result::eSuboptimalKHR) {
        recreateSwapchain();
    } else if (ret != vk::Result::eSuccess) {
        throw std::runtime_error("Failed to present swapchain image");
    }
#endif
}

std::vector<uint8_t> Renderer::readPixels() const {
#ifdef VKGS_RENDER_MODE_OFFSCREEN
    auto ret = context->device->waitForFences(inflightFences[0].get(), VK_TRUE, UINT64_MAX);
    if (ret != vk::Result::eSuccess) {
        throw std::runtime_error("Failed to wait for render fence before reading pixels");
    }

    auto [width, height] = getRenderExtent();
    const vk::DeviceSize pixelSize = 4u;
    const auto byteSize =
        sizing::bytesFor(sizing::bytesFor(width, height, "Offscreen pixel count"), pixelSize,
                         "Offscreen readback byte size");
    auto stagingBuffer = Buffer::staging(context, byteSize);
    auto image = getCurrentRenderImage();

    auto commandBuffer = context->beginOneTimeCommandBuffer();
    vk::ImageMemoryBarrier imageMemoryBarrier{};
    imageMemoryBarrier.oldLayout = vk::ImageLayout::eGeneral;
    imageMemoryBarrier.newLayout = vk::ImageLayout::eTransferSrcOptimal;
    imageMemoryBarrier.image = image.image;
    imageMemoryBarrier.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
    imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
    imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eTransferRead;
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    commandBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader, vk::PipelineStageFlagBits::eTransfer,
                                   vk::DependencyFlagBits::eByRegion, nullptr, nullptr, imageMemoryBarrier);

    vk::BufferImageCopy copyRegion{};
    copyRegion.bufferOffset = 0;
    copyRegion.bufferRowLength = 0;
    copyRegion.bufferImageHeight = 0;
    copyRegion.imageSubresource = {vk::ImageAspectFlagBits::eColor, 0, 0, 1};
    copyRegion.imageOffset = vk::Offset3D{0, 0, 0};
    copyRegion.imageExtent = vk::Extent3D{width, height, 1};
    commandBuffer->copyImageToBuffer(image.image, vk::ImageLayout::eTransferSrcOptimal, stagingBuffer->buffer, 1,
                                     &copyRegion);

    imageMemoryBarrier.oldLayout = vk::ImageLayout::eTransferSrcOptimal;
    imageMemoryBarrier.newLayout = vk::ImageLayout::eGeneral;
    imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eTransferRead;
    imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eShaderWrite;
    commandBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader,
                                   vk::DependencyFlagBits::eByRegion, nullptr, nullptr, imageMemoryBarrier);

    context->endOneTimeCommandBuffer(std::move(commandBuffer), VulkanContext::Queue::COMPUTE);

    stagingBuffer->invalidate();
    auto* data = static_cast<uint8_t*>(stagingBuffer->allocation_info.pMappedData);
    return {data, data + byteSize};
#else
    return {};
#endif
}

void Renderer::run() {
#ifdef VKGS_RENDER_MODE_ONSCREEN
    while (running) {
        if (!window->tick()) {
            break;
        }

        draw();

        auto now = std::chrono::high_resolution_clock::now();
        auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastFpsTime).count();
        if (diff > 1000) {
            spdlog::debug("FPS: {}", fpsCounter);
            fpsCounter = 0;
            lastFpsTime = now;
        } else {
            fpsCounter++;
        }

        retrieveTimestamps();
    }
#else
    draw();
    retrieveTimestamps();
#endif

    context->device->waitIdle();
}

void Renderer::stop() {
    // wait till device is idle
    running = false;

    context->device->waitIdle();
}

void Renderer::createCommandPool() {
    spdlog::debug("Creating command pool");
    vk::CommandPoolCreateInfo poolInfo = {};
    poolInfo.queueFamilyIndex = context->queues[VulkanContext::Queue::COMPUTE].queueFamily;
    poolInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer;

    commandPool = context->device->createCommandPoolUnique(poolInfo, nullptr);
}

void Renderer::recordPreprocessCommandBuffer() {
    spdlog::debug("Recording preprocess command buffer");
    if (!preprocessCommandBuffer) {
        vk::CommandBufferAllocateInfo allocateInfo = {commandPool.get(), vk::CommandBufferLevel::ePrimary, 1};
        auto buffers = context->device->allocateCommandBuffersUnique(allocateInfo);
        preprocessCommandBuffer = std::move(buffers[0]);
    }
    preprocessCommandBuffer->reset();

    auto numGroups = sizing::workgroupCount(scene->getNumVertices());

    preprocessCommandBuffer->begin(vk::CommandBufferBeginInfo{});

    resetTimestampQueries(preprocessCommandBuffer.get());

    preprocessPipeline->bind(preprocessCommandBuffer, 0, 0);
    writeTimestamp(preprocessCommandBuffer.get(), "preprocess_start");
    preprocessCommandBuffer->dispatch(numGroups, 1, 1);
    // Compute writes the overlap counts; the copy below reads them via transfer.
    tileOverlapBuffer->computeToTransferReadBarrier(preprocessCommandBuffer.get());

    vk::BufferCopy copyRegion = {0, 0, tileOverlapBuffer->size};
    preprocessCommandBuffer->copyBuffer(tileOverlapBuffer->buffer, prefixSumPingBuffer->buffer, 1, &copyRegion);

    // The transfer wrote the ping buffer; the prefix-sum compute pass reads it.
    prefixSumPingBuffer->transferToComputeReadBarrier(preprocessCommandBuffer.get());

    writeTimestamp(preprocessCommandBuffer.get(), "preprocess_end");

    prefixSumPipeline->bind(preprocessCommandBuffer, 0, 0);
    writeTimestamp(preprocessCommandBuffer.get(), "prefix_sum_start");
    const auto iters = sizing::prefixSumIterations(scene->getNumVertices());
    for (uint32_t timestep = 0; timestep <= iters; timestep++) {
        preprocessCommandBuffer->pushConstants(prefixSumPipeline->pipelineLayout.get(),
                                               vk::ShaderStageFlagBits::eCompute, 0, sizeof(uint32_t), &timestep);
        preprocessCommandBuffer->dispatch(numGroups, 1, 1);

        if (timestep % 2 == 0) {
            prefixSumPongBuffer->computeWriteReadBarrier(preprocessCommandBuffer.get());
            prefixSumPingBuffer->computeReadWriteBarrier(preprocessCommandBuffer.get());
        } else {
            prefixSumPingBuffer->computeWriteReadBarrier(preprocessCommandBuffer.get());
            prefixSumPongBuffer->computeReadWriteBarrier(preprocessCommandBuffer.get());
        }
    }

    auto totalSumRegion =
        vk::BufferCopy{sizing::bytesFor(scene->getNumVertices() - 1, sizeof(uint32_t), "Prefix sum final offset"), 0,
                       sizeof(uint32_t)};
    // Ensure the final prefix-sum compute write is visible to the transfer read.
    auto& finalPrefixSumBuffer = (iters % 2 == 0) ? prefixSumPingBuffer : prefixSumPongBuffer;
    finalPrefixSumBuffer->computeToTransferReadBarrier(preprocessCommandBuffer.get());
    preprocessCommandBuffer->copyBuffer(finalPrefixSumBuffer->buffer, totalSumBufferHost->buffer, 1, &totalSumRegion);

    writeTimestamp(preprocessCommandBuffer.get(), "prefix_sum_end");

    preprocessCommandBuffer->end();
}

bool Renderer::recordRenderCommandBuffer(uint32_t currentFrame) {
    if (!renderCommandBuffer) {
        renderCommandBuffer = std::move(context->device->allocateCommandBuffersUnique(
            vk::CommandBufferAllocateInfo(commandPool.get(), vk::CommandBufferLevel::ePrimary, 1))[0]);
    }

    uint32_t numInstances = totalSumBufferHost->readOne<uint32_t>();
    guiManager.pushTextMetric("instances", numInstances);
    auto capacity = sizing::sortCapacity(scene->getNumVertices(), sortBufferSizeMultiplier);
    if (numInstances > capacity) {
        auto old = sortBufferSizeMultiplier;
        while (numInstances > capacity) {
            sortBufferSizeMultiplier++;
            capacity = sizing::sortCapacity(scene->getNumVertices(), sortBufferSizeMultiplier);
        }
        spdlog::info("Reallocating sort buffers. {} -> {}", old, sortBufferSizeMultiplier);
        sortKBufferEven->realloc(sizing::bytesFor(capacity, sizeof(uint64_t), "Even sort key buffer size"));
        sortKBufferOdd->realloc(sizing::bytesFor(capacity, sizeof(uint64_t), "Odd sort key buffer size"));
        sortVBufferEven->realloc(sizing::bytesFor(capacity, sizeof(uint32_t), "Even sort payload buffer size"));
        sortVBufferOdd->realloc(sizing::bytesFor(capacity, sizeof(uint32_t), "Odd sort payload buffer size"));

        auto numWorkgroups = sizing::radixSortWorkgroupCount(capacity, numRadixSortBlocksPerWorkgroup);

        sortHistBuffer->realloc(sizing::sortHistogramBytes(numWorkgroups));

        recordPreprocessCommandBuffer();
        return false;
    }

    renderCommandBuffer->reset({});
    renderCommandBuffer->begin(vk::CommandBufferBeginInfo{});

    vertexAttributeBuffer->computeWriteReadBarrier(renderCommandBuffer.get());

    const auto iters = sizing::prefixSumIterations(scene->getNumVertices());
    auto numGroups = sizing::workgroupCount(scene->getNumVertices());
    preprocessSortPipeline->bind(renderCommandBuffer, 0, iters % 2 == 0 ? 0 : 1);
    writeTimestamp(renderCommandBuffer.get(), "preprocess_sort_start");
    uint32_t tileX = sizing::tileCountX(getRenderExtent().width);
    renderCommandBuffer->pushConstants(preprocessSortPipeline->pipelineLayout.get(), vk::ShaderStageFlagBits::eCompute,
                                       0, sizeof(uint32_t), &tileX);
    renderCommandBuffer->dispatch(numGroups, 1, 1);

    sortKBufferEven->computeWriteReadBarrier(renderCommandBuffer.get());
    writeTimestamp(renderCommandBuffer.get(), "preprocess_sort_end");

    assert(numInstances <= capacity);
    writeTimestamp(renderCommandBuffer.get(), "sort_start");
    for (auto i = 0; i < 8; i++) {
        sortHistPipeline->bind(renderCommandBuffer, 0, i % 2 == 0 ? 0 : 1);
        auto invocationSize = sizing::radixSortWorkgroupCount(numInstances, numRadixSortBlocksPerWorkgroup);

        vkgs::render::RadixSortPushConstants pushConstants{};
        pushConstants.numElements = numInstances;
        pushConstants.numBlocksPerWorkgroup = numRadixSortBlocksPerWorkgroup;
        pushConstants.shift = i * 8;
        pushConstants.numWorkgroups = invocationSize;
        renderCommandBuffer->pushConstants(sortHistPipeline->pipelineLayout.get(), vk::ShaderStageFlagBits::eCompute, 0,
                                            sizeof(pushConstants), &pushConstants);

        renderCommandBuffer->dispatch(invocationSize, 1, 1);

        sortHistBuffer->computeWriteReadBarrier(renderCommandBuffer.get());

        sortPipeline->bind(renderCommandBuffer, 0, i % 2 == 0 ? 0 : 1);
        renderCommandBuffer->pushConstants(sortPipeline->pipelineLayout.get(), vk::ShaderStageFlagBits::eCompute, 0,
                                            sizeof(pushConstants), &pushConstants);
        renderCommandBuffer->dispatch(invocationSize, 1, 1);

        if (i % 2 == 0) {
            sortKBufferOdd->computeWriteReadBarrier(renderCommandBuffer.get());
            sortVBufferOdd->computeWriteReadBarrier(renderCommandBuffer.get());
        } else {
            sortKBufferEven->computeWriteReadBarrier(renderCommandBuffer.get());
            sortVBufferEven->computeWriteReadBarrier(renderCommandBuffer.get());
        }
    }
    writeTimestamp(renderCommandBuffer.get(), "sort_end");

    renderCommandBuffer->fillBuffer(tileBoundaryBuffer->buffer, 0, VK_WHOLE_SIZE, 0);

    vkgs::vulkan::BarrierBuilder()
        .queueFamilyIndex(context->queues[VulkanContext::Queue::COMPUTE].queueFamily)
        .addBufferBarrier(tileBoundaryBuffer, vk::AccessFlagBits::eTransferWrite, vk::AccessFlagBits::eShaderWrite)
        .build(renderCommandBuffer.get(), vk::PipelineStageFlagBits::eTransfer,
               vk::PipelineStageFlagBits::eComputeShader);

    // Since we have 64 bit keys, the sort result is always in the even buffer
    tileBoundaryPipeline->bind(renderCommandBuffer, 0, 0);
    writeTimestamp(renderCommandBuffer.get(), "tile_boundary_start");
    renderCommandBuffer->pushConstants(tileBoundaryPipeline->pipelineLayout.get(), vk::ShaderStageFlagBits::eCompute, 0,
                                       sizeof(uint32_t), &numInstances);
    renderCommandBuffer->dispatch(sizing::workgroupCount(numInstances), 1, 1);

    tileBoundaryBuffer->computeWriteReadBarrier(renderCommandBuffer.get());
    writeTimestamp(renderCommandBuffer.get(), "tile_boundary_end");

    renderPipeline->bind(renderCommandBuffer, 0, std::vector<uint32_t>{0, currentImageIndex});
    writeTimestamp(renderCommandBuffer.get(), "render_start");
    auto [width, height] = getRenderExtent();
    uint32_t constants[2] = {width, height};
    renderCommandBuffer->pushConstants(renderPipeline->pipelineLayout.get(), vk::ShaderStageFlagBits::eCompute, 0,
                                       sizeof(uint32_t) * 2, constants);

    // image layout transition: render target -> general
    vk::ImageMemoryBarrier imageMemoryBarrier{};
#ifdef VKGS_RENDER_MODE_ONSCREEN
    imageMemoryBarrier.oldLayout = vk::ImageLayout::eUndefined;
#else
    imageMemoryBarrier.oldLayout = vk::ImageLayout::eGeneral;
#endif
    imageMemoryBarrier.newLayout = vk::ImageLayout::eGeneral;
    imageMemoryBarrier.image = getCurrentRenderImage().image;
    imageMemoryBarrier.subresourceRange = {vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1};
    imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eNoneKHR;
    imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eShaderWrite;
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    renderCommandBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe,
                                         vk::PipelineStageFlagBits::eComputeShader, vk::DependencyFlagBits::eByRegion,
                                         nullptr, nullptr, imageMemoryBarrier);

    renderCommandBuffer->dispatch(sizing::tileCountX(width), sizing::tileCountY(height), 1);

#ifdef VKGS_RENDER_MODE_ONSCREEN
    // image layout transition: general -> present
    imageMemoryBarrier.oldLayout = vk::ImageLayout::eGeneral;
    imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
    imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    if (configuration.enableGui) {
        imageMemoryBarrier.newLayout = vk::ImageLayout::eColorAttachmentOptimal;
        imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
        renderCommandBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                                             vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                             vk::DependencyFlagBits::eByRegion, nullptr, nullptr, imageMemoryBarrier);
    } else {
        imageMemoryBarrier.newLayout = vk::ImageLayout::ePresentSrcKHR;
        imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eMemoryRead;
        renderCommandBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                                             vk::PipelineStageFlagBits::eBottomOfPipe,
                                             vk::DependencyFlagBits::eByRegion, nullptr, nullptr, imageMemoryBarrier);
    }
#endif
    writeTimestamp(renderCommandBuffer.get(), "render_end");

#ifdef VKGS_RENDER_MODE_ONSCREEN
    if (configuration.enableGui) {
        guiManager.cameraPosition = camera.controller.position;
        guiManager.cameraRotation = camera.controller.rotation;
        imguiManager->draw(renderCommandBuffer.get(), currentImageIndex, std::bind(&GUIManager::buildGui, &guiManager));

        processGuiCameraRequests();

        imageMemoryBarrier.oldLayout = vk::ImageLayout::eColorAttachmentOptimal;
        imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;

        imageMemoryBarrier.newLayout = vk::ImageLayout::ePresentSrcKHR;
        imageMemoryBarrier.dstAccessMask = vk::AccessFlagBits::eMemoryRead;

        renderCommandBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eColorAttachmentOutput,
                                             vk::PipelineStageFlagBits::eComputeShader,
                                             vk::DependencyFlagBits::eByRegion, nullptr, nullptr, imageMemoryBarrier);
    }
#endif
    renderCommandBuffer->end();

    return true;
}

void Renderer::updateUniforms() {
    vkgs::render::UniformBuffer data{};
    auto [width, height] = getRenderExtent();
    data.width = width;
    data.height = height;
    data.cameraPosition = glm::vec4(camera.controller.position, 1.0f);

    auto rotation = glm::mat4_cast(camera.controller.rotation);
    auto translation = glm::translate(glm::mat4(1.0f), camera.controller.position);
    auto view = glm::inverse(translation * rotation);

    float tan_fovx = std::tan(glm::radians(camera.fov) / 2.0);
    float tan_fovy = tan_fovx * static_cast<float>(height) / static_cast<float>(width);
    data.view = view;
    data.projection = glm::perspective(std::atan(tan_fovy) * 2.0f,
                                       static_cast<float>(width) / static_cast<float>(height), camera.nearPlane,
                                       camera.farPlane) *
                      view;

    data.view[0][1] *= -1.0f;
    data.view[1][1] *= -1.0f;
    data.view[2][1] *= -1.0f;
    data.view[3][1] *= -1.0f;
    data.view[0][2] *= -1.0f;
    data.view[1][2] *= -1.0f;
    data.view[2][2] *= -1.0f;
    data.view[3][2] *= -1.0f;

    data.projection[0][1] *= -1.0f;
    data.projection[1][1] *= -1.0f;
    data.projection[2][1] *= -1.0f;
    data.projection[3][1] *= -1.0f;
    data.tanFovX = tan_fovx;
    data.tanFovY = tan_fovy;
    data.nearPlane = camera.nearPlane;
    uniformBuffer->uploadObject(data);
}

Renderer::~Renderer() {}
