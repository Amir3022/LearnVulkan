// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>
#include "vk_descriptors.h"

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
	void init_Descriptors();
	void init_Pipelines();
	void init_imgui();
	void init_triangle_Pipeline();
	void init_mesh_Pipeline();

	//Swapchain Functions
	void create_Swapchain(uint32_t width, uint32_t height);
	void destroy_Swapchain();

	//Pipeline Functions
	void init_Pipelines_Background();

	//draw loop
	void draw();
	void draw_Background(VkCommandBuffer cmd);

	//Draw Geometry functions
	void draw_Geometry(VkCommandBuffer cmd);

	//Immediate Commands submission function
	void submit_Immediate_Command(std::function<void(VkCommandBuffer cmd)>&& function);

	//ImGui functions
	void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView);

	//Buffer Functions
	AllocatedBuffer createBuffer(size_t bufferSize, VkBufferUsageFlags bufferUsage, VmaMemoryUsage memoryUsage);
	void destroyBuffer(const AllocatedBuffer& buffer);

	//Mesh Draw Functions
	GPUMeshBuffers uploadMesh(std::span<Vertex> vertices, std::span<uint32_t> indices);
	void init_Default_Values();
	
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
	VmaAllocator _allocator;

	//Swapchain Variables
	VkSwapchainKHR _swapchain;
	VkFormat _swapchainImageFormat;
	VkExtent2D _swapchainImageExtent2D;
	std::vector<VkImage> _swapchain_Images;
	std::vector<VkImageView> _swapchain_Image_Views;
	AllocatedImage _drawImage;	//Image used to draw on before writing on the swapchain acquired image, giving us more freedom to the operations we can do
	VkExtent2D _drawExtent;

	//Commands Variables
	std::array<FrameData, FRAME_COUNT> _frames;
	VkQueue _commandsQueue;
	uint32_t _commandsQueueFamilyIndex;

	//Immediate Commands Variables
	VkCommandPool _immCmdPool;
	VkCommandBuffer _immCmdBuffer;
	VkFence _immCmdFence;

	//Descriptor Variables
	DescriptorAllocator GlobalDescriptorAllocator;
	VkDescriptorSet _drawImageDescriptors;
	VkDescriptorSetLayout _drawImageDescriptorSetLayout;

	//Compute Pipeline Variables
	VkPipelineLayout _gradientPipelineLayout;
	VkPipeline _gradientPipeline;
	std::vector<ComputeEffect> backgroundEffects;
	int currentActiveBackgroundEffect;
	
	//Render Pipeline Variables
	VkPipelineLayout _trianglePipelineLayout;
	VkPipeline _trianglePipeline;

	//Mesh Render variables
	VkPipelineLayout _meshPipelineLayout;
	VkPipeline _meshPipeline;
	GPUMeshBuffers _meshBuffers;
};
