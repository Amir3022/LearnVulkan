
#pragma once 

namespace vkutil {

	void transition_Image(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);

	void copy_Image_to_Image(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D sourceExtent, VkExtent2D destinationExtent);
};