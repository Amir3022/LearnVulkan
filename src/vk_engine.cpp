//> includes
#include "vk_engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk_initializers.h>
#include <vk_types.h>
#include <vk_images.h>

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
}

void VulkanEngine::init_Swapchain()
{
    create_Swapchain(_windowExtent.width, _windowExtent.height);
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
        }
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

void VulkanEngine::draw()
{
    //Wait for the Render Fence to finish executing the current command in the Buffer, then reset it
    VK_CHECK(vkWaitForFences(_device, 1, &GetCurrentFrameData()._renderFence, true, 1000000000)); //Set wait timeout to 1 billion nano seconds = 1 seconds
    VK_CHECK(vkResetFences(_device, 1, &GetCurrentFrameData()._renderFence));

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

    //Transition the iamge we got from the swapchain from invalid state to general at which we can write to (General isn't the best for rastarization, but will do since we will be using compute rendering)
    vkutil::transition_Image(cmd, _swapchain_Images[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    //Create a color value for clearing the screen with flashing blue
    VkClearColorValue clearColor;
    float r_Val = std::abs(std::sin(_frameNumber / 60.0f));
    clearColor = { r_Val, 0.0f, 0.0f, 1.0f };
    VkImageSubresourceRange image_Subresource_Range = vkinit::image_subresource_range(VK_IMAGE_ASPECT_COLOR_BIT);
    //Add the command for clearing the image got from the swapchain using the clear color generated
    vkCmdClearColorImage(cmd, _swapchain_Images[swapchainImageIndex], VK_IMAGE_LAYOUT_GENERAL, &clearColor, 1, &image_Subresource_Range);

    //Transition the Image from general to Aspect which can be presented to screen
    vkutil::transition_Image(cmd, _swapchain_Images[swapchainImageIndex], VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

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

void VulkanEngine::run()
{
    SDL_Event e;
    bool bQuit = false;

    // main loop
    while (!bQuit) {
        // Handle events on queue
        while (SDL_PollEvent(&e) != 0) {
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
        }

        // do not draw if we are minimized
        if (stop_rendering) {
            // throttle the speed to avoid the endless spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        draw();
    }
}