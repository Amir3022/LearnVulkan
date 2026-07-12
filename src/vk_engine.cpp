//> includes
#include "vk_engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk_initializers.h>
#include <vk_images.h>
#include <vk_pipelines.h>
#include <vk_loader.h>
#include "draw_functions.h"

//Headers for Imgui
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"

//Headers from GLM
#include <glm/gtx/transform.hpp> 

#include "VkBootstrap.h"

#include <chrono>
#include <thread>

VulkanEngine* loadedEngine = nullptr;

VulkanEngine& VulkanEngine::Get() { return *loadedEngine; }

constexpr bool bUseValidationLayers = false;

VulkanEngine::VulkanEngine()
{
    _isInitialized =  false;
    _frameNumber = 0;
    stop_rendering = false;
    resize_Window = false;
    _renderScale = 1.0f;
    _windowExtent = { 800 , 600 };
    _window = nullptr;

    currentActiveBackgroundEffect = 0;
}

VulkanEngine::VulkanEngine(const VkExtent2D& inRes) : VulkanEngine()
{
    _windowExtent = inRes;
}

void VulkanEngine::init()
{
    // only one engine initialization is allowed with the application.
    assert(loadedEngine == nullptr);
    loadedEngine = this;

    // We initialize SDL and create a window with it.
    SDL_Init(SDL_INIT_VIDEO);

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

    _window = SDL_CreateWindow(
        "My Test Vulkan Engine",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        _windowExtent.width,
        _windowExtent.height,
        window_flags);

    //Init Vulkan Components
    init_Vulkan();

    init_Swapchain();

    init_Commands();

    init_Sync_Structures();

    init_Descriptors();

    init_Pipelines();

    init_imgui();

    init_Default_Values();

    init_Loaded_Mesh();

    // everything went fine
    _isInitialized = true;
}

