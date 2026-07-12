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
	DescriptorAllocatorGrowable _descriptorsPool;
};

//Material Type Structs
struct GLTF_MetallicRoughMaterial
{
	MaterialPipeline opaquePipeline;
	MaterialPipeline transparentPipeline;
	VkDescriptorSetLayout materialLayout;
	DescriptorSetWriter writer;

	struct MaterialConstants
	{
		glm::vec4 colorFactors;
		glm::vec4 metal_roughFactors;
		glm::vec4 padding[14];	//Used for padding for when passing the constants buffer to shader	(Total constants size a multiple of 256 bytes)
	};

	struct MaterialResources
	{
		AllocatedImage colorTexture;
		VkSampler colorTextureSampler;
		AllocatedImage metalRoughTexture;
		VkSampler metalRoughTextureSampler;
		VkBuffer materialDataBuffer;
		uint32_t materialDataBufferOffset;
	};

	void buildPipeline(class VulkanEngine* engine);
	void clearResources(VkDevice device);
	MaterialInstance writeMaterial(VkDevice device, EMaterialPass materialPass, const MaterialResources& resources, DescriptorAllocatorGrowable& descriptorSetAllocator);
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

	//Loading meshes Functions
	GPUMeshBuffers uploadMesh(std::span<Vertex> vertices, std::span<uint32_t> indices);

	/** Public Accessors */
	VkDevice getDevice() {return _device;}
	VkDescriptorSetLayout getSceneDataLayout() {return _gpuSceneDescriptorSetLayout;}
	AllocatedImage getDrawImage() {return _drawImage;}
	AllocatedImage getDepthImage() {return _depthImage;}
	float getDeltaTime() {return _deltaTime;}

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
	void resize_Swapchain();

	//Pipeline Functions
	void init_Pipelines_Background();

	//draw loop
	void draw();
	void draw_Background(VkCommandBuffer cmd);
	void draw_Geometry(VkCommandBuffer cmd);
	void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView);
	void updateScene();

	//Immediate Commands submission function
	void submit_Immediate_Command(std::function<void(VkCommandBuffer cmd)>&& function);

	//Buffer Functions
	AllocatedBuffer createBuffer(size_t bufferSize, VkBufferUsageFlags bufferUsage, VmaMemoryUsage memoryUsage);
	void destroyBuffer(const AllocatedBuffer& buffer);

	//Image Functions
	AllocatedImage createImage(VkExtent3D imageExtent, VkFormat imageFormat, VkImageUsageFlags flags, VmaMemoryUsage memUsageFlags, bool bUseMipMap = false);
	AllocatedImage createImage(void* data, VkExtent3D imageExtent, VkFormat imageFormat, VkImageUsageFlags flags, VmaMemoryUsage memUsageFlags, bool bUseMipMap = false);
	void destroyImage(const AllocatedImage& image);

	//Mesh Draw Functions
	void init_Default_Values();
	void init_Loaded_Mesh();

	//Game Engine functions
	void calculateDeltaTime();
	
private:
	//Engine Variables
	bool _isInitialized;
	int _frameNumber;
	bool stop_rendering;
	bool resize_Window;
	VkExtent2D _windowExtent;
	SDL_Window* _window;
	deletionQueue _mainDeletionQueue;
	float _deltaTime;
	std::chrono::steady_clock::time_point _timeStamp;

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
	AllocatedImage _depthImage; //Image used for depth testing
	VkExtent2D _drawExtent;
	float _renderScale;

	//Commands Variables
	std::array<FrameData, FRAME_COUNT> _frames;
	VkQueue _commandsQueue;
	uint32_t _commandsQueueFamilyIndex;

	//Immediate Commands Variables
	VkCommandPool _immCmdPool;
	VkCommandBuffer _immCmdBuffer;
	VkFence _immCmdFence;

	//Descriptor Variables
	DescriptorAllocatorGrowable _globalDescriptorAllocator;
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

	//Scene Data Variables
	GPUSceneData _gpuSceneData;
	VkDescriptorSetLayout _gpuSceneDescriptorSetLayout;
	VkDescriptorSet _gpuSceneDataDescriptorSet;

	//Test Texture variables
	VkDescriptorSetLayout _testTextureDescriptorSetLayout;

	//Loaded Mesh Variables
	std::vector<std::shared_ptr<MeshAsset>> _testMeshes;

	//Materials variables
	MaterialInstance _defaultMatInstance;
	GLTF_MetallicRoughMaterial _defaultMat; 

	//Abstracting Mesh Rendering
	DrawContext _mainDrawContext;
	std::unordered_map<std::string, std::shared_ptr<Node>> _loadedNodes;

	//Engine Default Colored Texture Images and samplers
	AllocatedImage _whiteTex;
	AllocatedImage _greyTex;
	AllocatedImage _blackTex;
	AllocatedImage _errorCheckerBoard;
	VkSampler _defaultSamplerLinear;
	VkSampler _defaultSamplerNearest;
};
