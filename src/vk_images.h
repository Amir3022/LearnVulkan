
#pragma once 

namespace vkutil {

	void transition_Image(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);
};