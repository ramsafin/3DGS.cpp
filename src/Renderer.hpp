#pragma once

#define GLM_FORCE_SWIZZLE

#include "CameraController.hpp"
#include "GpuConstants.hpp"
#include "render/RendererConfiguration.hpp"

#include "scene/GpuScene.hpp"
#include <3dgs/Types.hpp> // CameraPose

#include "vulkan/Window.hpp"
#include "vulkan/pipelines/ComputePipeline.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>

#ifdef VKGS_RENDER_MODE_ONSCREEN
#include "GUIManager.hpp"
#include "vulkan/ImguiManager.hpp"
#include "vulkan/Swapchain.hpp"
#else
#include "vulkan/OffscreenRenderTarget.hpp"
#endif
#include "vulkan/QueryManager.hpp"

#include <glm/gtc/quaternion.hpp>

class Renderer {
  public:
    struct Camera final {
        CameraController controller;
        float fov = 45.0f;
        float nearPlane = 0.1f;
        float farPlane = 1000.0f;
    };

    explicit Renderer(vkgs::render::RendererConfiguration configuration);

    void createGui();

    void initialize();

    void handleInput();

    void retrieveTimestamps();

    void recreateSwapchain();

    void draw();

    std::vector<uint8_t> readPixels() const;

    void setCameraPose(const vkgs::CameraPose& pose);

    void setCameraProjection(float fovDegrees, float nearPlane, float farPlane);

    void run();

    void stop();

    ~Renderer();

    Camera camera{};

  private:
    vkgs::render::RendererConfiguration configuration;
    std::shared_ptr<Window> window;
    std::shared_ptr<VulkanContext> context;
#ifdef VKGS_RENDER_MODE_ONSCREEN
    std::shared_ptr<ImguiManager> imguiManager;
    GUIManager guiManager;
#endif
    std::unique_ptr<vkgs::scene::GpuScene> scene;
    QueryManager queryManager;

    std::shared_ptr<ComputePipeline> preprocessPipeline;
    std::shared_ptr<ComputePipeline> renderPipeline;
    std::shared_ptr<ComputePipeline> prefixSumPipeline;
    std::shared_ptr<ComputePipeline> preprocessSortPipeline;
    std::shared_ptr<ComputePipeline> sortHistPipeline;
    std::shared_ptr<ComputePipeline> sortPipeline;
    std::shared_ptr<ComputePipeline> tileBoundaryPipeline;

    std::shared_ptr<Buffer> uniformBuffer;
    std::shared_ptr<Buffer> vertexAttributeBuffer;
    std::shared_ptr<Buffer> tileOverlapBuffer;
    std::shared_ptr<Buffer> prefixSumPingBuffer;
    std::shared_ptr<Buffer> prefixSumPongBuffer;
    std::shared_ptr<Buffer> sortKBufferEven;
    std::shared_ptr<Buffer> sortKBufferOdd;
    std::shared_ptr<Buffer> sortDepthKBufferEven;
    std::shared_ptr<Buffer> sortDepthKBufferOdd;
    std::shared_ptr<Buffer> sortHistBuffer;
    std::shared_ptr<Buffer> totalSumBufferHost;
    std::shared_ptr<Buffer> tileBoundaryBuffer;
    std::shared_ptr<Buffer> sortVBufferEven;
    std::shared_ptr<Buffer> sortVBufferOdd;

    std::shared_ptr<DescriptorSet> inputSet;

    std::atomic<bool> running = true;

    std::vector<vk::UniqueFence> inflightFences;

#ifdef VKGS_RENDER_MODE_ONSCREEN
    std::shared_ptr<Swapchain> swapchain;
    std::vector<vk::UniqueSemaphore> renderFinishedSemaphores;
#else
    std::shared_ptr<OffscreenRenderTarget> offscreenRenderTarget;
#endif

    vk::UniqueCommandPool commandPool;

    vk::UniqueCommandBuffer preprocessCommandBuffer;
    vk::UniqueCommandBuffer renderCommandBuffer;

    uint32_t currentImageIndex = 0;
    uint32_t numRadixSortBlocksPerWorkgroup = gpu::RadixBlocksPerWorkgroup;

    int fpsCounter = 0;
    std::chrono::high_resolution_clock::time_point lastFpsTime = std::chrono::high_resolution_clock::now();

    uint32_t sortBufferSizeMultiplier = 1;

    void initializeVulkan();

    void loadSceneToGPU();

    void createPreprocessPipeline();

    void createPrefixSumPipeline();

    void createRadixSortPipeline();

    void createPreprocessSortPipeline();

    void createTileBoundaryPipeline();

    void createRenderPipeline();

    void recordPreprocessCommandBuffer();

    bool recordRenderCommandBuffer(uint32_t currentFrame);

    void createCommandPool();

    void updateUniforms();

    void processGuiCameraRequests();

    void resetTimestampQueries(vk::CommandBuffer commandBuffer);
    void writeTimestamp(vk::CommandBuffer commandBuffer, const std::string& name);

    static void validateCameraProjection(float fovDegrees, float nearPlane, float farPlane);

    vk::Extent2D getRenderExtent() const;
    std::span<const vkgs::vulkan::RenderImageView> getRenderImages() const;
    const vkgs::vulkan::RenderImageView& getCurrentRenderImage() const;
};
