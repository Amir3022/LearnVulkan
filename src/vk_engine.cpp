//> includes
#include "vk_engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk_initializers.h>
#include <vk_images.h>
#include <vk_pipelines.h>

//Headers for Imgui
#include "imgui.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_vulkan.h"

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
    _windowExtent = { 800 , 600 };
    _window = nullptr;
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

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN);

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

    //Create the Draw Image Used to draw on
    VkExtent3D drawImageExtent
    {
        _windowExtent.width,
        _windowExtent.height,
        1
    };

    VkFormat imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
    //Set the Format and extent of the Allocated Image
    _drawImage._format = imageFormat;
    _drawImage._extent = { drawImageExtent.width, drawImageExtent.height };
    _drawExtent = _drawImage._extent;
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

    //Add destructors for created Image and ImageView to main deletion Queue (Image View First, since it was last created)
    _mainDeletionQueue.addDeletor([&]()
        {
            vkDestroyImageView(_device, _drawImage._imageView, nullptr);
        });
    _mainDeletionQueue.addDeletor([&]()
        {
            vmaDestroyImage(_allocator, _drawImage._image, _drawImage._allocation);
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
    std::vector<DescriptorAllocator::PoolSizeRatio> poolSizeRatios
    {
        { 
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            1
        }
    };  
    GlobalDescriptorAllocator.init_pool(_device, 10, poolSizeRatios);

    //Use DescriptorSetLayoutBuilder to build set layout with a single binding of type storage image
    DescriptorLayoutBuilder layoutBuilder;
    layoutBuilder.add_binding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
    _drawImageDescriptorSetLayout = layoutBuilder.build_Layout(_device, VK_SHADER_STAGE_COMPUTE_BIT);   //Remember to add to deletion queue

    //use allocator to allocate Descriptor set using the generated layout
    _drawImageDescriptors = GlobalDescriptorAllocator.allocate(_device, _drawImageDescriptorSetLayout);

    //Create Image info from Draw Image used to write on
    VkDescriptorImageInfo drawImageInfo = {};
    drawImageInfo.imageView = _drawImage._imageView;
    drawImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    //Create Write Descriptor to update the bound descriptor set with image info
    VkWriteDescriptorSet drawImageWrite = {};
    drawImageWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    drawImageWrite.pNext = nullptr;
    drawImageWrite.dstSet = _drawImageDescriptors;
    drawImageWrite.dstBinding = 0;
    drawImageWrite.descriptorCount = 1;
    drawImageWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    drawImageWrite.pImageInfo = &drawImageInfo;

    vkUpdateDescriptorSets(_device, 1, &drawImageWrite, 0, nullptr);

    //Add create Descriptor Set layout and descriptor allocation pool to deletion queue (Destroying the pool will destroy any allocated sets)
    _mainDeletionQueue.addDeletor([&]()
    {
        GlobalDescriptorAllocator.destroy_pool(_device);
        vkDestroyDescriptorSetLayout(_device, _drawImageDescriptorSetLayout, nullptr);
    });
}

void VulkanEngine::init_Pipelines()
{
    init_Pipelines_Background();
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

void VulkanEngine::init_Pipelines_Background()
{
    //Use Pipeline Layout Create Info to create layout for the Shader Pipeline using the Descriptor Set layout
    VkPipelineLayoutCreateInfo pipelineCreateInfo = {};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineCreateInfo.pNext = nullptr;
    pipelineCreateInfo.setLayoutCount = 1;
    pipelineCreateInfo.pSetLayouts = &_drawImageDescriptorSetLayout;

    VK_CHECK(vkCreatePipelineLayout(_device, &pipelineCreateInfo, nullptr, &_gradientPipelineLayout));

    //Use the VKUtils to create Shader Module using the gradient shader path
    VkShaderModule gradientShaderModule;
    if(!vkutil::load_Shader_Module(SHADER_PATH "/gradient.comp.spv", _device, &gradientShaderModule))
    {
        fmt::print("Failed to load shader module at path: {}", SHADER_PATH "/gradient.comp.spv");
        return;
    }
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

    VK_CHECK(vkCreateComputePipelines(_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &_gradientPipeline));

    //Destroy shader module, add the pipeline and pipeline layout objects to deletion queue
    vkDestroyShaderModule(_device, gradientShaderModule, nullptr);

    _mainDeletionQueue.addDeletor([&]()
    {
        vkDestroyPipeline(_device, _gradientPipeline, nullptr);
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

    //Acquire an Image with index from the swapchain with timeout set to 1 second
    uint32_t swapchainImageIndex;
    VK_CHECK(vkAcquireNextImageKHR(_device, _swapchain, 1000000000, GetCurrentFrameData()._swapchainSemaphore, nullptr, &swapchainImageIndex)); //No fence needed since this operation shouldn't stall the CPU

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

    //Transition the DrawImage from General to transfer source, to be used later to draw on the swapchain image
    vkutil::transition_Image(cmd, _drawImage._image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    //Transition the swapchain Image to transfer destination
    vkutil::transition_Image(cmd, _swapchain_Images[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    //Transition the Image from general to Aspect which can be presented to screen
    vkutil::transition_Image(cmd, _swapchain_Images[swapchainImageIndex], VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    //Copy the drawImage onto the Swapchain Image
    vkutil::copy_Image_to_Image(cmd, _drawImage._image, _swapchain_Images[swapchainImageIndex], _drawImage._extent, _swapchainImageExtent2D);

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
    VK_CHECK(vkQueuePresentKHR(_commandsQueue, &present_Info));

    //increase the Framenumber
    _frameNumber++;
}

void VulkanEngine::draw_Background(VkCommandBuffer cmd)
{
    //Clear the screen with a black image
    VkClearColorValue clearColor;
    clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
    VkImageSubresourceRange image_Subresource_Range = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);
    //Add the command for clearing the image got from the swapchain using the clear color generated
    vkCmdClearColorImage(cmd, _drawImage._image, VK_IMAGE_LAYOUT_GENERAL, &clearColor, 1, &image_Subresource_Range);

    //Bind the Pipeline to the draw command
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _gradientPipeline);
    //Bind the Descriptor Set to the draw Command
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _gradientPipelineLayout, 0, 1, &_drawImageDescriptors, 0, nullptr);
    //Dispatch the Compute shader to start drawing on the Draw Image with group count to fill the screen
    vkCmdDispatch(cmd, (uint32_t)(_drawExtent.width / 10), (uint32_t)(_drawExtent.height / 10), (uint32_t)1);
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

        //Start a new ImGui frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        //Show the Demo ImGui Scene
        ImGui::ShowDemoWindow();

        //Render the ImGui window
        ImGui::Render();

        draw();
    }
}