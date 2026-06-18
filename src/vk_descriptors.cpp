#include <vk_descriptors.h>

void DescriptorLayoutBuilder::add_binding(uint32_t binding, VkDescriptorType type)
{
    //Create Descriptor Set Layout Binding, and add it to bindings vector
    VkDescriptorSetLayoutBinding newBinding = {};
    newBinding.binding = binding;
    newBinding.descriptorType = type;
    newBinding.descriptorCount = 1;

    bindings.push_back(newBinding);
}

void DescriptorLayoutBuilder::clear()
{
    bindings.clear();
}

VkDescriptorSetLayout DescriptorLayoutBuilder::build_Layout(VkDevice device, VkShaderStageFlags shaderStageFlags, void *pNext, VkDescriptorSetLayoutCreateFlags flags)
{
    //Add the shader stage flags to all bindings
    for(auto& binding : bindings)
    {
        binding.stageFlags |= shaderStageFlags;
    }

    //Create VkDescriptorLayoutCreateInfo struct and use the VkDevice to create the Desc Layout
    VkDescriptorSetLayoutCreateInfo descriptorLayoutCreateInfo = {};
    descriptorLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorLayoutCreateInfo.pNext = pNext;
    descriptorLayoutCreateInfo.flags = flags;
    descriptorLayoutCreateInfo.bindingCount = (uint32_t)bindings.size();
    descriptorLayoutCreateInfo.pBindings = bindings.data();

    VkDescriptorSetLayout descriptorSetLayout;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &descriptorLayoutCreateInfo, nullptr, &descriptorSetLayout));

    return descriptorSetLayout;
}

void DescriptorAllocator::init_pool(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolSizeRatios)
{
    //Create vector of VkPoolSize from PoolSizeRatios span
    std::vector<VkDescriptorPoolSize> poolSizes;
    for(const PoolSizeRatio& ratio : poolSizeRatios)
    {
        VkDescriptorPoolSize poolSize = {};
        poolSize.type = ratio.type;
        poolSize.descriptorCount = ratio.ratio * maxSets;

        poolSizes.push_back(poolSize);
    }

    //Use VkPool Create Info to create Descriptor Set Allocation Pool
    VkDescriptorPoolCreateInfo poolCreateInfo = {};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCreateInfo.pNext = nullptr;
    poolCreateInfo.flags = 0;
    poolCreateInfo.maxSets = maxSets;
    poolCreateInfo.poolSizeCount = (uint32_t)poolSizes.size();
    poolCreateInfo.pPoolSizes = poolSizes.data();

    vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, &pool);
}

void DescriptorAllocator::reset_pool(VkDevice device)
{
    vkResetDescriptorPool(device, pool, 0);
}

void DescriptorAllocator::destroy_pool(VkDevice device)
{
    vkDestroyDescriptorPool(device, pool, nullptr);
}

VkDescriptorSet DescriptorAllocator::allocate(VkDevice device, VkDescriptorSetLayout layout)
{
    //Use Descriptor Allocator create info to create a new descriptor set
    VkDescriptorSetAllocateInfo allocatorInfo = {};
    allocatorInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocatorInfo.pNext = nullptr;
    allocatorInfo.descriptorSetCount = 1;
    allocatorInfo.descriptorPool = pool;
    allocatorInfo.pSetLayouts = &layout;

    VkDescriptorSet ds;

    vkAllocateDescriptorSets(device, &allocatorInfo, &ds);

    return ds;
}