void VulkanEngine::init_Vulkan()
{
    //Create a Vulkan Instance Builder using VKBootstrap
    vkb::InstanceBuilder instanceBuilder;

    //Build the Vulkan Instance
    auto instance_res = instanceBuilder.set_app_name("LearnVulkanEngine")   //Set App Name
        .request_validation_layers(bUseValidationLayers)                    //Set to use the Validation Layer, to avoid any GPU crashes
        .use_default_debug_messenger()                                      //Use the default Vulkan Instance debug messages
        .require_api_version(1, 3, 0)                                       //Require to use Vulkan Version 1.3.0
        .build();                                                           //Build an instance using the previous config

    //Generate a vulkan instance from builder result, and assign the internal instance and debug messanger variables
    vkb::Instance vkb_instance = instance_res.value();
    _instance = vkb_instance.instance;
    _debug_Messanger = vkb_instance.debug_messenger;

    //Create KHR Surface using the built Vulkan Instance
    SDL_Vulkan_CreateSurface(_window, _instance, &_surface);

    //Enable required Vulkan1.3 and 1.2 features we are going to use
    //Enable Dynamic Rendering and Synchronization2 from Vulkan1.3
    VkPhysicalDeviceVulkan13Features features13{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES };
    features13.dynamicRendering = true;
    features13.synchronization2 = true;
    //Enable Buffer Addressing and DescriptorIndexing from Vulkan1.2
    VkPhysicalDeviceVulkan12Features features12{ .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
    features12.bufferDeviceAddress = true;
    features12.descriptorIndexing = true;

    //Use VKBootStrap to create physical device selector, and use to select device that supports the wanted features
    vkb::PhysicalDeviceSelector vkb_pd_selector{ vkb_instance };
    vkb::PhysicalDevice vkb_PhysicalDevice = vkb_pd_selector.set_desired_version(1, 3)
        .set_required_features_12(features12)
        .set_required_features_13(features13)
        .set_surface(_surface)
        .select().value();

    //Use VKBootstrap to create device builder and use the selected Physical Device to create Device
    vkb::DeviceBuilder vkb_DeviceBuilder{ vkb_PhysicalDevice };
    vkb::Device vkb_Device = vkb_DeviceBuilder.build().value();

    //Assign values to Physical device and device handles
    _physicalDevice = vkb_Device.physical_device;
    _device = vkb_Device.device;

    //Use the VKBootstrap device to get the Queue and Queue family index
    _commandsQueue = vkb_Device.get_queue(vkb::QueueType::graphics).value();
    _commandsQueueFamilyIndex = vkb_Device.get_queue_index(vkb::QueueType::graphics).value();

    //Initialize Vulkan Memory Allocator used to allocate VRam for Vulkan Objects
    VmaAllocatorCreateInfo vma_Allocator_Create_Info = {};
    vma_Allocator_Create_Info.device = _device;
    vma_Allocator_Create_Info.physicalDevice = _physicalDevice;
    vma_Allocator_Create_Info.instance = _instance;
    vma_Allocator_Create_Info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;   //Read description on the flag to understand what it does
    vmaCreateAllocator(&vma_Allocator_Create_Info, &_allocator);

    //Add the created allocator destructor to the Main Deletion Queue
    _mainDeletionQueue.addDeletor([&]()
        {
            vmaDestroyAllocator(_allocator);
        });
}

void VulkanEngine::init_Swapchain()
{
    create_Swapchain(_windowExtent.width, _windowExtent.height);

    //Create the Draw Image Used to draw on, set the resulotion of the allocated image to be the display resolution
    SDL_DisplayMode currentDisplayMode;
    SDL_GetCurrentDisplayMode(0, &currentDisplayMode);

    VkExtent3D drawImageExtent
    {
        static_cast<uint32_t>(currentDisplayMode.w),
        static_cast<uint32_t>(currentDisplayMode.h),
        1
    };

    VkFormat imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    //Set the Format and extent of the Allocated Image
    _drawImage._format = imageFormat;
    _drawImage._extent = drawImageExtent;
    _drawExtent = {_drawImage._extent.width, _drawImage._extent.height};
    //Set the Image Usage Flags
    VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                        VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                        VK_IMAGE_USAGE_STORAGE_BIT;

    //Create the creation info
    VkImageCreateInfo imageCreateInfo = vkinit::image_create_info(imageFormat, imageUsageFlags, drawImageExtent);

    //Create the Image Allocation Info
    VmaAllocationCreateInfo allocationCreateInfo = {};
    allocationCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocationCreateInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    //Create the image with allocation in GPU memory
    vmaCreateImage(_allocator, &imageCreateInfo, &allocationCreateInfo, &_drawImage._image, &_drawImage._allocation, nullptr);

    //Create the ImageView
    VkImageViewCreateInfo imageViewCreateInfo = vkinit::imageview_create_info(imageFormat, _drawImage._image, VK_IMAGE_ASPECT_COLOR_BIT);
    VK_CHECK(vkCreateImageView(_device, &imageViewCreateInfo, nullptr, &_drawImage._imageView));

    //Create Depth Image with it's allocation
    imageFormat = VK_FORMAT_D32_SFLOAT; //Depth will consist of a single float variable
    _depthImage._format = imageFormat;
    _depthImage._extent = drawImageExtent;
    //Set the depth image usage flags to be used as depth attachment
    imageUsageFlags = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    //Create image creation info
    imageCreateInfo = vkinit::image_create_info(imageFormat, imageUsageFlags, drawImageExtent);
    //Use same vma alloaction create info to create depth image allocation
    vmaCreateImage(_allocator, &imageCreateInfo, &allocationCreateInfo, &_depthImage._image, &_depthImage._allocation, nullptr);
    //Create Depth Image View
    imageViewCreateInfo = vkinit::imageview_create_info(imageFormat, _depthImage._image, VK_IMAGE_ASPECT_DEPTH_BIT);
    VK_CHECK(vkCreateImageView(_device, &imageViewCreateInfo, nullptr, &_depthImage._imageView));

    //Add destructors for created Image and ImageView to main deletion Queue (Image View First, since it was last created)
    _mainDeletionQueue.addDeletor([&]()
        {
            vkDestroyImageView(_device, _drawImage._imageView, nullptr);
            vkDestroyImageView(_device, _depthImage._imageView, nullptr);
        });
    _mainDeletionQueue.addDeletor([&]()
        {
            vmaDestroyImage(_allocator, _drawImage._image, _drawImage._allocation);
            vmaDestroyImage(_allocator, _depthImage._image, _depthImage._allocation);
        });
}

void VulkanEngine::init_Commands()
{
    //Define Command Pool Creation Info and use Vulkan to create a command pool with the desired params
    VkCommandPoolCreateInfo commandPoolCreateInfo = vkinit::command_pool_create_info(_commandsQueueFamilyIndex, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    //Create Command Pool and allocate a Buffer for each Frame
    for (int i = 0; i < FRAME_COUNT; i++)
    {
        VK_CHECK(vkCreateCommandPool(_device, &commandPoolCreateInfo, nullptr, &_frames[i]._commandPool));  //use VK_CHECK to make sure the Command Pool was successfully created

        //Define Command Buffer allocator info struct and use Vulkan to allocate a command buffer from created Command Pool
        VkCommandBufferAllocateInfo commandBufferAllocatorInfo = vkinit::command_buffer_allocate_info(_frames[i]._commandPool, 1);

        VK_CHECK(vkAllocateCommandBuffers(_device, &commandBufferAllocatorInfo, &_frames[i]._mainCommandBuffer));
    }

    //Initialize the Command Pool for Immediate commands, use it to allocate immediate commands buffer
    VK_CHECK(vkCreateCommandPool(_device, &commandPoolCreateInfo, nullptr, &_immCmdPool));

    VkCommandBufferAllocateInfo immCmdBufferAllocateInfo = vkinit::command_buffer_allocate_info(_immCmdPool, 1);

    VK_CHECK(vkAllocateCommandBuffers(_device, &immCmdBufferAllocateInfo, &_immCmdBuffer));

    //Add the Created Pool to Deletion Queue, destroying the pool will destroy any buffer it allocated
    _mainDeletionQueue.addDeletor([=]()
    {
        vkDestroyCommandPool(_device, _immCmdPool, nullptr);
    });
}

void VulkanEngine::init_Sync_Structures()
{
    //Use VK_Initializers to create semaphore and fence creation structs
    VkSemaphoreCreateInfo semaphore_Create_Info = vkinit::semaphore_create_info();
    VkFenceCreateInfo fence_Create_Info = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT); //Create the Fence signaled, so first frame with no GPU work won't stall the application forever
    //Create 2 semaphores and a fence for each overalapping frames
    for (int i = 0; i < FRAME_COUNT; i++)
    {
        VK_CHECK(vkCreateSemaphore(_device, &semaphore_Create_Info, nullptr, &_frames[i]._swapchainSemaphore));
        VK_CHECK(vkCreateSemaphore(_device, &semaphore_Create_Info, nullptr, &_frames[i]._renderSemaphore));

        VK_CHECK(vkCreateFence(_device, &fence_Create_Info, nullptr, &_frames[i]._renderFence));
    }

    //Create Fence for the Immediate Command to halt all CPU execution while the immediate command is being processed on GPU
    VK_CHECK(vkCreateFence(_device, &fence_Create_Info, nullptr, &_immCmdFence));

    //Add the imm fence to deletion queue
    _mainDeletionQueue.addDeletor([=]()
    {
        vkDestroyFence(_device, _immCmdFence, nullptr);
    });
}

void VulkanEngine::init_Descriptors()
{
    //Initialize the Descriptor Pool with maxsets set to 10, each having a single binding of types Storage Image
    std::vector<PoolSizeRatio> poolSizeRatios
    {
        { 
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            1
        }
    };  
    _globalDescriptorAllocator.init(_device, 10, poolSizeRatios);

    //Use DescriptorSetLayoutBuilder to build set layout with a single binding of type storage image
    DescriptorLayoutBuilder layoutBuilder;
    layoutBuilder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    _drawImageDescriptorSetLayout = layoutBuilder.build_Layout(_device, VK_SHADER_STAGE_COMPUTE_BIT);   //Remember to add to deletion queue

    //use allocator to allocate Descriptor set using the generated layout
    _drawImageDescriptors = _globalDescriptorAllocator.allocate(_device, _drawImageDescriptorSetLayout);

    //Use DescriptorSetWriter to write to drawImageDescriptor to draw the background
    DescriptorSetWriter writer;
    writer.writeImage(0, _drawImage._imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    writer.updateSet(_device, _drawImageDescriptors);

    //Create PoolSizeRatios to be used for the initializing of each frame descriptor pool
    std::vector<PoolSizeRatio> poolSizeRatio = 
    {
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4},
    };

    //For each frame data for each frame count, create a growable descriptors pool
    for(int i = 0; i < FRAME_COUNT; i++)
    {
        _frames[i]._descriptorsPool = DescriptorAllocatorGrowable();
        _frames[i]._descriptorsPool.init(_device, 1000, poolSizeRatio);

        //Add the destruction of the pool to the main deletion queue
        _mainDeletionQueue.addDeletor([=, this]()
        {
             _frames[i]._descriptorsPool.destroyPools(_device);
        });
    }

    //Clear Descriptor set Layout Builder and add new single buffer binding
    layoutBuilder.clear();
    layoutBuilder.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);  //Uniform since that data we need is small for each frame scene data
    //Create gpu Scene Data descriptor layout
    _gpuSceneDescriptorSetLayout = layoutBuilder.build_Layout(_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

    //Clear Descriptor set layout Builder and bind combined image with sampler
    layoutBuilder.clear();
    layoutBuilder.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);    //We will send both the image and it's sampler to the shader
    //Create testTexture descriptor set layout
    _testTextureDescriptorSetLayout = layoutBuilder.build_Layout(_device, VK_SHADER_STAGE_FRAGMENT_BIT);

    //Add create Descriptor Set layout and descriptor allocation pool to deletion queue (Destroying the pool will destroy any allocated sets)
    _mainDeletionQueue.addDeletor([&]()
    {
        _globalDescriptorAllocator.destroyPools(_device);
        vkDestroyDescriptorSetLayout(_device, _drawImageDescriptorSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(_device, _gpuSceneDescriptorSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(_device, _testTextureDescriptorSetLayout, nullptr);
    });
}

void VulkanEngine::init_Pipelines()
{
    //Compute Pipelines
    init_Pipelines_Background();

    //Graphics Pipelines
    init_triangle_Pipeline();
    init_mesh_Pipeline();
}

void VulkanEngine::init_imgui()
{
    //Create Descriptor Set Pool for ImGui
    //  the size of the pool is very oversize, but it's copied from imgui demo
	//  itself.
	std::vector<VkDescriptorPoolSize> pool_sizes = 
    { 
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 } 
    };

    VkDescriptorPoolCreateInfo imguiDescriptorPoolCreateInfo = {};
    imguiDescriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    imguiDescriptorPoolCreateInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    imguiDescriptorPoolCreateInfo.pNext = nullptr;
    imguiDescriptorPoolCreateInfo.maxSets = 1000; //Probably an overkill, copied from Imgui vulkan demo
    imguiDescriptorPoolCreateInfo.poolSizeCount = (uint32_t)pool_sizes.size();
    imguiDescriptorPoolCreateInfo.pPoolSizes = pool_sizes.data();

    VkDescriptorPool imguiDescriptorPool;
    VK_CHECK(vkCreateDescriptorPool(_device, &imguiDescriptorPoolCreateInfo, nullptr, &imguiDescriptorPool));

    //Initialize ImGui
    //Init ImGui engine
    ImGui::CreateContext();

    //Init ImGui for SDL
    ImGui_ImplSDL2_InitForVulkan(_window);

    //Init ImGui for Vulkan
    //Creat ImGui Implement Vulkan Info
    ImGui_ImplVulkan_InitInfo imguiVulkanInitInfo = {};
    imguiVulkanInitInfo.Instance = _instance;
    imguiVulkanInitInfo.PhysicalDevice = _physicalDevice;
    imguiVulkanInitInfo.Device = _device;
    imguiVulkanInitInfo.QueueFamily = _commandsQueueFamilyIndex;
    imguiVulkanInitInfo.Queue = _commandsQueue;
    imguiVulkanInitInfo.DescriptorPool = imguiDescriptorPool;
    imguiVulkanInitInfo.MinImageCount = 3;
    imguiVulkanInitInfo.ImageCount = 3;
    imguiVulkanInitInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    //Add options for dynamic rendering
    imguiVulkanInitInfo.UseDynamicRendering = true;
    imguiVulkanInitInfo.PipelineRenderingCreateInfo =
    {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext = nullptr,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &_swapchainImageFormat
    };

    ImGui_ImplVulkan_Init(&imguiVulkanInitInfo);

    //Init ImGui font textures for Vulkan
    ImGui_ImplVulkan_CreateFontsTexture();  //No need to call deletion for the created Textures, since it's already handled by this function

    //Shutdown ImGui, and Add the Created Descriptor Pool to deletion queue
    _mainDeletionQueue.addDeletor([=]()
    {
       ImGui_ImplVulkan_Shutdown();
       vkDestroyDescriptorPool(_device, imguiDescriptorPool, nullptr); 
    });
}

void VulkanEngine::init_triangle_Pipeline()
{
    //Create 2 Shader modules for the vertex and fragment shader
    VkShaderModule vertexShader;
    if(!vkutil::load_Shader_Module(SHADER_PATH "/colored_triangle.vert.spv", _device, &vertexShader))
    {
        fmt::println("Failed to load vertex Shader: {}", SHADER_PATH "/colored_triangle.vert.spv");
        return;
    }
    VkShaderModule fragShader;
    if(!vkutil::load_Shader_Module(SHADER_PATH "/colored_triangle.frag.spv", _device, &fragShader))
    {
        fmt::println("Failed to load fragment Shader: {}", SHADER_PATH "/colored_triangle.frag.spv");
        return;
    }

    //Create Pipeline Builder, fill it's parameter to create the graphics pipeline
    PipelineBuilder graphicsPipelineBuilder;
    graphicsPipelineBuilder.addShaderStages(vertexShader, fragShader);
    graphicsPipelineBuilder.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    graphicsPipelineBuilder.SetPolygonMode(VK_POLYGON_MODE_FILL);
    graphicsPipelineBuilder.SetCullingMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE); //Don't cull any face, and set the orientation clockwise (LHO)
    graphicsPipelineBuilder.setMultisampleNone();
    //graphicsPipelineBuilder.disableDepthTest();
    graphicsPipelineBuilder.enableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    graphicsPipelineBuilder.disableBlending();
    graphicsPipelineBuilder.setColorAttachmentFormat(_drawImage._format);
    graphicsPipelineBuilder.setDepthFormat(_depthImage._format);
    //Create Pipeline layout using initializer info, and set it in the pipeline builder
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vkinit::pipeline_layout_create_info();
    VK_CHECK(vkCreatePipelineLayout(_device, &pipelineLayoutCreateInfo, nullptr, &_trianglePipelineLayout));
    graphicsPipelineBuilder._pipelineLayout = _trianglePipelineLayout; 

    //use the Builder to create the pipeline
    _trianglePipeline = graphicsPipelineBuilder.build_pipeline(_device);
    if(_trianglePipeline == VK_NULL_HANDLE)
    {
        fmt::println("Failed to create graphics pipeline, using null handle");
        return;
    }

    //Destroy the created shader modules
    vkDestroyShaderModule(_device, vertexShader, nullptr);
    vkDestroyShaderModule(_device, fragShader, nullptr);

    //Add the created triangle pipeline layout and pipeline to deletion queue
    _mainDeletionQueue.addDeletor([=]()
    {
        vkDestroyPipeline(_device, _trianglePipeline, nullptr);
        vkDestroyPipelineLayout(_device, _trianglePipelineLayout, nullptr);
    });
}

