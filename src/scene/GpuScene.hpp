#pragma once

#include "GaussianSceneData.hpp"
#include "vulkan/Buffer.hpp"

#include <memory>

class VulkanContext;

namespace vkgs::scene {

class GpuScene {
  public:
    explicit GpuScene(GaussianSceneData sceneData);

    void upload(const std::shared_ptr<VulkanContext>& context);

    [[nodiscard]] uint64_t getNumVertices() const;
    [[nodiscard]] const SceneBounds& getBounds() const;

    std::shared_ptr<Buffer> vertexBuffer;
    std::shared_ptr<Buffer> cov3DBuffer;

  private:
    GaussianSceneData sceneData;

    static std::shared_ptr<Buffer> createBuffer(const std::shared_ptr<VulkanContext>& context, vk::DeviceSize size);
    void precomputeCov3D(const std::shared_ptr<VulkanContext>& context);
};

} // namespace vkgs::scene
