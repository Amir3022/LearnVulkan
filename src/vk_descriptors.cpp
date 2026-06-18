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
