#pragma once 
#include <vk_types.h>
#include <string>

namespace vkutil {
    bool load_Shader_Module(const std::string& filePath, VkDevice device, VkShaderModule* outShaderModule);
};