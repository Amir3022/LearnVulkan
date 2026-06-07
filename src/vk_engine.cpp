//> includes
#include "vk_engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk_initializers.h>
#include <vk_types.h>

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
    // nothing yet
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