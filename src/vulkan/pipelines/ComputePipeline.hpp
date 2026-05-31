#pragma once

#include "Pipeline.hpp"
#include "vulkan/Shader.hpp"

#include <memory>

class ComputePipeline : public Pipeline {
  public:
    explicit ComputePipeline(const std::shared_ptr<VulkanContext>& context, std::shared_ptr<Shader> shader);
    ;

    void build() override;

  private:
    std::shared_ptr<Shader> shader;
};
