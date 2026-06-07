// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>

struct SDL_Window;

struct FrameData
{
	VkCommandPool _commandPool;
	VkCommandBuffer _mainCommandBuffer;
};

constexpr uint32_t FRAME_COUNT = 2;

class VulkanEngine {
public:

	VulkanEngine();

	VulkanEngine(const VkExtent2D& inRes);

	static VulkanEngine& Get();

	//initializes everything in the engine
	void init();

	//shuts down the engine
	void cleanup();

	//run main loop
	void run();

	//Commands Functions
	FrameData& GetCurrentFrameData() { return _frames[_frameNumber % FRAME_COUNT]; }

private:
	//Initialize the Various Vulkan Components
	void init_Vulkan();
	void init_Swapchain();
	void init_Commands();
	void init_Sync_Structures();

	//Swapchain Functions
	void create_Swapchain(uint32_t width, uint32_t height);
	void destroy_Swapchain();

	//draw loop
	void draw();

private:
	//Engine Variables
	bool _isInitialized;
	int _frameNumber;
	bool stop_rendering;
	VkExtent2D _windowExtent;
	SDL_Window* _window;

	//Vulkan Components Handles
	VkInstance _instance;
	VkDebugUtilsMessengerEXT  _debug_Messanger;
	VkPhysicalDevice _physicalDevice;
	VkDevice _device;
	VkSurfaceKHR _surface;

	//Swapchain Variables
	VkSwapchainKHR _swapchain;
	VkFormat _swapchainImageFormat;
	VkExtent2D _swapchainImageExtent2D;
	std::vector<VkImage> _swapchain_Images;
	std::vector<VkImageView> _swapchain_Image_Views;

	//Commands Variables
	std::array<FrameData, FRAME_COUNT> _frames;
	VkQueue _commandsQueue;
	uint32_t _commandsQueueFamilyIndex;
};
