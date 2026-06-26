#pragma once

#include <vk_types.h>


namespace draw_functions
{
    void draw_Clear_Background(VkCommandBuffer cmd, VkClearColorValue clearColor, VkImage drawImage);

    void draw_BackgroundEffects(VkCommandBuffer cmd, ComputeEffect& computeEffect, VkPipelineLayout pipelineLayout, VkDescriptorSet drawImageDescriptor, VkExtent2D imageExtents);
}