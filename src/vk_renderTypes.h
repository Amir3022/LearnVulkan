#pragma once
#include "vk_types.h"

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