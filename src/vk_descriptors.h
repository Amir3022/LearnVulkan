#pragma once

#include <vk_types.h>

struct PoolSizeRatio
{
    VkDescriptorType type;
    float ratio;
};
struct DescriptorLayoutBuilder
{
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    void add_binding(uint32_t binding, VkDescriptorType type);

    void clear();

    VkDescriptorSetLayout build_Layout(VkDevice device, VkShaderStageFlags shaderStageFlags, void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);
};


struct DescriptorAllocator
{
    VkDescriptorPool pool;

    void init_pool(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolSizeRatios);

    void reset_pool(VkDevice device);

    void destroy_pool(VkDevice device);

    VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout);
};