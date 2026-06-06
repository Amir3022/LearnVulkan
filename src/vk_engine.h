// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>

class VulkanEngine {
public:

	VulkanEngine();

	VulkanEngine(const VkExtent2D& inRes);

	static VulkanEngine& Get();

	//initializes everything in the engine
	void init();

	//Initialize the Various Vulkan Components
	void init_Vulkan();

	void init_Swapchain();

	void init_Commands();

	void init_Sync_Structures();

	//shuts down the engine
	void cleanup();

	//draw loop
	void draw();

	//run main loop
	void run();

private:
	//Engine Variables
	bool _isInitialized;
	int _frameNumber;
	bool stop_rendering;
	VkExtent2D _windowExtent;
	struct SDL_Window* _window;

	//Vulkan Components Handles
	VkInstance _instance;
	VkDebugUtilsMessengerEXT  _debug_Messanger;
	VkPhysicalDevice _physicalDevice;
	VkDevice _device;
	VkSurfaceKHR _surface;
};
