#include "Shader.h"

#include "core/FileIO.h"

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

void Shader::load() {
    vk::ShaderModuleCreateInfo create_info{};
    std::vector<uint32_t> ownedWords;
    if (data == nullptr) {
        auto fn = "shaders/" + filename + ".spv";
        auto shader_code = vkgs::core::readBinaryFile(fn);
        if (shader_code.empty()) {
            throw std::runtime_error("Failed to load shader: " + fn);
        }
        if (shader_code.size() % sizeof(uint32_t) != 0) {
            throw std::runtime_error("SPIR-V byte size is not divisible by 4: " + fn);
        }
        ownedWords.resize(shader_code.size() / sizeof(uint32_t));
        std::memcpy(ownedWords.data(), shader_code.data(), shader_code.size());
        create_info.codeSize = shader_code.size();
        create_info.pCode = ownedWords.data();
    } else {
        if (size == 0 || size % sizeof(uint32_t) != 0) {
            throw std::runtime_error("Embedded SPIR-V byte size must be a non-zero multiple of 4");
        }
        if (reinterpret_cast<uintptr_t>(data) % alignof(uint32_t) != 0) {
            throw std::runtime_error("Embedded SPIR-V data must be aligned to 4 bytes");
        }
        create_info.codeSize = size;
        create_info.pCode = reinterpret_cast<const uint32_t*>(data);
    }
    shader = context->device->createShaderModuleUnique(create_info);

    if (context->validationLayersEnabled) {
        context->device->setDebugUtilsObjectNameEXT(vk::DebugUtilsObjectNameInfoEXT{
            vk::ObjectType::eShaderModule, reinterpret_cast<uint64_t>(static_cast<VkShaderModule>(shader.get())),
            filename.c_str()});
    }
}
