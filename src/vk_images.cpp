#include <vk_images.h>
#include "vk_initializers.h"

namespace vkutil
{
	void transition_Image(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout)
	{
		VkImageMemoryBarrier2 image_Barrier = { .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
		image_Barrier.pNext = nullptr;
		image_Barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		image_Barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
		image_Barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
		image_Barrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;
		image_Barrier.oldLayout = currentLayout;
		image_Barrier.newLayout = newLayout;

		image_Barrier.image = image;

		VkImageAspectFlags image_Aspect_Flag = (newLayout == VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
		VkImageSubresourceRange image_Subresource_Range = vkinit::image_subresource_range(image_Aspect_Flag);

		VkDependencyInfo dep_Info{ .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
		dep_Info.pNext = nullptr;
		dep_Info.imageMemoryBarrierCount = 1;
		dep_Info.pImageMemoryBarriers = &image_Barrier;

		vkCmdPipelineBarrier2(cmd, &dep_Info);
	}
}