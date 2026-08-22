#include "vk_renderTypes.h"
#include "vk_engine.h"
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
        newRenderObject.bounds = surface.bounds;
        newRenderObject.vertexBufferDeviceAddress = meshAsset->meshBuffers.vertexBufferDeviceAddress;
        if(surface.material->data.pass == EMaterialPass::MaterialColor)
            ctx.opaqueMeshObjects.push_back(newRenderObject);
        else if(surface.material->data.pass == EMaterialPass::Transparent)
            ctx.transparentMeshObjects.push_back(newRenderObject);
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
    //Clear the buffers in all mesh assets
    for(auto mesh : meshes)
    {
        creator->destroyBuffer(mesh.second->meshBuffers.vertexBuffer);
        creator->destroyBuffer(mesh.second->meshBuffers.indexBuffer);
    }

    //Destroy all images (make sure note to destroy any default image)
    for(auto image : textures)
    {
        if(creator->isDefaultTexture(image.second))
            continue;
        creator->destroyImage(image.second);
    }

    //Clear the MaterialParameters buffer
    creator->destroyBuffer(materialParametersBuffer);

    //Destroy all the samplers created
    for(auto sampler : samplers)
    {
        vkDestroySampler(creator->getDevice(), sampler, nullptr);
    }

    //Destroy all descriptor sets created by the descriptor allocator
    descriptorAllocator->destroyPools(creator->getDevice());
}