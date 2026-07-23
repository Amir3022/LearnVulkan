#include "vk_types.h"

#define VMA_IMPLEMENTATION 
#include "vk_mem_alloc.h"

#include "vk_descriptors.h"

/** Rendering Related */
void MeshNode::draw(const glm::mat4& topMatrix, DrawContext& ctx)
{
    //Calculate Node matrix from world transform and top matrix (useful to draw same mesh multiple times with different transforms)
    glm::mat4 nodeMatrix = topMatrix * worldTransform;
    //Iterate through geo surfaces from mesh, create RenderObjects and add to draw context
    for(const GeoSurface& surface : meshAsset->surfaces)
    {
        RenderObject newRenderObject;
        newRenderObject.startIndex = surface.startIndex;
        newRenderObject.indicesCount = (uint32_t)surface.count;
        newRenderObject.indexBuffer = meshAsset->meshBuffers.indexBuffer.buffer;
        newRenderObject.material = &surface.material->data;
        newRenderObject.transform = nodeMatrix;
        newRenderObject.vertexBufferDeviceAddress = meshAsset->meshBuffers.vertexBufferDeviceAddress;

        ctx.opaqueMeshObjects.push_back(newRenderObject);
    }

    //Call draw on Node, to draw all child nodes
    Node::draw(topMatrix, ctx);
}

LoadedGLTF::LoadedGLTF()
{
    descriptorAllocator = std::make_unique<DescriptorAllocatorGrowable>();
}

LoadedGLTF::~LoadedGLTF()
{
    clearAll();
}

void LoadedGLTF::draw(const glm::mat4& topMatrix, DrawContext& ctx)
{
    //Draw the parentNodes as they will call draw on the child nodes as well
    for(auto node: parents)
    {
        node->draw(topMatrix, ctx);
    }
}

void LoadedGLTF::clearAll()
{
    
}