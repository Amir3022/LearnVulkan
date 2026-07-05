#include <vk_pipelines.h>
#include <fstream>
#include "vk_initializers.h"

namespace vkutil
{
    bool load_Shader_Module(const std::string& filePath, VkDevice device, VkShaderModule* outShaderModule)
    {
        //Use fstream to open the file with provided path, return false if file couldn't be found
        std::ifstream file(filePath.c_str(), std::ios::ate | std::ios::binary); //use flags to have the cursor set at the end, and read the data as binary bytes
        if(!file.is_open())
            return false;
        
        //Cursor set at the end of the file, can be used to determine length of data in bytes
        size_t fileSize = file.tellg();

        //Create buffer that holds int32 data (spirv expects int32 data)
        std::vector<uint32_t> buffer(fileSize/sizeof(uint32_t));

        //Set the cursor at the start of the file, and start reading data into the buffer
        file.seekg(0);
        file.read((char*)buffer.data(), fileSize);

        //Close the file after finishing reading data
        file.close();

        //Use shader module creation info to create shader module
        VkShaderModuleCreateInfo shaderModuleCreateInfo = {};
        shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderModuleCreateInfo.pNext = nullptr;
        shaderModuleCreateInfo.flags = 0;
        shaderModuleCreateInfo.codeSize = buffer.size() * sizeof(uint32_t); //Size needed in bytes
        shaderModuleCreateInfo.pCode = buffer.data();

        //Try creating shader module, if failed return false
        VkShaderModule shaderModule;
        if(vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr, &shaderModule) != VK_SUCCESS)
            return false;

        //Set the output shader module and return true
        *outShaderModule = shaderModule;
        return true;
    }
}

void PipelineBuilder::reset()
{
    //Reset all the create info to default state
    _shaderStagesInfo.clear();
    _inputAssemblyInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    _rasterizerInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    _multisampleInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    _depthStencilInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    _colorBlendAttachment = {};
    _renderInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    _pipelineLayout = {};
}

VkPipeline PipelineBuilder::build_pipeline(VkDevice device)
{
    //Initialize Vertex Input Create Info with empty data since we won't be using vertex attributes in vertex buffers to send data to vertex shader
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

    //Initialize Viewport state with empty data, since dynamic state will be used later to change viewport settings
    VkPipelineViewportStateCreateInfo viewportStateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportStateInfo.pNext = nullptr;
    viewportStateInfo.viewportCount = 1;
    viewportStateInfo.scissorCount = 1;
    
    //Initialize Color Blend State Info, we won't be using blending, so we need just to attach the color blend attachment state
    VkPipelineColorBlendStateCreateInfo colorBlendInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    colorBlendInfo.pNext = nullptr;
    colorBlendInfo.logicOp = VK_LOGIC_OP_COPY;
    colorBlendInfo.logicOpEnable = VK_FALSE;
    colorBlendInfo.attachmentCount = 1;
    colorBlendInfo.pAttachments = &_colorBlendAttachment;

    //Create Dynamic State Create info for Viewport and scissors
    std::array<VkDynamicState, 2> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicStateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamicStateInfo.pNext = nullptr;
    dynamicStateInfo.dynamicStateCount =(uint32_t)dynamicStates.size();
    dynamicStateInfo.pDynamicStates = dynamicStates.data();

    //Create the Graphics Pipeline Create info, attach all create info needed for initilization, set as next the Pipeline Rendering info
    VkGraphicsPipelineCreateInfo pipelineCreateInfo = {.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pipelineCreateInfo.pNext = &_renderInfo;

    pipelineCreateInfo.stageCount = (uint32_t)_shaderStagesInfo.size();
    pipelineCreateInfo.pStages = _shaderStagesInfo.data();
    pipelineCreateInfo.pVertexInputState = &vertexInputInfo;
    pipelineCreateInfo.pInputAssemblyState = &_inputAssemblyInfo;
    pipelineCreateInfo.pViewportState = &viewportStateInfo;
    pipelineCreateInfo.pRasterizationState = &_rasterizerInfo;
    pipelineCreateInfo.pMultisampleState = &_multisampleInfo;
    pipelineCreateInfo.pDepthStencilState = &_depthStencilInfo;
    pipelineCreateInfo.pColorBlendState = &colorBlendInfo;
    pipelineCreateInfo.pDynamicState = &dynamicStateInfo;
    pipelineCreateInfo.layout = _pipelineLayout;

    //Try to create graphics pipeline using create info
    VkPipeline graphicsPipeline;
    if(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineCreateInfo, nullptr, &graphicsPipeline) != VK_SUCCESS)
    {
        fmt::println("Failed to create Graphics Pipeline");
        return VK_NULL_HANDLE;
    }

    return graphicsPipeline;
}

