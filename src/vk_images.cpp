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

	void copy_Image_to_Image(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D sourceExtent, VkExtent2D destinationExtent)
	{
		VkImageBlit2 imageBlit2Region = {};
		imageBlit2Region.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
		imageBlit2Region.pNext = nullptr;

		imageBlit2Region.srcOffsets[1].x = sourceExtent.width;
		imageBlit2Region.srcOffsets[1].y = sourceExtent.height;
		imageBlit2Region.srcOffsets[1].z = 1;
		imageBlit2Region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageBlit2Region.srcSubresource.baseArrayLayer = 0;
		imageBlit2Region.srcSubresource.layerCount = 1;
		imageBlit2Region.srcSubresource.mipLevel = 0;

		imageBlit2Region.dstOffsets[1].x = destinationExtent.width;
		imageBlit2Region.dstOffsets[1].y = destinationExtent.height;
		imageBlit2Region.dstOffsets[1].z = 1;
		imageBlit2Region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageBlit2Region.dstSubresource.baseArrayLayer = 0;
		imageBlit2Region.dstSubresource.layerCount = 1;
		imageBlit2Region.dstSubresource.mipLevel = 0;

		VkBlitImageInfo2 imageBlit2Info = {};
		imageBlit2Info.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
		imageBlit2Info.pNext = nullptr;
		imageBlit2Info.srcImage = source;
		imageBlit2Info.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		imageBlit2Info.dstImage = destination;
		imageBlit2Info.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imageBlit2Info.regionCount = 1;
		imageBlit2Info.pRegions = &imageBlit2Region;
		imageBlit2Info.filter = VK_FILTER_LINEAR;

		vkCmdBlitImage2(cmd, &imageBlit2Info);
	}
}