#include <vk_loader.h>
#include <vk_engine.h>

#include "fastgltf/glm_element_traits.hpp"
#include "fastgltf/parser.hpp"
#include "fastgltf/types.hpp"


VkFilter getVulkanFilterType(fastgltf::Filter gltfFilter)
{
    VkFilter returnFilter;
    switch(gltfFilter)
    {
    case fastgltf::Filter::Linear:
    case fastgltf::Filter::LinearMipMapLinear:
    case fastgltf::Filter::LinearMipMapNearest:
        returnFilter = VK_FILTER_LINEAR;
        break;
    case fastgltf::Filter::Nearest:
    case fastgltf::Filter::NearestMipMapLinear:
    case fastgltf::Filter::NearestMipMapNearest:
        returnFilter = VK_FILTER_NEAREST;
        break;
    }

    return returnFilter;
}

VkSamplerMipmapMode getVulkanMipMapMode(fastgltf::Filter gltfFilter)
{
    VkSamplerMipmapMode returnMode;
    switch(gltfFilter)
    {
    case fastgltf::Filter::LinearMipMapLinear:
    case fastgltf::Filter::NearestMipMapLinear:
        returnMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        break;
    case fastgltf::Filter::LinearMipMapNearest:
    case fastgltf::Filter::NearestMipMapNearest:
        returnMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        break;
    }
    return returnMode;
}