void VulkanEngine::init_mesh_Pipeline()
{
    //Create 2 Shader modules for the vertex and fragment shader
    VkShaderModule vertexShader;
    if(!vkutil::load_Shader_Module(SHADER_PATH "/colored_mesh.vert.spv", _device, &vertexShader))
    {
        fmt::println("Failed to load vertex Shader: {}", SHADER_PATH "/colored_mesh.vert.spv");
        return;
    }
    VkShaderModule fragShader;
    if(!vkutil::load_Shader_Module(SHADER_PATH "/colored_triangle.frag.spv", _device, &fragShader))
    {
        fmt::println("Failed to load fragment Shader: {}", SHADER_PATH "/colored_triangle.frag.spv");
        return;
    }

    //Create Pipeline Builder, fill it's parameter to create the graphics pipeline
    PipelineBuilder graphicsPipelineBuilder;
    graphicsPipelineBuilder.addShaderStages(vertexShader, fragShader);
    graphicsPipelineBuilder.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    graphicsPipelineBuilder.SetPolygonMode(VK_POLYGON_MODE_FILL);
    graphicsPipelineBuilder.SetCullingMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE); //Don't cull any face, and set the orientation clockwise (LHO)
    graphicsPipelineBuilder.setMultisampleNone();
    //graphicsPipelineBuilder.disableDepthTest();
    graphicsPipelineBuilder.enableDepthTest(true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    graphicsPipelineBuilder.disableBlending();
    //graphicsPipelineBuilder.enableBlending(false);
    graphicsPipelineBuilder.setColorAttachmentFormat(_drawImage._format);
    graphicsPipelineBuilder.setDepthFormat(_depthImage._format);

    //Create Pipeline layout using initializer info, and set it in the pipeline builder
    VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vkinit::pipeline_layout_create_info();

    //Add the Push Constant Buffer range and test texture descriptor set layout to the pipeline create info
    VkPushConstantRange pushConstantRange = {};
    pushConstantRange.size = sizeof(GPUDrawPushConstants);
    pushConstantRange.offset = 0;
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
    pipelineLayoutCreateInfo.setLayoutCount = 1;
    pipelineLayoutCreateInfo.pSetLayouts = &_testTextureDescriptorSetLayout;

    VK_CHECK(vkCreatePipelineLayout(_device, &pipelineLayoutCreateInfo, nullptr, &_meshPipelineLayout));
    graphicsPipelineBuilder._pipelineLayout = _meshPipelineLayout; 

    //use the Builder to create the pipeline
    _meshPipeline = graphicsPipelineBuilder.build_pipeline(_device);
    if(_meshPipeline == VK_NULL_HANDLE)
    {
        fmt::println("Failed to create mesh draw pipeline, using null handle");
        return;
    }

    //Destroy the created shader modules
    vkDestroyShaderModule(_device, vertexShader, nullptr);
    vkDestroyShaderModule(_device, fragShader, nullptr);

    //Add the created triangle pipeline layout and pipeline to deletion queue
    _mainDeletionQueue.addDeletor([=]()
    {
        vkDestroyPipeline(_device, _meshPipeline, nullptr);
        vkDestroyPipelineLayout(_device, _meshPipelineLayout, nullptr);
    });
}

