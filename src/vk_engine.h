// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>

struct SDL_Window;

//Struct containing a queue of vulkan object deletors, to be called at each new frame after idle to delete last frame vulkan objects, and at cleanup on program shutdown
struct deletionQueue
{
	std::deque<std::function<void()>> _deletors;

	void addDeletor(std::function<void()>&& deletor)
	{
		_deletors.push_back(deletor);
	}

	void flush()
	{
		for (auto i = _deletors.rbegin(); i != _deletors.rend(); i++)
		{
			(*i)();
		}

		_deletors.clear();
	}
};

struct FrameData
{
	VkCommandPool _commandPool;
	VkCommandBuffer _mainCommandBuffer;
	VkSemaphore _swapchainSemaphore;
	VkSemaphore _renderSemaphore;
	VkFence _renderFence;

	deletionQueue _frameDeletionQueue;
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
	deletionQueue _mainDeletionQueue;

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
