#include <vk_loader.h>
#include <vk_engine.h>

#include "fastgltf/glm_element_traits.hpp"
#include "fastgltf/parser.hpp"
#include "fastgltf/types.hpp"

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
};