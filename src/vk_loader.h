#pragma once

#include "vk_types.h"
#include <string>
#include <optional>
#include <filesystem>


struct GeoSurface
{
    uint32_t startIndex;
    size_t count;
};

struct MeshAsset
{
    std::string name;
    std::vector<GeoSurface> surfaces;
    GPUMeshBuffers meshBuffers;
};

class VulkanEngine;

namespace vkutil
{
    std::optional<std::vector<MeshAsset>> loadMeshFromFile(VulkanEngine& engine, std::filesystem::path path);
};
