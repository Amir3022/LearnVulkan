// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <span>
#include <array>
#include <functional>
#include <deque>
#include <chrono>

#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vk_mem_alloc.h>

#include <fmt/core.h>

//Headers from GLM
#include <glm/glm.hpp> 
#include <glm/gtx/quaternion.hpp> 
#include <glm/gtx/transform.hpp> 
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>


#define VK_CHECK(x)                                                     \
    do {                                                                \
        VkResult err = x;                                               \
        if (err) {                                                      \
            fmt::println("Detected Vulkan error: {} at {}, {}" , string_VkResult(err), __FILE__, __LINE__); \
            abort();                                                    \
        }                                                               \
    } while (0)

struct AllocatedImage
{
    VkImage _image;
    VkImageView _imageView;
    VmaAllocation _allocation;
    VkFormat _format;
    VkExtent3D _extent;
};

struct ComputePushConstants
{
    glm::vec4 data1;
    glm::vec4 data2;
    glm::vec4 data3;
    glm::vec4 data4;
};

struct ComputeEffect
{
    std::string name;
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;
    ComputePushConstants pc_data;
};

//Allocated Memory for buffer
struct AllocatedBuffer
{
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo allocationInfo;
};

//GPU Mesh Draw Types
struct Vertex
{
    glm::vec3 position;
    float uv_x;
    glm::vec3 normal;
    float uv_y;
    glm::vec4 color;
};

struct GPUMeshBuffers
{
    AllocatedBuffer vertexBuffer;
    AllocatedBuffer indexBuffer;
    VkDeviceAddress vertexBufferDeviceAddress;
};

struct GPUSceneData
{
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 viewProj;
    glm::vec4 ambientColor;
    glm::vec4 sunlightDirection; //w is used for power
    glm::vec4 sunlightColor;
};

struct GPUDrawPushConstants
{
    glm::mat4 worldTransform;
    VkDeviceAddress vertexBufferDeviceAddress;
};

struct MaterialInstance;
struct GLTFMaterial;
struct GeoSurface
{
    uint32_t startIndex;
    size_t count;
    std::shared_ptr<GLTFMaterial> material;
};

struct MeshAsset
{
    std::string name;
    std::vector<GeoSurface> surfaces;
    GPUMeshBuffers meshBuffers;
};

/** Material related */
enum class EMaterialPass : uint8_t
{
    MaterialColor, 
    Transparent,
    Other,
};

struct MaterialPipeline
{
    VkPipeline pipeline;
    VkPipelineLayout pipelineLayout;
};

struct MaterialInstance
{
    MaterialPipeline* materialPipeline;
    VkDescriptorSet materialSet;
    EMaterialPass pass;
};

struct GLTFMaterial
{
    MaterialInstance data;
};

/** Rendering Meshes Related */
struct RenderObject
{
    uint32_t startIndex;
    uint32_t indicesCount;
    VkBuffer indexBuffer;

    MaterialInstance* material;

    glm::mat4 transform;
    VkDeviceAddress vertexBufferDeviceAddress;
};

struct DrawContext
{
    std::vector<RenderObject> opaqueMeshObjects;
};

//Interface for all structs and classes that will have rendering
class IRenderable   //Interface class with pure virtual functions
{
    virtual void draw(const glm::mat4& topMatrix, DrawContext& ctx) = 0;
};

//Using scene graph tree like composition struct for drawable objects
//Node will have information about local and world transform as well as 
//Parent and child nodes
struct Node : public IRenderable
{
    std::weak_ptr<Node> parentNode; //weak ptr to avoid ownership and prevent circular dependency
    std::vector<std::shared_ptr<Node>> childNodes;
    glm::mat4 localTransform;
    glm::mat4 worldTransform;

    //Function to update the nodes world transforms when it's parent world transform changes
    void refreshTransform(const glm::mat4& parentTransform)
    {
        worldTransform = parentTransform * localTransform;
        //Iterate through children node and do the same to them
        for(auto childNode : childNodes)
        {
            childNode->refreshTransform(worldTransform);
        }
    }

    virtual void draw(const glm::mat4& topMatrix, DrawContext& ctx) override
    {
        //Iterate through each child node and call draw (Draw functionality will be handled by MeshNode)
        for(auto childNode: childNodes)
        {
            childNode->draw(topMatrix, ctx);
        }
    }
};

//MeshNode will have actual rendering logic, shared ptr to Mesh Asset
struct MeshNode : public Node
{
    std::shared_ptr<MeshAsset> meshAsset;

    //Will create RenderObject from each geo surface of the mesh, and add them to draw context
    virtual void draw(const glm::mat4& topMatrix, DrawContext& ctx) override;
};

//LoadedGLTF mirrors the structure of GLTF files concerning nodes, meshes, materials, textures
class VulkanEngine;
struct DescriptorAllocatorGrowable;
struct LoadedGLTF : public IRenderable
{
    //GLTF Asset structure components
    std::unordered_map<std::string, std::shared_ptr<MeshAsset>> meshes;
    std::unordered_map<std::string, std::shared_ptr<Node>> nodes;
    std::unordered_map<std::string, std::shared_ptr<GLTFMaterial>> materials;
    std::unordered_map<std::string, AllocatedImage> textures;

    //Head Parent nodes to start the draw tree from 
    std::vector<std::shared_ptr<Node>> parents;

    //Samplers used by this Asset
    std::vector<VkSampler> samplers;

    //Single buffer for all material constant parameters (to be sized with the size of MaterialConstant * materials count)
    AllocatedBuffer materialParametersBuffer;

    //Local Descriptor Allocator used to allocated descriptors needed for asset
    std::unique_ptr<DescriptorAllocatorGrowable> descriptorAllocator;

    VulkanEngine* creator;

    LoadedGLTF();

    ~LoadedGLTF();

    virtual void draw(const glm::mat4& topMatrix, DrawContext& ctx) override;

private:
    void clearAll();    //Clear function to free all resources used by the GLTF asset
};