namespace vkutil
{
    std::optional<std::vector<std::shared_ptr<MeshAsset>>> loadMeshFromFile(VulkanEngine &engine, std::filesystem::path path)
    {
        fmt::println("Trying to load Mesh from file {}", path.string());

        //Try to load the Mesh data from file
        fastgltf::GltfDataBuffer data;
        if(!data.loadFromFile(path))
        {
            fmt::println("Failed to load mesh from file: {}", path.string());
            return std::nullopt;
        }

        //Specify the load options for GLTF parser to load binary gltf assets
        fastgltf::Options loadOptions = fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers;

        fastgltf::Asset gltfAsset;
        fastgltf::Parser parser{};

        //Try to parse the mesh data from the loaded file
        auto parsedAsset = parser.loadBinaryGLTF(&data, path.parent_path(), loadOptions);
        if(!parsedAsset)
        {
            fmt::println("Failed to parse mesh from file: {}, with error: {}", path.string(), fastgltf::getErrorMessage(parsedAsset.error()));
            return std::nullopt;
        }
        //Move the parset asset to out gltfAsset variable
        gltfAsset = std::move(parsedAsset.get());

        //Create mesh asset from sub meshes in the gltf asset
        std::vector<std::shared_ptr<MeshAsset>> meshAssets;

        //Create containers to hold the read vertices and indices from each submesh
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices; 

        for(const auto &mesh : gltfAsset.meshes)
        {
            //Create new meshAsset to hold the data for each submesh
            MeshAsset newMeshAsset;
            newMeshAsset.name = mesh.name;
            newMeshAsset.surfaces.reserve(mesh.primitives.size());

            //Reset the Vertex and index vectors to avoid cojoinging different meshes
            vertices.clear();
            indices.clear();

            //Iterate through primitives of each mesh, and read indeices and vertex data and add them to the containers
            for(const auto& primitive : mesh.primitives)
            {
                //Cache the initial vtx for current primitvie
                size_t initialVertCount = vertices.size();
                
                //Create new GeoSurface for each primitive
                GeoSurface newSurface;
                newSurface.startIndex = (uint32_t)indices.size();
                newSurface.count = 0;

                //Check if the primitive attributes has indices accessor
                if(primitive.indicesAccessor.has_value())
                {
                    //Get the indices accessor for this primitive from the gltfAsset
                    fastgltf::Accessor& indicesAccessor = gltfAsset.accessors[primitive.indicesAccessor.value()];
                    //Set the GeoSurface indices count
                    newSurface.count = indicesAccessor.count;

                    //Use Accessor iterator to iterate through indices and push them to index container
                    indices.reserve(indices.size() + newSurface.count);

                    fastgltf::iterateAccessor<uint32_t>(gltfAsset, indicesAccessor, 
                    [&](uint32_t idx)
                    {
                       indices.push_back(idx + (uint32_t)initialVertCount); 
                    });
                }
                //Add the GeoSurface to meshAsset surfaces
                newMeshAsset.surfaces.push_back(newSurface);
                
                //Check if the primitive attributes has position attribute
                auto posAttrib = primitive.findAttribute("POSITION");
                if(posAttrib != primitive.attributes.end())
                {
                    //Get the Position accessor iterator and use to iterate through primitive vertices and read position data
                    fastgltf::Accessor& posAccessor = gltfAsset.accessors[posAttrib->second];

                    //Resize the Vertices container with vertices new count
                    vertices.resize(vertices.size() + posAccessor.count);

                    fastgltf::iterateAccessorWithIndex<glm::vec3>(gltfAsset, posAccessor, 
                    [&](glm::vec3 vtx_pos, size_t idx)
                    {
                        Vertex newVtx;
                        newVtx.position = vtx_pos;
                        newVtx.normal = glm::vec3(0.0f, 0.0f, 0.0f);
                        newVtx.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
                        newVtx.uv_x = 0;
                        newVtx.uv_y = 0;
                        vertices[initialVertCount + idx] = newVtx;
                    });
                }

                //Check if the primitive attribute has normal attribute
                auto normalAttrib = primitive.findAttribute("NORMAL");
                if(normalAttrib != primitive.attributes.end())
                {
                    //Get the Normal accessor iterator and use to iterate through primitive vertices and read normal data
                    fastgltf::Accessor& normAccessor = gltfAsset.accessors[normalAttrib->second];

                    fastgltf::iterateAccessorWithIndex<glm::vec3>(gltfAsset, normAccessor, 
                    [&](glm::vec3 norm, size_t idx)
                    {
                        vertices[initialVertCount + idx].normal = norm;
                    });
                }

                //Check if the primitive attribute has color attribute
                auto colorAttrib = primitive.findAttribute("COLOR_0");
                if(colorAttrib != primitive.attributes.end())
                {
                    //Get the Color accessor iterator and use to iterate through primitive vertices and read color data
                    fastgltf::Accessor& colorAccessor = gltfAsset.accessors[colorAttrib->second];

                    fastgltf::iterateAccessorWithIndex<glm::vec4>(gltfAsset, colorAccessor, 
                    [&](glm::vec4 color, size_t idx)
                    {
                        vertices[initialVertCount + idx].color = color;
                    });
                }

                //Check if the primitive attribute has TexCoord attribute
                auto uvAttrib = primitive.findAttribute("TEXCOORD_0");
                if(uvAttrib != primitive.attributes.end())
                {
                    //Get the TexCoord accessor iterator and use to iterate through primitive vertices and read TexCoord data
                    fastgltf::Accessor& uvAccessor = gltfAsset.accessors[uvAttrib->second];

                    fastgltf::iterateAccessorWithIndex<glm::vec2>(gltfAsset, uvAccessor, 
                    [&](glm::vec2 uv, size_t idx)
                    {
                        vertices[initialVertCount + idx].uv_x = uv.x;
                        vertices[initialVertCount + idx].uv_y = uv.y;
                    });
                }

                //FOR DEBUGGING, force the normal to be the color of each vertex
                constexpr bool bOverrideColorWithNormal = false;
                if(bOverrideColorWithNormal)
                {
                    for(Vertex& vert : vertices)
                    {
                        vert.color = glm::vec4(vert.normal, 1.0);
                    }
                }
            }
            //Use the engine to upload the vertices and indices vectors, and use them to create mesh buffers
            newMeshAsset.meshBuffers = engine.uploadMesh(vertices, indices);

            meshAssets.push_back(std::make_shared<MeshAsset>(std::move(newMeshAsset)));
        }

        return meshAssets;
    }

