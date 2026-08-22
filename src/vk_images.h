
#pragma once 

#include "vk_types.h"
#include "fastgltf/glm_element_traits.hpp"
#include "fastgltf/parser.hpp"
#include "fastgltf/types.hpp"

class VulkanEngine;
namespace vkutil {

	void transition_Image(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);

	void copy_Image_to_Image(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D sourceExtent, VkExtent2D destinationExtent);

	void createMipmaps(VkCommandBuffer cmd, VkImage image, VkExtent2D imageSize);

	std::optional<AllocatedImage> loadImage(VulkanEngine* engine, fastgltf::Asset& gltfAsset, fastgltf::Image& image);
};