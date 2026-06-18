#pragma once

#include <vk_types.h>

struct DescriptorLayoutBuilder
{
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    void add_binding(uint32_t binding, VkDescriptorType type);

    void clear();

    VkDescriptorSetLayout build_Layout(VkDevice device, VkShaderStageFlags shaderStageFlags, void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);
}