    std::optional<std::shared_ptr<LoadedGLTF>> loadGLTF(VulkanEngine* engine, std::filesystem::path path)
    {
        fmt::println("Trying to load GLTF Scene from file {}", path.string());
        //Create new instance of LoadedGLTF
        std::shared_ptr<LoadedGLTF> loadedScene = std::make_shared<LoadedGLTF>();
        loadedScene->creator = engine;

        //Try to load the Mesh data from file
        fastgltf::GltfDataBuffer data;
        if(!data.loadFromFile(path))
        {
            fmt::println("Failed to load GLTF Data from file: {}", path.string());
            return std::nullopt;
        }

        //Specify the load options for GLTF parser to load binary gltf assets
        fastgltf::Options loadOptions = fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers | fastgltf::Options::AllowDouble | fastgltf::Options::DontRequireValidAssetMember;
        fastgltf::Asset gltfAsset;
        fastgltf::Parser parser{};

        //Parse the GLTF Asset based on the loaded asset data type
        if(fastgltf::determineGltfFileType(&data) == fastgltf::GltfType::glTF)
        {
            auto parsedAsset = parser.loadGLTF(&data, path.parent_path(), loadOptions);
            if(!parsedAsset)
            {
                fmt::println("Failed to parse GTLF Scene from file: {}, with error: {}", path.string(), fastgltf::getErrorMessage(parsedAsset.error()));
                return std::nullopt;
            }
            gltfAsset = std::move(parsedAsset.get());
        }
        else if (fastgltf::determineGltfFileType(&data) == fastgltf::GltfType::GLB)
        {
            auto parsedAsset = parser.loadBinaryGLTF(&data, path.parent_path(), loadOptions);
            if(!parsedAsset)
            {
                fmt::println("Failed to parse GLB Scene from file: {}, with error: {}", path.string(), fastgltf::getErrorMessage(parsedAsset.error()));
                return std::nullopt;
            }
            gltfAsset = std::move(parsedAsset.get());
        }
        else
        {
                fmt::println("Failed to determine loaded data type");
                return std::nullopt;    
        }

        //Create poolSizeRatios with estimated sized, and use them to create descriptorAllocaterGrowable for the gltfAsset
        std::vector<PoolSizeRatio> sizes =
        {
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3}, 
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}
        };

        loadedScene->descriptorAllocator->init(engine->getDevice(), gltfAsset.materials.size(), sizes); //Set the max set per pool equal to the number of materials in the scene

        //Create samplers from info form gltfAsset
        for(const fastgltf::Sampler& sampler : gltfAsset.samplers)
        {
            //Use Sampler Create Info to create new VkSampler
            VkSamplerCreateInfo samplerCreateInfo = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
            samplerCreateInfo.pNext = nullptr;
            samplerCreateInfo.minLod = 0;
            samplerCreateInfo.maxLod = VK_LOD_CLAMP_NONE;
            samplerCreateInfo.minFilter = getVulkanFilterType(sampler.minFilter.value_or(fastgltf::Filter::Nearest));
            samplerCreateInfo.magFilter = getVulkanFilterType(sampler.magFilter.value_or(fastgltf::Filter::Nearest));
            samplerCreateInfo.mipmapMode = getVulkanMipMapMode(sampler.minFilter.value_or(fastgltf::Filter::NearestMipMapNearest));
            
            VkSampler newSampler;
            if(vkCreateSampler(engine->getDevice(), &samplerCreateInfo, nullptr, &newSampler) == VK_SUCCESS)    //If the sampler is successfully created add it to the sampler list in the loaded scene
            {
                loadedScene->samplers.push_back(newSampler);
            }
        }

        //Create flat containers to contain the gltf data read from gltfAsset
        std::vector<std::shared_ptr<MeshAsset>> meshes;
        std::vector<std::shared_ptr<Node>> nodes;
        std::vector<AllocatedImage> images;
        std::vector<std::shared_ptr<GLTFMaterial>> materials;

        //Asset read should be done with this order images->materials->meshes->nodes
        //Set all images to the default error material (TODO - Set actual gltf Texture)
        for(const fastgltf::Image& image : gltfAsset.images)
        {
            images.push_back(engine->getDefaultErrorTexture());
        }

        //Create material parameters buffer for the whole scene
        loadedScene->materialParametersBuffer = engine->createBuffer(sizeof(GLTF_MetallicRoughMaterial::MaterialConstants) * gltfAsset.materials.size(), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
        std::vector<GLTF_MetallicRoughMaterial::MaterialConstants> materialsParamatersData;
        materialsParamatersData.clear();

        //Create MaterialConstants and MaterialResources from data from gltfAsset materials and add them both materials and loadedScene materials
        for(const fastgltf::Material& material : gltfAsset.materials)
        {
            //Create new GLTFMaterial
            std::shared_ptr<GLTFMaterial> newMat = std::make_shared<GLTFMaterial>();
            materials.push_back(newMat);
            loadedScene->materials[material.name.c_str()] = newMat;

            //Create Material Constants struct and fill it's data
            GLTF_MetallicRoughMaterial::MaterialConstants materialParams;
            materialParams.colorFactors = glm::vec4(material.pbrData.baseColorFactor[0], material.pbrData.baseColorFactor[1], material.pbrData.baseColorFactor[2], material.pbrData.baseColorFactor[3]);
            materialParams.metal_roughFactors = glm::vec4(material.pbrData.metallicFactor, material.pbrData.roughnessFactor, 0.0f, 0.0f);

            //Create Material Resources struct and fill it's data
            //Start material with default values
            GLTF_MetallicRoughMaterial::MaterialResources materialResources;
            materialResources.colorTexture = engine->getWhiteTexture();
            materialResources.colorTextureSampler = engine->getDefaultNearestSampler();
            materialResources.metalRoughTexture = engine->getWhiteTexture();
            materialResources.metalRoughTextureSampler = engine->getDefaultNearestSampler();
            //Set the Material Parameters Data buffer to be the one shared by the loaded gltf asset, and get the exact parameters using offset (calculated from parameters data array size)
            materialResources.materialDataBuffer = loadedScene->materialParametersBuffer.buffer;
            materialResources.materialDataBufferOffset = sizeof(GLTF_MetallicRoughMaterial::MaterialConstants) * materialsParamatersData.size();
            //Check if the material pbr data has base color index
            if(material.pbrData.baseColorTexture.has_value())
            {
                //Get the texture and sampler indices in prespective vectors
                size_t img = gltfAsset.textures[material.pbrData.baseColorTexture.value().textureIndex].imageIndex.value();
                size_t samp = gltfAsset.textures[material.pbrData.baseColorTexture.value().textureIndex].samplerIndex.value();
                materialResources.colorTexture = images[img];
                materialResources.colorTextureSampler = loadedScene->samplers[samp];
            }
            //Get the material pass type from material alpha blend type
            EMaterialPass matPass = EMaterialPass::MaterialColor;
            if(material.alphaMode == fastgltf::AlphaMode::Blend )
            {
                matPass= EMaterialPass::Transparent;
            }

            //Use Material Constants and Resources to create material instance from material blueprint
            newMat->data = engine->getDefaultMatTemp()->writeMaterial(engine->getDevice(), matPass, materialResources, *loadedScene->descriptorAllocator);

            //Add the Material Constants to vector
            materialsParamatersData.push_back(materialParams);
        }
        //Copy the data in materialsParameters vector to the allocated buffer
        memcpy(loadedScene->materialParametersBuffer.allocationInfo.pMappedData, materialsParamatersData.data(), sizeof(GLTF_MetallicRoughMaterial::MaterialConstants) * materialsParamatersData.size());

        //Read meshes data
        //Create containers to hold the read vertices and indices from each submesh
        std::vector<Vertex> vertices;
        std::vector<uint32_t> indices; 
        for(const fastgltf::Mesh& mesh : gltfAsset.meshes)
        {
            //Create new meshAsset to hold the data for each submesh
            std::shared_ptr<MeshAsset> newMesh = std::make_shared<MeshAsset>();
            newMesh->name = mesh.name;
            newMesh->surfaces.reserve(mesh.primitives.size());
            //Add the newly created mesh to meshes vectors and loadedScene meshes
            meshes.push_back(newMesh);
            loadedScene->meshes[mesh.name.c_str()] = newMesh;

            //Reset the Vertex and index vectors to avoid cojoinging different meshes
            vertices.clear();
            indices.clear();

            //Iterate through primitives of each mesh, and read indeices and vertex data and add them to the containers
            for(const auto& primitive : mesh.primitives)
            {
                //Cache the initial vtx for current primitvie
                size_t initialVertCount = vertices.size();
                
                //Create new GeoSurface for each primitive
                GeoSurface newSurface;
                newSurface.startIndex = (uint32_t)indices.size();
                newSurface.count = 0;

                //Check if the primitive attributes has indices accessor
                if(primitive.indicesAccessor.has_value())
                {
                    //Get the indices accessor for this primitive from the gltfAsset
                    fastgltf::Accessor& indicesAccessor = gltfAsset.accessors[primitive.indicesAccessor.value()];
                    //Set the GeoSurface indices count
                    newSurface.count = indicesAccessor.count;

                    //Use Accessor iterator to iterate through indices and push them to index container
                    indices.reserve(indices.size() + newSurface.count);

                    fastgltf::iterateAccessor<uint32_t>(gltfAsset, indicesAccessor, 
                    [&](uint32_t idx)
                    {
                       indices.push_back(idx + (uint32_t)initialVertCount); 
                    });
                }

                //Check if primitive has material index accessor
                if(primitive.materialIndex.has_value())
                {
                    newSurface.material = materials[primitive.materialIndex.value()];
                }
                else
                {
                    newSurface.material = materials[0];
                }
                //Add the GeoSurface to meshAsset surfaces
                newMesh->surfaces.push_back(newSurface);
                
                //Check if the primitive attributes has position attribute
                auto posAttrib = primitive.findAttribute("POSITION");
                if(posAttrib != primitive.attributes.end())
                {
                    //Get the Position accessor iterator and use to iterate through primitive vertices and read position data
                    fastgltf::Accessor& posAccessor = gltfAsset.accessors[posAttrib->second];

                    //Resize the Vertices container with vertices new count
                    vertices.resize(vertices.size() + posAccessor.count);

                    fastgltf::iterateAccessorWithIndex<glm::vec3>(gltfAsset, posAccessor, 
                    [&](glm::vec3 vtx_pos, size_t idx)
                    {
                        Vertex newVtx;
                        newVtx.position = vtx_pos;
                        newVtx.normal = glm::vec3(0.0f, 0.0f, 0.0f);
                        newVtx.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
                        newVtx.uv_x = 0;
                        newVtx.uv_y = 0;
                        vertices[initialVertCount + idx] = newVtx;
                    });
                }

                //Check if the primitive attribute has normal attribute
                auto normalAttrib = primitive.findAttribute("NORMAL");
                if(normalAttrib != primitive.attributes.end())
                {
                    //Get the Normal accessor iterator and use to iterate through primitive vertices and read normal data
                    fastgltf::Accessor& normAccessor = gltfAsset.accessors[normalAttrib->second];

                    fastgltf::iterateAccessorWithIndex<glm::vec3>(gltfAsset, normAccessor, 
                    [&](glm::vec3 norm, size_t idx)
                    {
                        vertices[initialVertCount + idx].normal = norm;
                    });
                }

                //Check if the primitive attribute has color attribute
                auto colorAttrib = primitive.findAttribute("COLOR_0");
                if(colorAttrib != primitive.attributes.end())
                {
                    //Get the Color accessor iterator and use to iterate through primitive vertices and read color data
                    fastgltf::Accessor& colorAccessor = gltfAsset.accessors[colorAttrib->second];

                    fastgltf::iterateAccessorWithIndex<glm::vec4>(gltfAsset, colorAccessor, 
                    [&](glm::vec4 color, size_t idx)
                    {
                        vertices[initialVertCount + idx].color = color;
                    });
                }

                //Check if the primitive attribute has TexCoord attribute
                auto uvAttrib = primitive.findAttribute("TEXCOORD_0");
                if(uvAttrib != primitive.attributes.end())
                {
                    //Get the TexCoord accessor iterator and use to iterate through primitive vertices and read TexCoord data
                    fastgltf::Accessor& uvAccessor = gltfAsset.accessors[uvAttrib->second];

                    fastgltf::iterateAccessorWithIndex<glm::vec2>(gltfAsset, uvAccessor, 
                    [&](glm::vec2 uv, size_t idx)
                    {
                        vertices[initialVertCount + idx].uv_x = uv.x;
                        vertices[initialVertCount + idx].uv_y = uv.y;
                    });
                }
            }
            //Use the engine to upload the vertices and indices vectors, and use them to create mesh buffers
            newMesh->meshBuffers = engine->uploadMesh(vertices, indices);
        }

        //Read Nodes data from GLTFAsset
        for(const fastgltf::Node& node : gltfAsset.nodes)
        {
            //Create new node asset 
            std::shared_ptr<Node> newNode;

            //Check if it's a meshNode
            if(node.meshIndex.has_value())
            {
                newNode = std::make_shared<MeshNode>();
                MeshNode* meshNode = static_cast<MeshNode*>(newNode.get());
                meshNode->meshAsset = meshes[node.meshIndex.value()];
            }
            else
            {
                newNode = std::make_shared<Node>();
            }
            //Add to nodes and loadedScene nodes
            nodes.push_back(newNode);
            loadedScene->nodes[node.name.c_str()] = newNode;

            //Use std::visit to build the node transform depending on the used variant by the gltf node
            std::visit(fastgltf::visitor{
                [&](const fastgltf::Node::TransformMatrix& matrix)  //In case the node transform is set as mat4x4, copy the data from matrix to node transform matrix
                {
                    memcpy(&newNode->localTransform, matrix.data(), sizeof(matrix));
                },
                [&](const fastgltf::Node::TRS& trs)
                {
                    glm::vec3 translation = glm::vec3(trs.translation[0], trs.translation[1], trs.translation[2]);
                    glm::quat rotation = glm::quat(trs.rotation[3], trs.rotation[0], trs.rotation[1], trs.rotation[2]);
                    glm::vec3 scale = glm::vec3(trs.scale[0], trs.scale[1], trs.scale[2]);
                    glm::mat4 trans = glm::translate(glm::identity<glm::mat4>(), translation);
                    glm::mat4 tM = glm::translate(glm::identity<glm::mat4>(), translation);
                    glm::mat4 rM = glm::toMat4(rotation);
                    glm::mat4 sM = glm::scale(glm::identity<glm::mat4>(), scale);
                    newNode->localTransform = tM * rM * sM;
                }
            }, node.transform);
        }

        //Iterate through all gltfNodes again, assign child nodes as children in loadedScene nodes, assign them as parents to children nodes
        for(size_t i = 0; i < gltfAsset.nodes.size(); i++)
        {
            std::shared_ptr<Node> node = nodes[i];
            for(size_t child : gltfAsset.nodes[i].children)
            {
                node->childNodes.push_back(nodes[child]);
                nodes[child]->parentNode = node;
            }
        }

        //Iterate through all nodes, find nodes without parents, add them to LoadedScene parentNodes, refresh transform for parent nodes thus assigning world transform to all child nodes
        for(auto node : nodes)
        {
            if(node->parentNode.lock() == nullptr)
            {
                loadedScene->parents.push_back(node);
                node->refreshTransform(glm::mat4(1.0f));
            }
        }

        //return the created loadedScene
        return loadedScene;
    }
};
