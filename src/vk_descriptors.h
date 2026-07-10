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

struct DescriptorAllocatorGrowable
{
    void init(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolSizeRatios);

    void resetPools(VkDevice device);

    void destroyPools(VkDevice device);

    VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout);

private:
    VkDescriptorPool getPool(VkDevice device);
    VkDescriptorPool createPool(VkDevice device);

    std::vector<PoolSizeRatio> ratios;
    std::deque<VkDescriptorPool> fullPools;
    std::deque<VkDescriptorPool> readyPools;

    uint32_t maxSetsCount;

    uint32_t maxSetsCountLimit = 4092;
};

struct DescriptorSetWriter
{
    void writeBuffer(uint32_t binding, VkBuffer buffer, uint32_t size, uint32_t offset, VkDescriptorType type);
    void writeImage(uint32_t binding, VkImageView image, VkSampler sampler, VkImageLayout layout, VkDescriptorType type);
    void clear();
    void updateSet(VkDevice device, VkDescriptorSet set);

private:
    std::deque<VkDescriptorBufferInfo> buffersInfo;
    std::deque<VkDescriptorImageInfo> imagesInfo;
    std::vector<VkWriteDescriptorSet> writes;
};