//Function used to init the Values for the Vertex and Index buffer for the drawn mesh (Default Values)
void VulkanEngine::init_Default_Values()
{
    //Initial values for the Vertex and Index buffers
    std::vector<Vertex> vertices = 
    {
        {.position = glm::vec3(0.5f, -0.5f, 0.0f), .color = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f)},     //0
        {.position = glm::vec3(0.5f, 0.5f, 0.0f), .color = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f)},      //1
        {.position = glm::vec3(-0.5f, -0.5f, 0.0f), .color = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f)},    //2
        {.position = glm::vec3(-0.5f, 0.5f, 0.0f), .color = glm::vec4(1.0f, 1.0f, 0.0f, 1.0f)},     //3
    };

    std::array<uint32_t, 6> indices = 
    {
        //First Triangle
        0, 1, 2,
        //Second Triangle
        2, 1, 3
    };

    //use the Containers to create mesh Draw Buffers
    _meshBuffers = uploadMesh(vertices, indices);

    //Create Images for each default texture colors
    uint32_t white = glm::packUnorm4x8(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    _whiteTex = createImage((void*)&white, VkExtent3D(1 ,1 ,1), VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
    uint32_t grey = glm::packUnorm4x8(glm::vec4(0.6f, 0.6f, 0.6f, 1.0f));
    _greyTex = createImage((void*)&grey, VkExtent3D(1 ,1 ,1), VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
    uint32_t black = glm::packUnorm4x8(glm::vec4(0.0f, 0.0f, 0.0f, 0.0f));
    _blackTex = createImage((void*)&black, VkExtent3D(1 ,1 ,1), VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
    uint32_t magenta = glm::packUnorm4x8(glm::vec4(1.0f, 0.0f, 1.0f, 1.0f));
    std::vector<uint32_t> checkersPixels;
    checkersPixels.resize(16*16);
    for(int y = 0; y < 16; y++)
    {
        for(int x = 0; x < 16;  x++)
        {
            checkersPixels[y * 16 + x] = ((x % 2 == 0) ^ (y % 2 == 0)) ? black : magenta;
        }
    }
    _errorCheckerBoard = createImage((void*)checkersPixels.data(), VkExtent3D(16, 16, 1),VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, VMA_MEMORY_USAGE_GPU_ONLY); 

    //Create samplers for linear and nearest min and mag filtering
    VkSamplerCreateInfo samplerCreateInfo = {};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.pNext = nullptr;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    vkCreateSampler(_device, &samplerCreateInfo, nullptr, &_defaultSamplerLinear);
    samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
    samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
    vkCreateSampler(_device, &samplerCreateInfo, nullptr, &_defaultSamplerNearest);

    //Add the created default samplers and tex images to deletion queue
    _mainDeletionQueue.addDeletor([&]()
    {
        vkDestroySampler(_device, _defaultSamplerNearest, nullptr);
        vkDestroySampler(_device, _defaultSamplerLinear, nullptr);

        destroyImage(_whiteTex);
        destroyImage(_greyTex);
        destroyImage(_blackTex);
        destroyImage(_errorCheckerBoard);
    });

    //Add both buffers to deletion queue
    _mainDeletionQueue.addDeletor([&]()
    {
       destroyBuffer(_meshBuffers.vertexBuffer);
       destroyBuffer(_meshBuffers.indexBuffer); 
    });
}

void VulkanEngine::init_Loaded_Mesh()
{
    //Get the test meshes using LoadMeshFromFile util funtion
    _testMeshes = vkutil::loadMeshFromFile(*this, ASSET_PATH "/basicmesh.glb").value_or(std::vector<std::shared_ptr<MeshAsset>>{});
    //Add buffers from each mesh to deletion queue
    _mainDeletionQueue.addDeletor([&]()
    {
        for(std::shared_ptr<MeshAsset> mesh : _testMeshes)
        {
            destroyBuffer(mesh->meshBuffers.indexBuffer);
            destroyBuffer(mesh->meshBuffers.vertexBuffer);
        }
    });
}

void VulkanEngine::init_Pipelines_Background()
{
    //Use the VKUtils to create Shader Module using the gradient shader path
    VkShaderModule gradientShaderModule;
    if(!vkutil::load_Shader_Module(SHADER_PATH "/gradient_color.comp.spv", _device, &gradientShaderModule))
    {
        fmt::print("Failed to load shader module at path: {}", SHADER_PATH "/gradient_color.comp.spv");
        return;
    }
    VkShaderModule skyShaderModule;
    if(!vkutil::load_Shader_Module(SHADER_PATH "/sky.comp.spv", _device, &skyShaderModule))
    {
        fmt::print("Failed to load shader module at path: {}", SHADER_PATH "/sky.comp.spv");
        return;
    }

    //Initialize Push Constants Range
    VkPushConstantRange pushConstantRange = {};
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(ComputePushConstants);
    pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    //Use Pipeline Layout Create Info to create layout for the Shader Pipeline using the Descriptor Set layout
    VkPipelineLayoutCreateInfo pipelineCreateInfo = {};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineCreateInfo.pNext = nullptr;
    pipelineCreateInfo.setLayoutCount = 1;
    pipelineCreateInfo.pSetLayouts = &_drawImageDescriptorSetLayout;
    pipelineCreateInfo.pushConstantRangeCount = 1;
    pipelineCreateInfo.pPushConstantRanges = &pushConstantRange;

    VK_CHECK(vkCreatePipelineLayout(_device, &pipelineCreateInfo, nullptr, &_gradientPipelineLayout));

    //Create Compute Shader Pipeline using Shader stage info, and pipeline layout
    VkPipelineShaderStageCreateInfo pipelineShaderCreateInfo = {};
    pipelineShaderCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineShaderCreateInfo.pNext = nullptr;
    pipelineShaderCreateInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipelineShaderCreateInfo.module = gradientShaderModule;
    pipelineShaderCreateInfo.pName = "main";    //Main entry point in the shader, can be used with shader with multiple entry points to determine which one to be used

    VkComputePipelineCreateInfo computePipelineCreateInfo = {};
    computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    computePipelineCreateInfo.pNext = nullptr;
    computePipelineCreateInfo.stage = pipelineShaderCreateInfo;
    computePipelineCreateInfo.layout = _gradientPipelineLayout;

    //Create 2 Compute Effects for each shader module used
    //Initialize the Compute Effect for the gradient shader, create pipeline with new create info
    ComputeEffect gradientEffect = {};
    gradientEffect.name = "gradient";
    gradientEffect.pipelineLayout = _gradientPipelineLayout;
    gradientEffect.pc_data = {
        .data1 = glm::vec4(1.0f, 0.0f, 0.0f, 1.0f),
        .data2 = glm::vec4(0.0f, 0.0f, 1.0f, 1.0f)
    };
    VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &gradientEffect.pipeline));
    backgroundEffects.push_back(gradientEffect);

    //Initialize the Compute Effect for the gradient shader, create pipeline with new create info
    ComputeEffect skyEffect = {};
    skyEffect.name = "sky";
    skyEffect.pipelineLayout = _gradientPipelineLayout;
    skyEffect.pc_data = {};
    computePipelineCreateInfo.stage.module = skyShaderModule;
    VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &skyEffect.pipeline));
    backgroundEffects.push_back(skyEffect);

    //Destroy shader modules, add the pipeline and pipeline layout objects to deletion queue
    vkDestroyShaderModule(_device, gradientShaderModule, nullptr);
    vkDestroyShaderModule(_device, skyShaderModule, nullptr);

    _mainDeletionQueue.addDeletor([&]()
    {
        for(auto& computeEffect : backgroundEffects)
        {        
            vkDestroyPipeline(_device, computeEffect.pipeline, nullptr);
        }
        vkDestroyPipelineLayout(_device, _gradientPipelineLayout, nullptr);
    });
}

