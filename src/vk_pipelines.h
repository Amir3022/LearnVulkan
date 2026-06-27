#pragma once 
#include <vk_types.h>
#include <string>
#include <vector>

namespace vkutil {
    bool load_Shader_Module(const std::string& filePath, VkDevice device, VkShaderModule* outShaderModule);
};

struct PipelineBuilder
{
    //Member Variables for Create info need for the Graphics Pipeline Create Info
    std::vector<VkPipelineShaderStageCreateInfo> _shaderStagesInfo;
    VkPipelineInputAssemblyStateCreateInfo _inputAssemblyInfo;
    VkPipelineRasterizationStateCreateInfo _rasterizerInfo;
    VkPipelineMultisampleStateCreateInfo _multisampleInfo;
    VkPipelineDepthStencilStateCreateInfo _depthStencilInfo;
    VkPipelineColorBlendAttachmentState _colorBlendAttachment;
    VkPipelineRenderingCreateInfo _renderInfo;
    VkFormat _colorAttachmentFormat;
    VkPipelineLayout _pipelineLayout;

    PipelineBuilder()
    {
        reset();
    }

    void reset();

    VkPipeline build_pipeline(VkDevice device);

    void addShaderStages(VkShaderModule vertexShader, VkShaderModule fragmentShader);

    void setInputTopology(VkPrimitiveTopology topology);

    void SetPolygonMode(VkPolygonMode mode);

    void SetCullingMode(VkCullModeFlags cullModeFlags, VkFrontFace frontFace);

    void setMultisampleNone();

    void disableDepthTest();

    void disableBlending();

    void setColorAttachmentFormat(VkFormat format);

    void setDepthFormat(VkFormat format);
};