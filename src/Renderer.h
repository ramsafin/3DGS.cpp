#ifndef RENDERER_H
#define RENDERER_H

#define GLM_FORCE_SWIZZLE

#include "GSScene.h"
#include "vulkan/Window.h"
#include "vulkan/pipelines/ComputePipeline.h"

#include "GpuConstants.h"

#include <3dgs/3dgs.h>
#include <atomic>
#include <cstddef>
#ifdef VKGS_RENDER_MODE_ONSCREEN
#include "vulkan/ImguiManager.h"
#include "vulkan/Swapchain.h"
#else
#include "vulkan/OffscreenRenderTarget.h"
#endif
#include "GUIManager.h"
#include "vulkan/QueryManager.h"

#include <glm/gtc/quaternion.hpp>

class Renderer {
  public:
    struct alignas(16) UniformBuffer {
        glm::vec4 camera_position;
        glm::mat4 proj_mat;
        glm::mat4 view_mat;
        uint32_t width;
        uint32_t height;
        float tan_fovx;
        float tan_fovy;
        float near_plane;
    };

    struct VertexAttributeBuffer {
        glm::vec4 conic_opacity;
        glm::vec4 color_radii;
        glm::uvec4 aabb;
        glm::vec2 uv;
        float depth;
        uint32_t __padding[1];
    };

    struct Camera {
        glm::vec3 position;
        glm::quat rotation;
        float fov;
        float nearPlane;
        float farPlane;

        void translate(glm::vec3 translation) {
            position += rotation * translation;
        }
    };

    struct RadixSortPushConstants {
        uint32_t g_num_elements;             // == NUM_ELEMENTS
        uint32_t g_shift;                    // (*)
        uint32_t g_num_workgroups;           // == NUMBER_OF_WORKGROUPS as defined in the section above
        uint32_t g_num_blocks_per_workgroup; // == NUM_BLOCKS_PER_WORKGROUP
    };

    // CPU/GLSL ABI contracts (VKGS-011).
    // UniformBuffer mirrors src/shaders/preprocess.comp std140 Params block.
    static_assert(sizeof(UniformBuffer) == 176, "Renderer::UniformBuffer must be 176 bytes to match std140 layout");
    static_assert(offsetof(UniformBuffer, camera_position) == 0, "UniformBuffer::camera_position offset mismatch");
    static_assert(offsetof(UniformBuffer, proj_mat) == 16, "UniformBuffer::proj_mat offset mismatch");
    static_assert(offsetof(UniformBuffer, view_mat) == 80, "UniformBuffer::view_mat offset mismatch");
    static_assert(offsetof(UniformBuffer, width) == 144, "UniformBuffer::width offset mismatch");
    static_assert(offsetof(UniformBuffer, height) == 148, "UniformBuffer::height offset mismatch");
    static_assert(offsetof(UniformBuffer, tan_fovx) == 152, "UniformBuffer::tan_fovx offset mismatch");
    static_assert(offsetof(UniformBuffer, tan_fovy) == 156, "UniformBuffer::tan_fovy offset mismatch");
    static_assert(offsetof(UniformBuffer, near_plane) == 160, "UniformBuffer::near_plane offset mismatch");

    // VertexAttributeBuffer mirrors `struct VertexAttribute` in src/shaders/common.glsl:42-49.
    static_assert(sizeof(VertexAttributeBuffer) == 64, "Renderer::VertexAttributeBuffer must be 64 bytes");
    static_assert(offsetof(VertexAttributeBuffer, conic_opacity) == 0, "VertexAttributeBuffer::conic_opacity offset");
    static_assert(offsetof(VertexAttributeBuffer, color_radii) == 16, "VertexAttributeBuffer::color_radii offset");
    static_assert(offsetof(VertexAttributeBuffer, aabb) == 32, "VertexAttributeBuffer::aabb offset");
    static_assert(offsetof(VertexAttributeBuffer, uv) == 48, "VertexAttributeBuffer::uv offset");
    static_assert(offsetof(VertexAttributeBuffer, depth) == 56, "VertexAttributeBuffer::depth offset");
    static_assert(offsetof(VertexAttributeBuffer, __padding) == 60, "VertexAttributeBuffer magic marker offset");

    // RadixSortPushConstants mirrors src/shaders/sort/sort.comp:56-61.
    static_assert(sizeof(RadixSortPushConstants) == 16, "Renderer::RadixSortPushConstants must be 16 bytes");

    explicit Renderer(VulkanSplatting::RendererConfiguration configuration);

    void createGui();

    void initialize();

    void handleInput();

    void retrieveTimestamps();

    void recreateSwapchain();

    void draw();

    std::vector<uint8_t> readPixels();

    void setCameraPose(float px, float py, float pz, float qw, float qx, float qy, float qz);

    void setCameraProjection(float fovDegrees, float nearPlane, float farPlane);

    void run();

    void stop();

    ~Renderer();

    Camera camera{.position = glm::vec3(0.0f, 0.0f, 0.0f),
                  .rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f),
                  .fov = 45.0f,
                  .nearPlane = 0.1f,
                  .farPlane = 1000.0f};

  private:
    VulkanSplatting::RendererConfiguration configuration;
    std::shared_ptr<Window> window;
    std::shared_ptr<VulkanContext> context;
#ifdef VKGS_RENDER_MODE_ONSCREEN
    std::shared_ptr<ImguiManager> imguiManager;
#endif
    std::shared_ptr<GSScene> scene;
    std::shared_ptr<QueryManager> queryManager = std::make_shared<QueryManager>();
    GUIManager guiManager{};

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
#else
    std::shared_ptr<OffscreenRenderTarget> offscreenRenderTarget;
#endif

    vk::UniqueCommandPool commandPool;

    vk::UniqueCommandBuffer preprocessCommandBuffer;
    vk::UniqueCommandBuffer renderCommandBuffer;

    uint32_t currentImageIndex;

#ifdef VKGS_RENDER_MODE_ONSCREEN
    std::vector<vk::UniqueSemaphore> renderFinishedSemaphores;
#endif

    uint32_t numRadixSortBlocksPerWorkgroup = gpu::RadixBlocksPerWorkgroup;

    int fpsCounter = 0;
    std::chrono::high_resolution_clock::time_point lastFpsTime = std::chrono::high_resolution_clock::now();

    unsigned int sortBufferSizeMultiplier = 1;

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

    vk::Extent2D getRenderExtent() const;

    const std::vector<std::shared_ptr<Image>>& getRenderImages() const;

    std::shared_ptr<Image> getCurrentRenderImage() const;
};

#endif // RENDERER_H
