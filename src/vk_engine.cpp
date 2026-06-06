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
}

void VulkanEngine::init_Swapchain()
{
}

void VulkanEngine::init_Commands()
{
}

void VulkanEngine::init_Sync_Structures()
{
}

void VulkanEngine::cleanup()
{
    if (_isInitialized) {

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
                else
                {
                    fmt::println("Key held down is: {}", (char)e.key.keysym.sym);
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