void PipelineBuilder::addShaderStages(VkShaderModule vertexShader, VkShaderModule fragmentShader)
{
    //Initialize VkPipelineShaderStageCreateInfo for each shader module and add them to vector
    VkPipelineShaderStageCreateInfo vertexShaderStageInfo = vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, vertexShader, "main");
    _shaderStagesInfo.push_back(vertexShaderStageInfo);

    VkPipelineShaderStageCreateInfo fragmentShaderStageInfo = vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader, "main");
    _shaderStagesInfo.push_back(fragmentShaderStageInfo);
}

void PipelineBuilder::setInputTopology(VkPrimitiveTopology topology)
{
    //Set Input Assembly primitive topology (Wether to draw lines, triangles, points)
    _inputAssemblyInfo.topology = topology;
    _inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;   //Used for strips, won't be used
}

void PipelineBuilder::SetPolygonMode(VkPolygonMode mode)
{
    //Set the Polygon mode in rasterization State Info
    _rasterizerInfo.polygonMode = mode;
    _rasterizerInfo.lineWidth = 1.0f;
}

void PipelineBuilder::SetCullingMode(VkCullModeFlags cullModeFlags, VkFrontFace frontFace)
{
    //Set Cull Mode Flags and Front Face type in rasterier info
    _rasterizerInfo.cullMode = cullModeFlags;
    _rasterizerInfo.frontFace = frontFace;
}

void PipelineBuilder::setMultisampleNone()
{
    //Disable Multisample by setting the sample count to 1
    _multisampleInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    _multisampleInfo.sampleShadingEnable = VK_FALSE;
    _multisampleInfo.minSampleShading = 1.0f;
    _multisampleInfo.pSampleMask = nullptr;

    //Set Alpha to Coverage disabled
    _multisampleInfo.alphaToCoverageEnable = VK_FALSE;
    _multisampleInfo.alphaToOneEnable = VK_FALSE;
}

void PipelineBuilder::disableDepthTest()
{
    //Disable Depth and stencil for the created pipeline
    _depthStencilInfo.depthTestEnable = VK_FALSE;
    _depthStencilInfo.depthWriteEnable = VK_FALSE;
    _depthStencilInfo.depthCompareOp = VK_COMPARE_OP_NEVER;
    _depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
    _depthStencilInfo.stencilTestEnable = VK_FALSE;
    _depthStencilInfo.front = {};
    _depthStencilInfo.back = {};
    _depthStencilInfo.minDepthBounds = 0.0f;
    _depthStencilInfo.maxDepthBounds = 1.0f;
}

void PipelineBuilder::enableDepthTest(bool depthWriteEnable, VkCompareOp compareOp)
{
    //Enable Depth and stencil for the created pipeline
    _depthStencilInfo.depthTestEnable = VK_TRUE;
    _depthStencilInfo.depthWriteEnable = depthWriteEnable;
    _depthStencilInfo.depthCompareOp = compareOp;
    _depthStencilInfo.depthBoundsTestEnable = VK_FALSE;
    _depthStencilInfo.stencilTestEnable = VK_FALSE;
    _depthStencilInfo.front = {};
    _depthStencilInfo.back = {};
    _depthStencilInfo.minDepthBounds = 0.0f;
    _depthStencilInfo.maxDepthBounds = 1.0f;
}

void PipelineBuilder::disableBlending()
{
    //Disable blending in the color blend attachment
    _colorBlendAttachment.blendEnable = VK_FALSE;
    _colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
}

void PipelineBuilder::setColorAttachmentFormat(VkFormat format)
{
    _colorAttachmentFormat = format;
    //Attach the color attachment formal to the pipeline rendering info
    _renderInfo.colorAttachmentCount = 1;
    _renderInfo.pColorAttachmentFormats = &_colorAttachmentFormat;
}

void PipelineBuilder::setDepthFormat(VkFormat format)
{
    _renderInfo.depthAttachmentFormat = format;
}
