#pragma once

#include "vk_types.h"
#include <string>
#include <optional>
#include <filesystem>

class VulkanEngine;

namespace vkutil
{
    std::optional<std::vector<MeshAsset>> loadMeshFromFile(VulkanEngine& engine, std::filesystem::path path);
};