void VulkanEngine::create_Swapchain(uint32_t width, uint32_t height)
{
    //Use VKBootstrap to create a swapchain builder with the correct config
    vkb::SwapchainBuilder vkb_swapchain_builder{ _physicalDevice, _device, _surface };
    _swapchainImageFormat = VK_FORMAT_R8G8B8A8_UNORM;
    vkb::Swapchain vkb_Swapchain = vkb_swapchain_builder
        .set_desired_format(VkSurfaceFormatKHR{ .format = _swapchainImageFormat , .colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR }) //Set the Format of the swapchain images to RGBA8
        .set_desired_extent(width, height)                                                                                          //Set the Extent of the swapchain image to be the same as the window extent
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)                                                                         //Use hard VSYNC for swapchain, forcing FPS to Screen RR
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)                                                                     //Set the Swapchain image as destination for computed data in GPU Buffer
        .build().value();

    //Set Swapchain variables values using the Swapchain Builder result
    _swapchain = vkb_Swapchain.swapchain;
    _swapchainImageExtent2D = vkb_Swapchain.extent;
    _swapchain_Images = vkb_Swapchain.get_images().value();
    _swapchain_Image_Views = vkb_Swapchain.get_image_views().value();
}

void VulkanEngine::destroy_Swapchain()
{
    //Use VK Function to destroy the created Swapchain
    vkDestroySwapchainKHR(_device, _swapchain, nullptr);
    //Images are destroyed with Swapchain, but image views need to be destroyed explicitly
    for (auto i = _swapchain_Image_Views.begin(); i != _swapchain_Image_Views.end(); i++)
    {
        vkDestroyImageView(_device, *i, nullptr);
    }
}

void VulkanEngine::resize_Swapchain()
{
    //Wait for the GPU operations to compelete before destroying the old swapchain
    vkDeviceWaitIdle(_device);

    //destroy the old Swapchain
    destroy_Swapchain();

    //query new window size from SLD Window
    int w, h;
    SDL_GetWindowSize(_window, &w, &h);
    _windowExtent.width = w;
    _windowExtent.height = h;
    create_Swapchain(_windowExtent.width, _windowExtent.height);

    //Set the Resize window variable to false
    resize_Window = false;
}

void VulkanEngine::cleanup()
{
    if (_isInitialized)
    {
        //Check for the GPU to have finished any executing operaions
        vkDeviceWaitIdle(_device);

        //Destroy all created components in the reverse order of creation
        //Destroy each Command Buffer by destroying the Command Buffer used to allocate (Can't destroy Commands Queue, since its something that's already there provided by VKInstance and not created)
        for (int i = 0; i < FRAME_COUNT; i++)
        {
            vkDestroyCommandPool(_device, _frames[i]._commandPool, nullptr);

            //Destroy each framedata semaphores and fence
            vkDestroySemaphore(_device, _frames[i]._renderSemaphore, nullptr);
            vkDestroySemaphore(_device, _frames[i]._swapchainSemaphore, nullptr);
            vkDestroyFence(_device, _frames[i]._renderFence, nullptr);

            //Clean all vulkan objects in the frame deletion queue
            _frames[i]._frameDeletionQueue.flush();
        }

        //Clear Engine main Deletion Queue
        _mainDeletionQueue.flush();

        //Destroy Swapchain
        destroy_Swapchain();
        //Destroy Device(Can't destroy PD since it's a handle to the physical GPU)
        vkDestroyDevice(_device, nullptr);
        //Destroy Surface
        vkDestroySurfaceKHR(_instance, _surface, nullptr);
        //Destroy Debug Messanger using VKB
        vkb::destroy_debug_utils_messenger(_instance, _debug_Messanger, nullptr);
        //Destroy Vulkan Instance
        vkDestroyInstance(_instance, nullptr);
        //Destroy created SDL Window
        SDL_DestroyWindow(_window);
    }

    // clear engine pointer
    loadedEngine = nullptr;
}

void VulkanEngine::submit_Immediate_Command(std::function<void(VkCommandBuffer cmd)> &&function)
{
    //For simplicity assign the immediate submit command handle to simpler variable
    VkCommandBuffer cmd = _immCmdBuffer;

    //Reset The immediate command buffer and fence to begin adding new commands
    VK_CHECK(vkResetFences(_device, 1, &_immCmdFence));
    VK_CHECK(vkResetCommandBuffer(cmd, 0));

    //Create Command Buffer Begin
    VkCommandBufferBeginInfo immCmdBufferBeginInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);    //Use this flag to tell Vulkan this command buffer is for one time use, good for optimization
    VK_CHECK(vkBeginCommandBuffer(cmd, &immCmdBufferBeginInfo));

    //Use the function to call the immediate commands to be added to the buffer
    function(cmd);

    //End the Command buffer, no new commands to be added
    VK_CHECK(vkEndCommandBuffer(cmd));

    //Create Buffer Submit Info to submit the buffer to commands queue
    VkCommandBufferSubmitInfo immCmdBufferSubmitInfo = vkinit::command_buffer_submit_info(cmd);

    //Use Submit info with no signal or wait semaphore used
    VkSubmitInfo2 immCommandSubmitInfo = vkinit::submit_info(&immCmdBufferSubmitInfo, nullptr, nullptr);

    //Submit the Command Buffer
    VK_CHECK(vkQueueSubmit2(_commandsQueue, 1, &immCommandSubmitInfo, _immCmdFence));

    //Wait for the Immediate Command to be executed in the Command Queue
    VK_CHECK(vkWaitForFences(_device, 1, &_immCmdFence, VK_TRUE, 9999999999));
}

