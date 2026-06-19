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
