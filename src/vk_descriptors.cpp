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
        poolSize.descriptorCount = (uint32_t)(ratio.ratio * maxSets);

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

void DescriptorAllocatorGrowable::init(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolSizeRatios)
{
    //Clear ratios vector, and add poolSizeRatios to the local variable
    ratios.clear();
    for(auto p : poolSizeRatios)
    {    
        ratios.push_back(p);
    }
    
    //Set the maxSetsCount variable from maxSets property
    maxSetsCount = maxSets;

    //Create initial pool and add it to ready pools
    VkDescriptorPool newPool;
    newPool = createPool(device);

    readyPools.push_back(newPool);
}

void DescriptorAllocatorGrowable::resetPools(VkDevice device)
{
    for(VkDescriptorPool pool : readyPools)
    {
        vkResetDescriptorPool(device, pool, 0);
    }
    for(VkDescriptorPool pool : fullPools)
    {
        vkResetDescriptorPool(device, pool, 0);
        readyPools.push_back(pool); //Add all reset full pools to ready pools
    }
    fullPools.clear();
}

void DescriptorAllocatorGrowable::destroyPools(VkDevice device)
{
    for(VkDescriptorPool pool : readyPools)
    {
        vkDestroyDescriptorPool(device, pool, nullptr);
    }
    readyPools.clear();
    for(VkDescriptorPool pool : fullPools)
    {
        vkDestroyDescriptorPool(device, pool, nullptr);
    }
    fullPools.clear();
}

VkDescriptorSet DescriptorAllocatorGrowable::allocate(VkDevice device, VkDescriptorSetLayout layout)
{
    //Get pool from ready pools using getPool funciton
    VkDescriptorPool poolToUse = getPool(device);

    //Use Descriptor Allocator create info to create a new descriptor set
    VkDescriptorSetAllocateInfo allocatorInfo = {};
    allocatorInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocatorInfo.pNext = nullptr;
    allocatorInfo.descriptorSetCount = 1;
    allocatorInfo.descriptorPool = poolToUse;
    allocatorInfo.pSetLayouts = &layout;

    //Try to allocate set from pool
    VkDescriptorSet ds;
    VkResult allocationResult = vkAllocateDescriptorSets(device, &allocatorInfo, &ds);

    if(allocationResult == VK_ERROR_OUT_OF_POOL_MEMORY || allocationResult == VK_ERROR_FRAGMENTED_POOL)  //if allocation failed due to full pool, add the pool to full pools and get a new pool
    {
        fullPools.push_back(poolToUse);
        poolToUse = getPool(device);
        allocatorInfo.descriptorPool = poolToUse;   //Update the used pool
        VK_CHECK(vkAllocateDescriptorSets(device, &allocatorInfo, &ds));    //If failed again, should crash the program, since it shouldn't fail twice in succession
    }

    //Add the gotten pool to ready pool
    readyPools.push_back(poolToUse);

    return ds;
}

VkDescriptorPool DescriptorAllocatorGrowable::getPool(VkDevice device)
{
    VkDescriptorPool poolToGet;
    //Check if the readyPools isn't empty pop the back pool and return it
    if(readyPools.size() != 0)
    {
        poolToGet = readyPools.back();
        readyPools.pop_back();
    }
    else    //If the readyPools doesn't have any pools we create a new pool and increase the maxSetsCount
    {
        poolToGet = createPool(device);

        maxSetsCount = (uint32_t)std::ceil(maxSetsCount * 1.5f);
        if(maxSetsCount > maxSetsCountLimit)
        {
            maxSetsCount = maxSetsCountLimit;
        }
    }
    return poolToGet;
}

VkDescriptorPool DescriptorAllocatorGrowable::createPool(VkDevice device)
{
     //Create vector of VkPoolSize from PoolSizeRatios span
    std::vector<VkDescriptorPoolSize> poolSizes;
    for(const PoolSizeRatio& ratio : ratios)
    {
        VkDescriptorPoolSize poolSize = {};
        poolSize.type = ratio.type;
        poolSize.descriptorCount = (uint32_t)(ratio.ratio * maxSetsCount);
        poolSizes.push_back(poolSize);
    }

    //Use VkPool Create Info to create Descriptor Set Allocation Pool
    VkDescriptorPoolCreateInfo poolCreateInfo = {};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolCreateInfo.pNext = nullptr;
    poolCreateInfo.flags = 0;
    poolCreateInfo.maxSets = maxSetsCount;
    poolCreateInfo.poolSizeCount = (uint32_t)poolSizes.size();
    poolCreateInfo.pPoolSizes = poolSizes.data();

    VkDescriptorPool newPool;
    VkResult creationResult = vkCreateDescriptorPool(device, &poolCreateInfo, nullptr, &newPool);
    if(creationResult != VK_SUCCESS)
        return VK_NULL_HANDLE;

    return newPool;
}

void DescriptorSetWriter::writeBuffer(uint32_t binding, VkBuffer buffer, uint32_t size, uint32_t offset, VkDescriptorType type)
{
    //Create Descriptor Buffer info and add it to buffer infos
    VkDescriptorBufferInfo& bufferInfo = buffersInfo.emplace_back(VkDescriptorBufferInfo{
        .buffer = buffer,
        .offset = offset,
        .range = size,
    });

    //Create Write Descriptor set and fill with proper data and add to array
    VkWriteDescriptorSet writeDescriptorSet = {};
    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.pNext = nullptr;
    writeDescriptorSet.dstBinding = binding;
    writeDescriptorSet.dstSet = VK_NULL_HANDLE; //To be updated when updating descriptor set with writers
    writeDescriptorSet.descriptorCount = 1;
    writeDescriptorSet.descriptorType = type;
    writeDescriptorSet.pBufferInfo = &bufferInfo;

    writes.push_back(writeDescriptorSet);
}

void DescriptorSetWriter::writeImage(uint32_t binding, VkImageView image, VkSampler sampler, VkImageLayout layout, VkDescriptorType type)
{
    //Create Descriptor Image Info and add it to images info
    VkDescriptorImageInfo& imageInfo = imagesInfo.emplace_back(VkDescriptorImageInfo{
        .sampler = sampler,
        .imageView = image,
        .imageLayout = layout,
    });

    //Create Write Descriptor set and fill with proper data and add to array
    VkWriteDescriptorSet writeDescriptorSet = {};
    writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeDescriptorSet.pNext = nullptr;
    writeDescriptorSet.dstBinding = binding;
    writeDescriptorSet.dstSet = VK_NULL_HANDLE; //To be updated when updating descriptor set with writers
    writeDescriptorSet.descriptorCount = 1;
    writeDescriptorSet.descriptorType = type;
    writeDescriptorSet.pImageInfo = &imageInfo;

    writes.push_back(writeDescriptorSet);
}

void DescriptorSetWriter::clear()
{
    buffersInfo.clear();
    imagesInfo.clear();
    writes.clear();
}

void DescriptorSetWriter::updateSet(VkDevice device, VkDescriptorSet set)
{
    //Set the descriptor Set as the set for each write descriptor set struct
    for(auto& w : writes)
    {
        w.dstSet = set;
    }

    //Update the descriptor set with writes 
    vkUpdateDescriptorSets(device, (uint32_t)writes.size(), writes.data(), 0, nullptr);
}
