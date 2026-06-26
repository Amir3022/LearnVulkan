#include "draw_functions.h"
#include "vk_initializers.h"

namespace draw_functions
{
    void draw_Clear_Background(VkCommandBuffer cmd, VkClearColorValue clearColor, VkImage drawImage)
    {
        //Clear the screen with a clear Color
        VkImageSubresourceRange image_Subresource_Range = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);
        //Add the command for clearing the image got from the swapchain using the clear color generated
        vkCmdClearColorImage(cmd, drawImage, VK_IMAGE_LAYOUT_GENERAL, &clearColor, 1, &image_Subresource_Range);
    }

    void draw_BackgroundEffects(VkCommandBuffer cmd, ComputeEffect &computeEffect, VkPipelineLayout pipelineLayout, VkDescriptorSet drawImageDescriptor, VkExtent2D imageExtents)
    {
        //Bind the Pipeline to the draw command
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computeEffect.pipeline);
        //Bind the Descriptor Set to the draw Command
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &drawImageDescriptor, 0, nullptr);
        //Push Constants from selected compute effect constant data
        vkCmdPushConstants(cmd, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &computeEffect.pc_data);
        //Dispatch the Compute shader to start drawing on the Draw Image with group count to fill the screen
        vkCmdDispatch(cmd, (uint32_t)(imageExtents.width / 10), (uint32_t)(imageExtents.height / 10), (uint32_t)1);
    }
}