void VulkanEngine::draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView)
{
    //Create Render Attachment info and Render info to start rendering imgui on Swapchain imageview
    VkRenderingAttachmentInfo imguiRenderAttachmentInfo = vkinit::attachment_info(targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingInfo imguiRenderingInfo = vkinit::rendering_info(_swapchainImageExtent2D, &imguiRenderAttachmentInfo, nullptr);

    //Start rendering command
    vkCmdBeginRendering(cmd, &imguiRenderingInfo);

    //draw Imgui on swapchain view using ImGui Vulkan Implementation
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd, nullptr);

    //End rendering Command
    vkCmdEndRendering(cmd);
}

void VulkanEngine::draw()
{
    //Wait for the Render Fence to finish executing the current command in the Buffer, then reset it
    VK_CHECK(vkWaitForFences(_device, 1, &GetCurrentFrameData()._renderFence, true, 1000000000)); //Set wait timeout to 1 billion nano seconds = 1 seconds
    VK_CHECK(vkResetFences(_device, 1, &GetCurrentFrameData()._renderFence));
    //Clean all objects in currentframe deletors
    GetCurrentFrameData()._frameDeletionQueue.flush();
    //Clear all allocated descriptor sets by the current frame descriptor pool
    GetCurrentFrameData()._descriptorsPool.resetPools(_device);

    //Acquire an Image with index from the swapchain with timeout set to 1 second
    uint32_t swapchainImageIndex;
    VkResult acquireResult = vkAcquireNextImageKHR(_device, _swapchain, 1000000000, GetCurrentFrameData()._swapchainSemaphore, nullptr, &swapchainImageIndex); //No fence needed since this operation shouldn't stall the CPU
    if(acquireResult == VK_ERROR_OUT_OF_DATE_KHR)   //If out of date error, window resized
    {
        resize_Window = true;
        return;
    }

    //Update the DrawExtent based on current swapchain size and render scale
    _drawExtent.width = (uint32_t)(glm::min(_swapchainImageExtent2D.width, _drawImage._extent.width) * _renderScale);
    _drawExtent.height = (uint32_t)(glm::min(_swapchainImageExtent2D.height, _drawImage._extent.height) * _renderScale);

    //Begin the command buffer so we can start adding commands for submissiong
    VkCommandBuffer cmd = GetCurrentFrameData()._mainCommandBuffer;

    //Reset the command buffer
    VK_CHECK(vkResetCommandBuffer(cmd, 0));

    //Use VKInit to create a command buffer begin info struct and use to begin the commnad buffer
    VkCommandBufferBeginInfo command_Buffer_Begin_Info = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT); //Use this flag to tell Vulkan this command buffer is for one time use, good for optimization

    VK_CHECK(vkBeginCommandBuffer(cmd, &command_Buffer_Begin_Info));

    //Transition the DrawImage layout from it's current layout to General for drawing into (General isn't the best for rastarization, but will do since we will be using compute rendering)
    vkutil::transition_Image(cmd, _drawImage._image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    //Non standard draw commands in Draw_Background function
    draw_Background(cmd);

    //Transition the Draw Image from general to Color Attachment to render on it using Graphics Pipeline
    vkutil::transition_Image(cmd, _drawImage._image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    //Transition the Depth Image from it's current layout to Depth Attachment to be used in depth testing
    vkutil::transition_Image(cmd, _depthImage._image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    //Render using Graphics Pipeline
    draw_Geometry(cmd);

    //Transition the DrawImage from Color Attachment to transfer source, to be used later to draw on the swapchain image
    vkutil::transition_Image(cmd, _drawImage._image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    //Transition the swapchain Image to transfer destination
    vkutil::transition_Image(cmd, _swapchain_Images[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    //Transition the Image from general to Aspect which can be presented to screen
    vkutil::transition_Image(cmd, _swapchain_Images[swapchainImageIndex], VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    //Copy the drawImage onto the Swapchain Image
    vkutil::copy_Image_to_Image(cmd, _drawImage._image, _swapchain_Images[swapchainImageIndex], _drawExtent, _swapchainImageExtent2D);

    //Transition the Swapchain Image to Color Attachment, so we can draw ImGui on it
    vkutil::transition_Image(cmd, _swapchain_Images[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    //Draw the ImGui window on the swapchain image view
    draw_imgui(cmd, _swapchain_Image_Views[swapchainImageIndex]);

    //Transition the swapchain Image to Presentable layout so it can be displayed on the window
    vkutil::transition_Image(cmd, _swapchain_Images[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    //End the command buffer, no more commands will be added
    VK_CHECK(vkEndCommandBuffer(cmd));

    //Create command buffer submit info
    VkCommandBufferSubmitInfo command_Submit_Info = vkinit::command_buffer_submit_info(cmd);

    //Creat two semaphore submit info, using the swapchain as the wait semaphore, amd the render as the signal semaphore
    VkSemaphoreSubmitInfo wait_Semaphore_Info = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, GetCurrentFrameData()._swapchainSemaphore);
    VkSemaphoreSubmitInfo signal_Semaphore_Info = vkinit::semaphore_submit_info(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, GetCurrentFrameData()._renderSemaphore);

    //Create Submit Info for the command buffer to be added to the Commands queue
    VkSubmitInfo2 submit_Info = vkinit::submit_info(&command_Submit_Info, &signal_Semaphore_Info, &wait_Semaphore_Info);

    //Submit the Command Buffer
    VK_CHECK(vkQueueSubmit2(_commandsQueue, 1, &submit_Info, GetCurrentFrameData()._renderFence));

    //Prepare the image to be presented to the display window
    VkPresentInfoKHR present_Info = { .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    present_Info.pNext = nullptr;
    present_Info.swapchainCount = 1;
    present_Info.pSwapchains = &_swapchain;
    present_Info.waitSemaphoreCount = 1;
    present_Info.pWaitSemaphores = &GetCurrentFrameData()._renderSemaphore;
    present_Info.pImageIndices = &swapchainImageIndex;

    //Preset the Image to display window
    VkResult presentResult = vkQueuePresentKHR(_commandsQueue, &present_Info);
    if(presentResult == VK_ERROR_OUT_OF_DATE_KHR)
    {
        resize_Window = true;
    }

    //increase the Framenumber
    _frameNumber++;
}

void VulkanEngine::draw_Background(VkCommandBuffer cmd)
{
    //Clear the screen with a black image
    VkClearColorValue clearColor;
    clearColor = {0.1f, 0.1f, 0.1f, 1.0f};
    draw_functions::draw_Clear_Background(cmd, clearColor, _drawImage._image);

    // //Get the Selected Background effect
    // ComputeEffect& backgroundEffect = backgroundEffects[currentActiveBackgroundEffect];
    // //Draw the Selected background effect using compute shader
    // draw_functions::draw_BackgroundEffects(cmd, backgroundEffect, _gradientPipelineLayout, _drawImageDescriptors, _drawExtent);
}

void VulkanEngine::draw_Geometry(VkCommandBuffer cmd)
{
    //Create Render Attachment info and Render info to start rendering on drawImage
    VkRenderingAttachmentInfo renderAttachmentInfo = vkinit::attachment_info(_drawImage._imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo depthAttachmentInfo = vkinit::depth_attachment_info(_depthImage._imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);
    VkRenderingInfo renderingInfo = vkinit::rendering_info(_drawExtent, &renderAttachmentInfo, &depthAttachmentInfo);
    vkCmdBeginRendering(cmd, &renderingInfo);

    //Create Dynamic State for Viewport and Scissor
    VkViewport viewport = {};
    viewport.x = 0;
    viewport.y = 0;
    viewport.width = (uint32_t)_drawExtent.width;
    viewport.height = (uint32_t)_drawExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset.x = 0.0f;
    scissor.offset.y = 0.0f;
    scissor.extent = _drawExtent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    //Create Buffer for current frame scene data to add to descriptor set and bind it to draw command
    AllocatedBuffer sceneDataBuffer = createBuffer(sizeof(GPUSceneData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    //Add the created buffer destroyed to current frame deletion queue
    GetCurrentFrameData()._frameDeletionQueue.addDeletor([=]()
    {
        destroyBuffer(sceneDataBuffer);
    });
    
    //Copy the data from scene data to the created buffer
    memcpy(sceneDataBuffer.allocationInfo.pMappedData, &_gpuSceneData, sizeof(GPUSceneData));

    //Use Current Frame descriptor pool allocator to allocate a new descriptor set for this frame scene data
    VkDescriptorSet gpuSceneDataDescriptorSet = GetCurrentFrameData()._descriptorsPool.allocate(_device, _gpuSceneDescriptorSetLayout);

    //Initialize Descriptor set writer and use it write buffer info into current frame scene data descriptor set
    DescriptorSetWriter writer;
    writer.writeBuffer(0, sceneDataBuffer.buffer, sceneDataBuffer.allocationInfo.size, sceneDataBuffer.allocationInfo.offset, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    writer.updateSet(_device, gpuSceneDataDescriptorSet);

    //Use Current Frame Data Descriptor Pool to allocate a new descriptor set for test texture
    VkDescriptorSet testTextureDescriptorSet = GetCurrentFrameData()._descriptorsPool.allocate(_device, _testTextureDescriptorSetLayout);
    //Use created writer to write image info to the descriptor set
    writer.clear();
    writer.writeImage(0, _errorCheckerBoard._imageView, _defaultSamplerNearest, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    writer.updateSet(_device, testTextureDescriptorSet);

    //Bind the Pipeline to draw the mesh
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _meshPipeline);
    //Bind the descriptor set to the bound pipeline
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _meshPipelineLayout, 0, 1, &testTextureDescriptorSet, 0, nullptr);

    //Draw the third mesh from test meshes loaded from glb file
    //Setup render matrices to render the mesh
    glm::mat4 worldMat = glm::identity<glm::mat4>();
    //Rotate the Mesh around the y axis
    worldMat = glm::rotate(worldMat, glm::radians((float)_frameNumber * 0.25f), glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 viewMat = glm::identity<glm::mat4>();
    viewMat = glm::translate(viewMat, glm::vec3(0.0f, 0.0f, -5.0f));
    glm::mat4 projection = glm::perspective(glm::radians(45.0f), (float)_drawExtent.width / (float)_drawExtent.height, 10000.0f, 0.1f); //We are making the near plane the large value, so near place is at 1 and far plane at 0, this greatly increases depth calc accuracy
    projection[1][1] *= -1; //flip the scale in y direction to make the mesh in the correct orientation
    //Create the push constants needed to draw the mesh
    GPUDrawPushConstants drawPushConstants = {};
    drawPushConstants.worldTransform = projection * viewMat * worldMat;
    drawPushConstants.vertexBufferDeviceAddress = _testMeshes[2]->meshBuffers.vertexBufferDeviceAddress;
    vkCmdPushConstants(cmd, _meshPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GPUDrawPushConstants), &drawPushConstants);
    //Bind the Indices Buffer
    vkCmdBindIndexBuffer(cmd, _testMeshes[2]->meshBuffers.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
    //Launch indexed draw command to draw the surface of the mesh
    vkCmdDrawIndexed(cmd, (uint32_t)_testMeshes[2]->surfaces[0].count, 1, _testMeshes[2]->surfaces[0].startIndex, 0, 0);

    //End rendering command
    vkCmdEndRendering(cmd);
}

void VulkanEngine::run()
{
    SDL_Event e;
    bool bQuit = false;

    // main loop
    while (!bQuit) {
        // Handle events on queue
        while (SDL_PollEvent(&e) != 0)
        {
            // close the window when user alt-f4s or clicks the X button
            if (e.type == SDL_QUIT)
                bQuit = true;

            if (e.type == SDL_WINDOWEVENT) {
                if (e.window.event == SDL_WINDOWEVENT_MINIMIZED) 
                {
                    stop_rendering = true;
                    fmt::println("Screen currently minimized");
                }
                if (e.window.event == SDL_WINDOWEVENT_RESTORED)
                {
                    stop_rendering = false;
                    fmt::println("Screen currently displayed again");
                }
            }
            if(e.type == SDL_KEYDOWN)
            {
                if (e.key.keysym.sym == SDLK_ESCAPE)
                {
                    bQuit = true;
                }
            }

            //Process SDL poll events on ImGui as well
            ImGui_ImplSDL2_ProcessEvent(&e);
        }

        // do not draw if we are minimized
        if (stop_rendering) {
            // throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        //Resize swapchain when window size changes
        if(resize_Window)
        {
            resize_Swapchain();
        }

        //Start a new ImGui frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        //Create Sliders and selections to change between background effects
        // if(ImGui::Begin("Background Effect"))
        // {
        //     ComputeEffect& selectedComputeEffect = backgroundEffects[currentActiveBackgroundEffect];
        //     std::string formatedTextTitle = fmt::format("Selected Effect: {}", selectedComputeEffect.name.c_str());
        //     ImGui::Text(formatedTextTitle.c_str()); //Set the name of the current selected effect
        //     ImGui::SliderInt("Effect Index", &currentActiveBackgroundEffect, 0, backgroundEffects.size() - 1);  //Choose an effect index using slider
        //     //Setting the Push Constants data
        //     ImGui::InputFloat4("Data1", (float*)&selectedComputeEffect.pc_data.data1);
        //     ImGui::InputFloat4("Data2", (float*)&selectedComputeEffect.pc_data.data2);
        //     ImGui::InputFloat4("Data3", (float*)&selectedComputeEffect.pc_data.data3);
        //     ImGui::InputFloat4("Data4", (float*)&selectedComputeEffect.pc_data.data4);
        // }
        // ImGui::End();

        //Create Slider to change render scale for the displayed rendered image
        if(ImGui::Begin("Render Scale"))
        {
            ImGui::SliderFloat("Render Scale Slider Value", &_renderScale, 0.3f, 1.0f);
        }
        ImGui::End();

        //Render the ImGui window
        ImGui::Render();

        draw();
    }
}

AllocatedBuffer VulkanEngine::createBuffer(size_t bufferSize, VkBufferUsageFlags bufferUsage, VmaMemoryUsage memoryUsage)
{
    //Create Buffer Create Info
    VkBufferCreateInfo bufferInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.pNext = nullptr;
    bufferInfo.usage = bufferUsage;
    bufferInfo.size = bufferSize;

    //Create VMA Allocation Create Info
    VmaAllocationCreateInfo allocationCreateInfo = {};
    allocationCreateInfo.usage = memoryUsage;
    allocationCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    //Use Buffer Info and Allocation Info to create buffer and allocate Memory to it
    AllocatedBuffer newBuffer;
    VK_CHECK(vmaCreateBuffer(_allocator, &bufferInfo, &allocationCreateInfo, &newBuffer.buffer, &newBuffer.allocation, &newBuffer.allocationInfo));
    return newBuffer;
}

void VulkanEngine::destroyBuffer(const AllocatedBuffer &buffer)
{
    vmaDestroyBuffer(_allocator, buffer.buffer, buffer.allocation);
}

AllocatedImage VulkanEngine::createImage(VkExtent3D imageExtent, VkFormat imageFormat, VkImageUsageFlags usageFlags, VmaMemoryUsage memUsageFlags, bool bUseMipMap)
{
    AllocatedImage newImage;
    //Set the Format and extent of the Allocated Image
    newImage._format = imageFormat;
    newImage._extent = imageExtent;

    //Create the creation info
    VkImageCreateInfo imageCreateInfo = vkinit::image_create_info(imageFormat, usageFlags, imageExtent);
    if(bUseMipMap)
    {
        imageCreateInfo.mipLevels = std::floor(std::log2(std::max(imageExtent.width, imageExtent.height))) + 1;
    }

    //Create the Image Allocation Info
    VmaAllocationCreateInfo allocationCreateInfo = {};
    allocationCreateInfo.usage = memUsageFlags;
    allocationCreateInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    //Create the image with allocation
    vmaCreateImage(_allocator, &imageCreateInfo, &allocationCreateInfo, &newImage._image, &newImage._allocation, nullptr);

    //Set the Image Aspect Flag based on the Image Format
    VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    if(imageFormat == VK_FORMAT_D32_SFLOAT)
    {
        aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    }
    //Create the ImageView
    VkImageViewCreateInfo imageViewCreateInfo = vkinit::imageview_create_info(imageFormat, newImage._image, aspect);
    imageViewCreateInfo.subresourceRange.levelCount = imageCreateInfo.mipLevels;
    VK_CHECK(vkCreateImageView(_device, &imageViewCreateInfo, nullptr, &newImage._imageView));

    //return the created Image
    return newImage;
}

AllocatedImage VulkanEngine::createImage(void *data, VkExtent3D imageExtent, VkFormat imageFormat, VkImageUsageFlags usageFlags, VmaMemoryUsage memUsageFlags, bool bUseMipMap)
{
    //Create staging buffer to use to copy data to created image
    size_t dataSize = imageExtent.width * imageExtent.height * imageExtent.depth * 4; //4 cause image has rgba elements
    AllocatedBuffer stagingBuffer = createBuffer(dataSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    //Copy data from input data to buffer allocation
    memcpy(stagingBuffer.allocationInfo.pMappedData, data, dataSize);

    //Create new Image with empty data using input parameters
    AllocatedImage newImage = createImage(imageExtent, imageFormat, usageFlags | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, memUsageFlags, bUseMipMap);

    //Use Immediate Command to transfer data from staging buffer to new image
    submit_Immediate_Command([&](VkCommandBuffer cmd)
    {
        //Transition the created image from current layout to transfer destination
        vkutil::transition_Image(cmd, newImage._image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        //Create Buffer Image Copy info with appropriate data
        VkBufferImageCopy newImageCopy = {};
        newImageCopy.bufferOffset = 0;
        newImageCopy.bufferRowLength = 0;
        newImageCopy.bufferImageHeight = 0;
        newImageCopy.imageSubresource.mipLevel = 0;
        newImageCopy.imageSubresource.layerCount = 1;
        newImageCopy.imageSubresource.baseArrayLayer = 0;
        newImageCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        newImageCopy.imageExtent = imageExtent;

        //use Copy command to transfer the data
        vkCmdCopyBufferToImage(cmd, stagingBuffer.buffer, newImage._image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &newImageCopy);

        //Transition the image from it's current layout to shader usage only
        vkutil::transition_Image(cmd, newImage._image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    });

    //Destroy the created staging buffer
    destroyBuffer(stagingBuffer);

    //return the newly created image
    return newImage;
}

void VulkanEngine::destroyImage(const AllocatedImage& image)
{
    //Destroy the image view first then the Image and it's allocation
    vkDestroyImageView(_device, image._imageView, nullptr);
    vmaDestroyImage(_allocator, image._image, image._allocation);
}

GPUMeshBuffers VulkanEngine::uploadMesh(std::span<Vertex> vertices, std::span<uint32_t> indices)
{
    //Get the Sizes of both buffers and use them to create the buffer using createBuffer Function
    size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
    size_t indicesBufferSize = indices.size() * sizeof(uint32_t);

    GPUMeshBuffers newSurfaceBuffers;
    //Create Buffer using flags for storage (since we will make the vertex buffer a storage buffer object), destination flag(since we will memcpy from staging buffer), device address
    //Set memory usage to GPU only, since we won't be reading data from main program only on shaders
    newSurfaceBuffers.vertexBuffer = createBuffer(vertexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
    
    //Init Buffer Device Address info to get the Device address of created vertex buffer
    VkBufferDeviceAddressInfo deviceAddressInfo = {.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = newSurfaceBuffers.vertexBuffer.buffer};
    newSurfaceBuffers.vertexBufferDeviceAddress = vkGetBufferDeviceAddress(_device, &deviceAddressInfo);

    //Create Index Buffer using Index Flag, destination Flag
    newSurfaceBuffers.indexBuffer = createBuffer(indicesBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

    //Create Staging Buffer being able to read from CPU to copy buffer to GPUMeshBuffers
    AllocatedBuffer stagingBuffer = createBuffer(vertexBufferSize + indicesBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    char* stagingBufferData = (char*)stagingBuffer.allocationInfo.pMappedData;

    //Copy data from Vertex and Index arrays to staging Buffer Data
    memcpy(stagingBufferData, vertices.data(), vertexBufferSize);
    memcpy(stagingBufferData + vertexBufferSize, indices.data(), indicesBufferSize);

    //Use Immediate Command to Copy the Data from staging buffer to appropriate buffers
    submit_Immediate_Command([&](VkCommandBuffer cmd)
    {
        VkBufferCopy vertexBufferCopy = {};
        vertexBufferCopy.srcOffset = 0;
        vertexBufferCopy.dstOffset = 0;
        vertexBufferCopy.size = vertexBufferSize;
        vkCmdCopyBuffer(cmd, stagingBuffer.buffer, newSurfaceBuffers.vertexBuffer.buffer, 1, &vertexBufferCopy);

        VkBufferCopy indexBufferCopy = {};
        indexBufferCopy.srcOffset = vertexBufferSize;
        indexBufferCopy.dstOffset = 0;
        indexBufferCopy.size = indicesBufferSize;
        vkCmdCopyBuffer(cmd, stagingBuffer.buffer, newSurfaceBuffers.indexBuffer.buffer, 1, &indexBufferCopy);
    });

    //Destroy The created staging buffer
    destroyBuffer(stagingBuffer);

    return newSurfaceBuffers;
}
