
#include "clither/bezier.h"
#include "clither/camera.h"
#include "clither/command.h"
#include "clither/fs.h"
#include "clither/gfx.h"
#include "clither/input.h"
#include "clither/log.h"
#include "clither/resource_pack.h"
#include "clither/snake.h"
#include "clither/world.h"

#include "cstructures/memory.h"
#include "cstructures/string.h"

#include "GLFW/glfw3.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define SHADOW_MAP_SIZE_FACTOR 4

//#define VK_USE_PLATFORM_WIN32_KHR
#include "vulkan/vulkan.h"

PFN_vkGetPhysicalDeviceSurfaceSupportKHR fpGetPhysicalDeviceSurfaceSupportKHR;
PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR fpGetPhysicalDeviceSurfaceCapabilitiesKHR;
PFN_vkGetPhysicalDeviceSurfaceFormatsKHR fpGetPhysicalDeviceSurfaceFormatsKHR;
PFN_vkGetPhysicalDeviceSurfacePresentModesKHR fpGetPhysicalDeviceSurfacePresentModesKHR;
PFN_vkCreateSwapchainKHR fpCreateSwapchainKHR;
PFN_vkDestroySwapchainKHR fpDestroySwapchainKHR;
PFN_vkGetSwapchainImagesKHR fpGetSwapchainImagesKHR;
PFN_vkAcquireNextImageKHR fpAcquireNextImageKHR;
PFN_vkQueuePresentKHR fpQueuePresentKHR;
PFN_vkWaitSemaphoresKHR fpWaitSemaphoresKHR;
PFN_vkGetSemaphoreCounterValueKHR fpGetSemaphoreCounterValueKHR;

#define DEFAULT_FENCE_TIMEOUT 100000000000
#define MAX_RENDER_TARGETS 8
#define BUFFER_COUNT 3 // the number of frames the cpu is allowed to operate ahead of the gpu
#define MAX_GPU_COUNT 4 //usually one integrated, and three in sli/crossfire for crazy configs
#define MAX_QUEUE_FAMILY_COUNT 16
#define MAX_EXT_COUNT 512
#define MAX_QUEUE_COUNT 64
#define MAX_SURFACE_FORMATS 64
#define MAX_SWAP_CHAIN_PRESENT_MODES 16
#define MAX_QUEUE_FAMILIES 5

struct VulkanTexture {
	VkImage image;
	VkImageLayout imageLayout;
	VkDeviceMemory deviceMemory;
	VkImageView view;
};

struct VulkanBuffer
{
	VkDevice device;
	VkBuffer buffer;
	VkDeviceMemory memory;
	VkDescriptorBufferInfo descriptor;
	VkDeviceSize size;
	VkDeviceSize alignment;
	void* mapped;
	/** @brief Usage flags to be filled by external source at buffer creation (to query at some later point) */
	VkBufferUsageFlags usageFlags;
	/** @brief Memory property flags to be filled by external source at buffer creation (to query at some later point) */
	VkMemoryPropertyFlags memoryPropertyFlags;
	//VkResult map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
	//void unmap();
	//VkResult bind(VkDeviceSize offset = 0);
	//void setupDescriptor(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
	//void copyTo(void* data, VkDeviceSize size);
	//VkResult flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
	//VkResult invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
	//void destroy();
};

/*
struct bg
{
	GLuint program;
	struct VulkanBuffer vbo;
	struct VulkanBuffer ibo;
	GLuint fbo;
	struct VulkanTexture texShadow;
	struct VulkanTexture texCol;
	struct VulkanTexture texNor;
	GLuint uAspectRatio;
	GLuint uCamera;
	GLuint uShadowInvRes;
	GLuint sShadow;
	GLuint sCol;
	GLuint sNM;
};

struct sprite_mesh
{
	struct VulkanBuffer vbo;
	struct VulkanBuffer ibo;
};

struct sprite_shadow
{
	GLuint program;
	GLuint uAspectRatio;
	GLuint uPosCameraSpace;
	GLuint uDir;
	GLuint uSize;
	GLuint uAnim;
	GLuint sNM;
};

struct sprite_mat
{
	GLuint program;
	GLuint uAspectRatio;
	GLuint uPosCameraSpace;
	GLuint uDir;
	GLuint uSize;
	GLuint uAnim;
	GLuint sCol;
	GLuint sNM;
};

struct sprite_tex
{
	struct VulkanTexture texDiffuse;
	struct VulkanTexture texNM;
	int8_t tile_x, tile_y, tile_count, frame;
};*/

struct RenderTarget {
	struct VulkanTexture texture[3];
	uint8_t isDepth;
};

struct RenderPass {
	VkRenderPass* renderPass;
	uint32_t renderTargetCount;
	struct RenderTarget renderTargets[MAX_RENDER_TARGETS];
	struct RenderTarget depthTarget;
	VkAttachmentLoadOp loadOperations[MAX_RENDER_TARGETS];
	float clearColor[4];
};

struct SwapChain {
	VkSurfaceKHR surface;
	VkFormat colorFormat;
	VkColorSpaceKHR colorSpace;
	VkSwapchainKHR swapChain;
	VkImage images[BUFFER_COUNT];
	struct VulkanTexture buffers[BUFFER_COUNT];
	uint32_t queueNodeIndex;
};

struct CommandBuffer {
	VkFence* waitFences[BUFFER_COUNT];
	VkCommandBuffer* drawCmdBuffers[BUFFER_COUNT];
};


struct gfx
{
	GLFWwindow* window;
	int width, height;

	VkInstance instance;
	char *supportedInstanceExtensions;
	VkQueueFamilyProperties queueFamilyProperties[MAX_QUEUE_FAMILIES];
	char* supportedExtensions[MAX_EXT_COUNT];

	VkPhysicalDevice physicalDevice;
	VkPhysicalDeviceProperties deviceProperties;
	VkPhysicalDeviceFeatures deviceFeatures;
	VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
	VkPhysicalDeviceFeatures enabledFeatures;
	char* enabledDeviceExtensions;
	char* enabledInstanceExtensions;

	void* deviceCreatepNextChain;
	VkDevice device;
	VkQueue queue;

	/** @brief Pipeline stages used to wait at for graphics queue submissions */
	VkPipelineStageFlags submitPipelineStages;
	// Contains command buffers and semaphores to be presented to the queue
	VkSubmitInfo submitInfo;
	// Command buffers used for rendering
	VkCommandBuffer* drawCmdBuffers;
	// Global render pass for frame buffer writes

	struct RenderPass renderPass;
	// List of available frame buffers (same as number of swap chain images)
	VkFramebuffer* frameBuffers[BUFFER_COUNT];
	// Active frame buffer index
	uint32_t currentBuffer;
	uint32_t bufferCount;

	VkSemaphore renderComplete[BUFFER_COUNT];
	VkSemaphore presentComplete[BUFFER_COUNT];
	uint32_t presentIdx;

	// Wraps the swap chain to present images (framebuffers) to the windowing system
	struct SwapChain swapChain;
	VkFormat depthFormat;
	VkCommandPool cmdPool;

	struct CommandBuffer cmdBuffer;

	struct
	{
		uint32_t graphics;
		uint32_t compute;
		uint32_t transfer;
	} queueFamilyIndices;

	struct input input_buffer;

	/*
	struct bg bg;
	struct sprite_mesh sprite_mesh;
	struct sprite_mat sprite_mat;
	struct sprite_shadow sprite_shadow;
	struct sprite_tex head0_base;
	struct sprite_tex head0_gather;
	struct sprite_tex body0_base;
	struct sprite_tex tail0_base;*/
};

struct aspect_ratio
{
	float scale_x, scale_y;
	float pad_x, pad_y;
};

struct vertex
{
	float pos[2];
	float uv[2];
};

static const struct vertex sprite_vertices[4] = {
	{{-0.5, -0.5}, {0,  1}},
	{{-0.5,  0.5}, {0,  0}},
	{{ 0.5, -0.5}, {1,  1}},
	{{ 0.5,  0.5}, {1,  0}}
};
static const short sprite_indices[6] = {
	0, 2, 1,
	1, 3, 2
};
static const struct vertex bg_vertices[4] = {
	{{-1, -1}, {0,  0}},
	{{-1,  1}, {0,  1}},
	{{ 1, -1}, {1,  0}},
	{{ 1,  1}, {1,  1}}
};
static const short bg_indices[6] = {
	0, 2, 1,
	1, 3, 2
};
static const char* attr_bindings[] = {
	"vPosition",
	"vTexCoord",
	NULL
};

struct Texture {
	VkImage image;
	VkImageView view;
};

int
gfx_init(void)
{
	if (!glfwInit())
	{
		log_err("Failed to initialize GLFW\n");
		return -1;
	}
	if (!glfwVulkanSupported()) {
		log_err("Vulkan not supported\n");
		return -1;
	}

	return 0;
}


uint32_t findMemoryType(struct gfx* gfx, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(gfx->physicalDevice, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}
}

uint32_t getQueueFamilyIndex(const VkQueueFamilyProperties* queueFamilyProperties, VkQueueFlagBits queueFlags)
{

	/* 
	*
	* 
	*  No support for compute/transfer only queue types for now. Graphics queues suffice for these workloads at the cost of
	*  missing out on some async capability
	* 
	* 
	// Dedicated queue for compute
	// Try to find a queue family index that supports compute but not graphics
	if (queueFlags & VK_QUEUE_COMPUTE_BIT)
	{
		for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++)
		{
			if ((queueFamilyProperties[i].queueFlags & queueFlags) && ((queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0))
			{
				return i;
			}
		}
	}

	// Dedicated queue for transfer
	// Try to find a queue family index that supports transfer but not graphics and compute
	if (queueFlags & VK_QUEUE_TRANSFER_BIT)
	{
		for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++)
		{
			if ((queueFamilyProperties[i].queueFlags & queueFlags) && ((queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) && ((queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0))
			{
				return i;
			}
		}
	}*/

	// For other queue types or if no separate compute queue is present, return the first one to support the requested flags
	for (uint32_t i = 0; i < 64; i++)
	{
		if (queueFamilyProperties[i].queueFlags & queueFlags)
		{
			return i;
		}
	}
}

static
VkResult createLogicalDevice(struct gfx* gfx, VkPhysicalDeviceFeatures enabledFeatures, const char* enabledExtensions, void* pNextChain, int useSwapChain, VkQueueFlags requestedQueueTypes)
{
	// Desired queues need to be requested upon logical device creation
	// Due to differing queue family configurations of Vulkan implementations this can be a bit tricky, especially if the application
	// requests different queue types

	VkDeviceQueueCreateInfo queueCreateInfo;
	int numberOfQueues = 0;

	// Get queue family indices for the requested queue family types
	// Note that the indices may overlap depending on the implementation

	const float defaultQueuePriority = 0.0f;

	// Graphics queue
	if (requestedQueueTypes & VK_QUEUE_GRAPHICS_BIT)
	{
		gfx->queueFamilyIndices.graphics = getQueueFamilyIndex(gfx->queueFamilyProperties, VK_QUEUE_GRAPHICS_BIT);
		VkDeviceQueueCreateInfo queueInfo;
		queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueInfo.queueFamilyIndex = gfx->queueFamilyIndices.graphics;
		queueInfo.queueCount = 1;
		queueInfo.pQueuePriorities = &defaultQueuePriority;
		queueInfo.pNext = 0;
		queueInfo.flags = 0;
		queueCreateInfo = queueInfo;
	}
	else
	{
		gfx->queueFamilyIndices.graphics = 0;
	}

	/*
	// Dedicated compute queue
	if (requestedQueueTypes & VK_QUEUE_COMPUTE_BIT)
	{
		pRenderer->queueFamilyIndices.compute = getQueueFamilyIndex(pRenderer->queueFamilyProperties, VK_QUEUE_COMPUTE_BIT);
		if (pRenderer->queueFamilyIndices.compute != pRenderer->queueFamilyIndices.graphics)
		{
			// If compute family index differs, we need an additional queue create info for the compute queue
			VkDeviceQueueCreateInfo queueInfo{};
			queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueInfo.queueFamilyIndex = pRenderer->queueFamilyIndices.compute;
			queueInfo.queueCount = 1;
			queueInfo.pQueuePriorities = &defaultQueuePriority;
			queueCreateInfos.push_back(queueInfo);
		}
	}
	else
	{
		// Else we use the same queue
		pRenderer->queueFamilyIndices.compute = pRenderer->queueFamilyIndices.graphics;
	}

	// Dedicated transfer queue
	if (requestedQueueTypes & VK_QUEUE_TRANSFER_BIT)
	{
		pRenderer->queueFamilyIndices.transfer = getQueueFamilyIndex(pRenderer->queueFamilyProperties, VK_QUEUE_TRANSFER_BIT);
		if ((pRenderer->queueFamilyIndices.transfer != pRenderer->queueFamilyIndices.graphics) && (pRenderer->queueFamilyIndices.transfer != pRenderer->queueFamilyIndices.compute))
		{
			// If compute family index differs, we need an additional queue create info for the compute queue
			VkDeviceQueueCreateInfo queueInfo{};
			queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueInfo.queueFamilyIndex = pRenderer->queueFamilyIndices.transfer;
			queueInfo.queueCount = 1;
			queueInfo.pQueuePriorities = &defaultQueuePriority;
			queueCreateInfos.push_back(queueInfo);
		}
	}
	else
	{
		// Else we use the same queue
		pRenderer->queueFamilyIndices.transfer = pRenderer->queueFamilyIndices.graphics;
	}
	*/

	// Create the logical device representation
	const char* deviceExtensions[MAX_EXT_COUNT];
#if defined(VK_USE_PLATFORM_MACOS_MVK) && (VK_HEADER_VERSION >= 216)
	deviceExtensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
#endif

	VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering_feature;
	dynamic_rendering_feature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
	dynamic_rendering_feature.dynamicRendering = VK_TRUE;

	VkDeviceCreateInfo deviceCreateInfo;
	memset(&deviceCreateInfo, 0, sizeof(VkDeviceCreateInfo));
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.queueCreateInfoCount = 1;
	deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
	deviceCreateInfo.pEnabledFeatures = 0;
	deviceCreateInfo.pNext = 0;

	/*
	// If a pNext(Chain) has been passed, we need to add it to the device creation info
	VkPhysicalDeviceFeatures2 physicalDeviceFeatures2;
	memset(&physicalDeviceFeatures2, 0, sizeof(VkPhysicalDeviceFeatures2));
	if (pNextChain) {
		physicalDeviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		physicalDeviceFeatures2.features = enabledFeatures;
		physicalDeviceFeatures2.pNext = pNextChain;
		deviceCreateInfo.pEnabledFeatures = 0;
		deviceCreateInfo.pNext = &physicalDeviceFeatures2;
	}
	*/
	// Enable the debug marker extension if it is present (likely meaning a debugging tool is present)
	//if (extensionSupported(VK_EXT_DEBUG_MARKER_EXTENSION_NAME))
	//{
	//	deviceExtensions.push_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
	//	enableDebugMarkers = true;
	//}

	deviceExtensions[0] = (VK_KHR_SWAPCHAIN_EXTENSION_NAME);
	
	deviceCreateInfo.enabledExtensionCount = 1;
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions;
	
	gfx->enabledFeatures = enabledFeatures;
	VkResult result = vkCreateDevice(gfx->physicalDevice, &deviceCreateInfo, 0, &gfx->device);
	if (result != VK_SUCCESS)
	{
		return result;
	}

	return result;
}

struct gfx*
	gfx_create(int initial_width, int initial_height) {

	//gfx = new Renderer();
	struct gfx* gfx = MALLOC(sizeof * gfx);
	struct SwapChain* pSwapChain = &gfx->swapChain;
	gfx->bufferCount = BUFFER_COUNT;
	gfx->currentBuffer = 0;

	const char* instance_layers = "VK_LAYER_KHRONOS_validation";

	PFN_vkCreateInstance pfnCreateInstance = (PFN_vkCreateInstance)
		glfwGetInstanceProcAddress(NULL, "vkCreateInstance");

	uint32_t count;
	const char** extensions = glfwGetRequiredInstanceExtensions(&count);
	if (extensions == NULL) {
		log_err("No supported vulkan extensions found\n");
		return NULL;
	}

	VkApplicationInfo appInfo;
	memset(&appInfo, 0, sizeof(VkApplicationInfo));
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "clither";
	appInfo.pEngineName = "Mouse House";
	appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 1);
	appInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
	appInfo.apiVersion = VK_MAKE_VERSION(1, 0, 2);

	VkInstanceCreateInfo ici;
	memset(&ici, 0, sizeof(VkInstanceCreateInfo));
	ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	ici.enabledExtensionCount = count;
	ici.ppEnabledExtensionNames = extensions;
	ici.enabledLayerCount = 1;
	ici.ppEnabledLayerNames = &instance_layers;
	ici.pApplicationInfo = &appInfo;

	VkInstance instance;
	VkResult res = vkCreateInstance(&ici, 0, &instance);
	if (res != VK_SUCCESS) {
		log_err("Unable to create vulkan instance\n");
		return NULL;
	}
	gfx->instance = instance;

	// Physical device
	uint32_t gpuCount = 0;
	// Get number of available physical devices
	vkEnumeratePhysicalDevices(gfx->instance, &gpuCount, NULL);
	if (gpuCount == 0) {
		log_err("No device with vulkan support found\n");
		return NULL;
	}

	// Enumerate devices
	VkPhysicalDevice physicalDevices[MAX_GPU_COUNT+1];
	vkEnumeratePhysicalDevices(gfx->instance, &gpuCount, physicalDevices);
	
	// GPU selection
	// Select physical device to be used for the Vulkan example
	// Defaults to the first device unless specified by command line
	uint32_t selectedDevice = 1;
	gfx->physicalDevice = physicalDevices[selectedDevice];

	// Store properties (including limits), features and memory properties of the physical device (so that examples can check against them)
	vkGetPhysicalDeviceProperties(gfx->physicalDevice, &gfx->deviceProperties);
	vkGetPhysicalDeviceFeatures(gfx->physicalDevice, &gfx->deviceFeatures);
	vkGetPhysicalDeviceMemoryProperties(gfx->physicalDevice, &gfx->deviceMemoryProperties);

	uint32_t queueFamilyCount;
	vkGetPhysicalDeviceQueueFamilyProperties(gfx->physicalDevice, &queueFamilyCount, NULL);
	assert(queueFamilyCount > 0);

	vkGetPhysicalDeviceQueueFamilyProperties(gfx->physicalDevice, &queueFamilyCount, gfx->queueFamilyProperties);

	// Get list of supported extensions
	uint32_t extCount = 0;
	vkEnumerateDeviceExtensionProperties(gfx->physicalDevice, NULL, &extCount, NULL);
	if (extCount > 0)
	{
		VkExtensionProperties extensions[MAX_EXT_COUNT];
		if (vkEnumerateDeviceExtensionProperties(gfx->physicalDevice, NULL, &extCount, &extensions) == VK_SUCCESS)
		{
			for (int i = 0; i < extCount; i++)
			{
				gfx->supportedExtensions[i] = extensions[i].extensionName;
			}
		}
	}

	memset(&gfx->enabledFeatures, 0, sizeof(VkPhysicalDeviceFeatures));
	createLogicalDevice(gfx, gfx->enabledFeatures, gfx->enabledDeviceExtensions, gfx->deviceCreatepNextChain, 1, VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT);

	// Get a graphics queue from the device
	vkGetDeviceQueue(gfx->device, gfx->queueFamilyIndices.graphics, 0, &gfx->queue);

	// Find a suitable depth format
	//VkBool32 validDepthFormat = getSupportedDepthFormat(gfx->physicalDevice, &gfx->depthFormat);
	//assert(validDepthFormat);

	// Create synchronization objects
	VkSemaphoreCreateInfo semaphoreCreateInfo;
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreCreateInfo.pNext = 0;
	semaphoreCreateInfo.flags = 0;
	// Create a semaphore used to synchronize image presentation
	// Ensures that the image is displayed before we start submitting new commands to the queue

	for (int i = 0; i < BUFFER_COUNT; i++) {
		vkCreateSemaphore(gfx->device, &semaphoreCreateInfo, NULL, &gfx->presentComplete[i]);
		// Create a semaphore used to synchronize command submission
		// Ensures that the image is not presented until all commands have been submitted and executed
		vkCreateSemaphore(gfx->device, &semaphoreCreateInfo, NULL, &gfx->renderComplete[i]);
	}
	// Set up submit info structure
	// Semaphores will stay the same during application lifetime
	// Command buffer submission info is set by each example

	//this->physicalDevice = physicalDevice;
	//this->device = device;
	fpGetPhysicalDeviceSurfaceSupportKHR = (PFN_vkGetPhysicalDeviceSurfaceSupportKHR)(vkGetInstanceProcAddr(gfx->instance, "vkGetPhysicalDeviceSurfaceSupportKHR"));
	fpGetPhysicalDeviceSurfaceCapabilitiesKHR = (PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR)(vkGetInstanceProcAddr(gfx->instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));
	fpGetPhysicalDeviceSurfaceFormatsKHR = (PFN_vkGetPhysicalDeviceSurfaceFormatsKHR)(vkGetInstanceProcAddr(gfx->instance, "vkGetPhysicalDeviceSurfaceFormatsKHR"));
	fpGetPhysicalDeviceSurfacePresentModesKHR = (PFN_vkGetPhysicalDeviceSurfacePresentModesKHR)(vkGetInstanceProcAddr(gfx->instance, "vkGetPhysicalDeviceSurfacePresentModesKHR"));

	fpCreateSwapchainKHR          = (PFN_vkCreateSwapchainKHR)(vkGetDeviceProcAddr(gfx->device, "vkCreateSwapchainKHR"));
	fpDestroySwapchainKHR         = (PFN_vkDestroySwapchainKHR)(vkGetDeviceProcAddr(gfx->device, "vkDestroySwapchainKHR"));
	fpGetSwapchainImagesKHR       = (PFN_vkGetSwapchainImagesKHR)(vkGetDeviceProcAddr(gfx->device, "vkGetSwapchainImagesKHR"));
	fpAcquireNextImageKHR         = (PFN_vkAcquireNextImageKHR)(vkGetDeviceProcAddr(gfx->device, "vkAcquireNextImageKHR"));
	fpQueuePresentKHR             = (PFN_vkQueuePresentKHR)(vkGetDeviceProcAddr(gfx->device, "vkQueuePresentKHR"));
	fpWaitSemaphoresKHR           = (PFN_vkWaitSemaphoresKHR)(vkGetDeviceProcAddr(gfx->device, "vkWaitSemaphoresKHR"));
	fpGetSemaphoreCounterValueKHR = (PFN_vkGetSemaphoreCounterValueKHR)(vkGetDeviceProcAddr(gfx->device, "vkGetSemaphoreCounterValueKHR"));

	GLFWwindow* window;
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	window = glfwCreateWindow(initial_width, initial_height, appInfo.pApplicationName, 0, 0);

	// make sure we indeed get the surface size we want.
	glfwGetFramebufferSize(window, &initial_width, &initial_height);
	VkResult ret = glfwCreateWindowSurface(gfx->instance, window, 0, &gfx->swapChain.surface);
	if (VK_SUCCESS != ret) {
		return -1;
	}

	// Get available queue family properties
	uint32_t queueCount;
	vkGetPhysicalDeviceQueueFamilyProperties(gfx->physicalDevice, &queueCount, NULL);
	assert(queueCount >= 1);

	VkQueueFamilyProperties queueProps[MAX_QUEUE_COUNT];
	vkGetPhysicalDeviceQueueFamilyProperties(gfx->physicalDevice, &queueCount, queueProps);

	// Iterate over each queue to learn whether it supports presenting:
	// Find a queue with present support
	// Will be used to present the swap chain images to the windowing system
	VkBool32 supportsPresent[MAX_QUEUE_COUNT];
	for (uint32_t i = 0; i < queueCount; i++)
	{
		fpGetPhysicalDeviceSurfaceSupportKHR(gfx->physicalDevice, i, pSwapChain->surface, &supportsPresent[i]);
	}

	// Search for a graphics and a present queue in the array of queue
	// families, try to find one that supports both
	uint32_t graphicsQueueNodeIndex = UINT32_MAX;
	uint32_t presentQueueNodeIndex = UINT32_MAX;
	for (uint32_t i = 0; i < queueCount; i++)
	{
		if ((queueProps[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
		{
			if (graphicsQueueNodeIndex == UINT32_MAX)
			{
				graphicsQueueNodeIndex = i;
			}

			if (supportsPresent[i] == VK_TRUE)
			{
				graphicsQueueNodeIndex = i;
				presentQueueNodeIndex = i;
				break;
			}
		}
	}
	if (presentQueueNodeIndex == UINT32_MAX)
	{
		// If there's no queue that supports both present and graphics
		// try to find a separate present queue
		for (uint32_t i = 0; i < queueCount; ++i)
		{
			if (supportsPresent[i] == VK_TRUE)
			{
				presentQueueNodeIndex = i;
				break;
			}
		}
	}

	// Exit if either a graphics or a presenting queue hasn't been found
	if (graphicsQueueNodeIndex == UINT32_MAX || presentQueueNodeIndex == UINT32_MAX)
	{
		//vks::tools::exitFatal("Could not find a graphics and/or presenting queue!", -1);
	}

	// todo : Add support for separate graphics and presenting queue
	if (graphicsQueueNodeIndex != presentQueueNodeIndex)
	{
		//vks::tools::exitFatal("Separate graphics and presenting queues are not supported yet!", -1);
	}

	pSwapChain->queueNodeIndex = graphicsQueueNodeIndex;

	// Get list of supported surface formats
	uint32_t formatCount;
	fpGetPhysicalDeviceSurfaceFormatsKHR(gfx->physicalDevice, pSwapChain->surface, &formatCount, NULL);
	assert(formatCount > 0);

	VkSurfaceFormatKHR surfaceFormats[MAX_SURFACE_FORMATS];
	fpGetPhysicalDeviceSurfaceFormatsKHR(gfx->physicalDevice, pSwapChain->surface, &formatCount, surfaceFormats);

	// If the surface format list only includes one entry with VK_FORMAT_UNDEFINED,
	// there is no preferred format, so we assume VK_FORMAT_B8G8R8A8_UNORM
	if ((formatCount == 1) && (surfaceFormats[0].format == VK_FORMAT_UNDEFINED))
	{
		pSwapChain->colorFormat = VK_FORMAT_B8G8R8A8_UNORM;
		pSwapChain->colorSpace = surfaceFormats[0].colorSpace;
	}
	else
	{
		// iterate over the list of available surface format and
		// check for the presence of VK_FORMAT_B8G8R8A8_UNORM
		int found_B8G8R8A8_UNORM = 0;
		for (int i = 0; i < formatCount; i++)
		{
			if (surfaceFormats[i].format == VK_FORMAT_B8G8R8A8_UNORM)
			{
				pSwapChain->colorFormat = surfaceFormats[i].format;
				pSwapChain->colorSpace = surfaceFormats[i].colorSpace;
				found_B8G8R8A8_UNORM = 1;
				break;
			}
		}

		// in case VK_FORMAT_B8G8R8A8_UNORM is not available
		// select the first available color format
		if (!found_B8G8R8A8_UNORM)
		{
			pSwapChain->colorFormat = surfaceFormats[0].format;
			pSwapChain->colorSpace = surfaceFormats[0].colorSpace;
		}
	}

	// Store the current swap chain handle so we can use it later on to ease up recreation
	pSwapChain->swapChain = VK_NULL_HANDLE;
	VkSwapchainKHR oldSwapchain = pSwapChain->swapChain;

	// Get physical device surface properties and formats
	VkSurfaceCapabilitiesKHR surfCaps;
	fpGetPhysicalDeviceSurfaceCapabilitiesKHR(gfx->physicalDevice, pSwapChain->surface, &surfCaps);

	// Get available present modes
	uint32_t presentModeCount;
	fpGetPhysicalDeviceSurfacePresentModesKHR(gfx->physicalDevice, pSwapChain->surface, &presentModeCount, NULL);
	assert(presentModeCount > 0);

	VkPresentModeKHR presentModes[MAX_SWAP_CHAIN_PRESENT_MODES];
	fpGetPhysicalDeviceSurfacePresentModesKHR(gfx->physicalDevice, pSwapChain->surface, &presentModeCount, presentModes);

	VkExtent2D swapchainExtent;
	// If width (and height) equals the special value 0xFFFFFFFF, the size of the surface will be set by the swapchain
	if (surfCaps.currentExtent.width == (uint32_t)-1)
	{
		// If the surface size is undefined, the size is set to
		// the size of the images requested.
		swapchainExtent.width = initial_width;
		swapchainExtent.height = initial_height;
	}
	else
	{
		// If the surface size is defined, the swap chain size must match
		swapchainExtent = surfCaps.currentExtent;
		gfx->width = surfCaps.currentExtent.width;
		gfx->height = surfCaps.currentExtent.height;
	}

	// Select a present mode for the swapchain

	// The VK_PRESENT_MODE_FIFO_KHR mode must always be present as per spec
	// This mode waits for the vertical blank ("v-sync")
	VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;

	// If v-sync is not requested, try to find a mailbox mode
	// It's the lowest latency non-tearing present mode available
	/*
	if (!rendererDesc.vsync)
	{
		for (size_t i = 0; i < presentModeCount; i++)
		{
			if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				swapchainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
				break;
			}
			if (presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR)
			{
				swapchainPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
			}
		}
	}*/

	// Determine the number of images
	uint32_t desiredNumberOfSwapchainImages = BUFFER_COUNT;
	if ((surfCaps.maxImageCount > 0) && (desiredNumberOfSwapchainImages > surfCaps.maxImageCount))
	{
		desiredNumberOfSwapchainImages = surfCaps.maxImageCount;
	}

	// Find the transformation of the surface
	VkSurfaceTransformFlagsKHR preTransform;
	if (surfCaps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
	{
		// We prefer a non-rotated transform
		preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	}
	else
	{
		preTransform = surfCaps.currentTransform;
	}

	// Find a supported composite alpha format (not all devices support alpha opaque)
	VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	// Simply select the first composite alpha format available
	VkCompositeAlphaFlagBitsKHR compositeAlphaFlags[] = {
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
	};
	for (int i = 0; i < 4; i++) {
		if (surfCaps.supportedCompositeAlpha & compositeAlphaFlags[i]) {
			compositeAlpha = compositeAlphaFlags[i];
			break;
		};
	}

	VkSwapchainCreateInfoKHR swapchainCI;
	memset(&swapchainCI, 0, sizeof(VkSwapchainCreateInfoKHR));
	swapchainCI.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainCI.surface = pSwapChain->surface;
	swapchainCI.minImageCount = desiredNumberOfSwapchainImages;
	swapchainCI.imageFormat = pSwapChain->colorFormat;
	swapchainCI.imageColorSpace = pSwapChain->colorSpace;
	swapchainCI.imageExtent.height = swapchainExtent.height;
	swapchainCI.imageExtent.width = swapchainExtent.width;
	swapchainCI.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchainCI.preTransform = (VkSurfaceTransformFlagBitsKHR)preTransform;
	swapchainCI.imageArrayLayers = 1;
	swapchainCI.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapchainCI.queueFamilyIndexCount = 0;
	swapchainCI.presentMode = swapchainPresentMode;
	// Setting oldSwapChain to the saved handle of the previous swapchain aids in resource reuse and makes sure that we can still present already acquired images
	swapchainCI.oldSwapchain = oldSwapchain;
	// Setting clipped to VK_TRUE allows the implementation to discard rendering outside of the surface area
	swapchainCI.clipped = VK_TRUE;
	swapchainCI.compositeAlpha = compositeAlpha;

	// Enable transfer source on swap chain images if supported
	if (surfCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) {
		swapchainCI.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	}

	// Enable transfer destination on swap chain images if supported
	if (surfCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) {
		swapchainCI.imageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	}

	fpCreateSwapchainKHR(gfx->device, &swapchainCI, 0, &pSwapChain->swapChain);
	// If an existing swap chain is re-created, destroy the old swap chain
	// This also cleans up all the presentable images
	if (oldSwapchain != VK_NULL_HANDLE)
	{
		for (uint32_t i = 0; i < BUFFER_COUNT; i++)
		{
			vkDestroyImageView(gfx->device, pSwapChain->buffers[i].view, 0);
		}
		fpDestroySwapchainKHR(gfx->device, oldSwapchain, 0);
	}
	uint32_t bufferCount = 0;
	fpGetSwapchainImagesKHR(gfx->device, pSwapChain->swapChain, &bufferCount, NULL);

	// Get the swap chain images
	fpGetSwapchainImagesKHR(gfx->device, pSwapChain->swapChain, &bufferCount, pSwapChain->images);

	// Get the swap chain buffers containing the image and imageview
	struct RenderTarget renderTarget;
	for (uint32_t i = 0; i < BUFFER_COUNT; i++)
	{
		VkImageViewCreateInfo colorAttachmentView;
		memset(&colorAttachmentView, 0, sizeof(VkImageViewCreateInfo));
		colorAttachmentView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		colorAttachmentView.pNext = NULL;
		colorAttachmentView.format = pSwapChain->colorFormat;
		colorAttachmentView.components.r = VK_COMPONENT_SWIZZLE_R;
		colorAttachmentView.components.g = VK_COMPONENT_SWIZZLE_G;
		colorAttachmentView.components.b = VK_COMPONENT_SWIZZLE_B;
		colorAttachmentView.components.a = VK_COMPONENT_SWIZZLE_A;
		colorAttachmentView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		colorAttachmentView.subresourceRange.baseMipLevel = 0;
		colorAttachmentView.subresourceRange.levelCount = 1;
		colorAttachmentView.subresourceRange.baseArrayLayer = 0;
		colorAttachmentView.subresourceRange.layerCount = 1;
		colorAttachmentView.viewType = VK_IMAGE_VIEW_TYPE_2D;
		colorAttachmentView.flags = 0;

		pSwapChain->buffers[i].image = pSwapChain->images[i];
		colorAttachmentView.image = pSwapChain->buffers[i].image;

		vkCreateImageView(gfx->device, &colorAttachmentView, 0, &pSwapChain->buffers[i].view);

		renderTarget.texture[i].image = pSwapChain->buffers[i].image;
		renderTarget.texture[i].view = pSwapChain->buffers[i].view;
	}

	VkAttachmentDescription attachment;
	attachment.format = gfx->swapChain.colorFormat;
	attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	attachment.flags = 0;

	VkAttachmentReference colorReference;
	colorReference.attachment = 0;
	colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpassDescription;
	subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDescription.colorAttachmentCount = 1;
	subpassDescription.pColorAttachments = &colorReference;
	subpassDescription.pDepthStencilAttachment = 0;
	subpassDescription.inputAttachmentCount = 0;
	subpassDescription.pInputAttachments = 0;
	subpassDescription.preserveAttachmentCount = 0;
	subpassDescription.pPreserveAttachments = 0;
	subpassDescription.pResolveAttachments = 0;
	subpassDescription.flags = 0;

	VkSubpassDependency dependency;
	memset(&dependency, 0, sizeof(VkSubpassDependency));
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
	dependency.dependencyFlags = 0;

	VkRenderPassCreateInfo renderPassInfo;
	memset(&renderPassInfo, 0, sizeof(VkRenderPassCreateInfo));
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &attachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpassDescription;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	vkCreateRenderPass(gfx->device, &renderPassInfo, 0, &gfx->renderPass.renderPass);

	VkFramebufferCreateInfo frameBufferCreateInfo;
	frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	frameBufferCreateInfo.pNext = NULL;
	frameBufferCreateInfo.renderPass = gfx->renderPass.renderPass;
	frameBufferCreateInfo.attachmentCount = 1;
	frameBufferCreateInfo.pAttachments = &attachment;
	frameBufferCreateInfo.width = gfx->width;
	frameBufferCreateInfo.height = gfx->height;
	frameBufferCreateInfo.layers = 1;
	frameBufferCreateInfo.flags = 0;

	// Create frame buffers for every swap chain image
	for (uint32_t i = 0; i < BUFFER_COUNT; i++)
	{
		frameBufferCreateInfo.pAttachments = &gfx->swapChain.buffers[i].view;
		vkCreateFramebuffer(gfx->device, &frameBufferCreateInfo, 0, &gfx->frameBuffers[i]);
	}

	gfx->renderPass.renderTargets[0] = renderTarget;
	gfx->renderPass.renderTargetCount = 1;
	gfx->renderPass.clearColor[0] = 0.2f;
	gfx->renderPass.clearColor[1] = 0.4f;
	gfx->renderPass.clearColor[2] = 0.6f;
	gfx->renderPass.clearColor[3] = 1.0f;
	gfx->renderPass.loadOperations[0] = VK_ATTACHMENT_LOAD_OP_CLEAR;
	// Create a frame buffer for every image in the swapchain

	VkCommandPoolCreateInfo cmdPoolInfo;
	cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolInfo.queueFamilyIndex = gfx->swapChain.queueNodeIndex;
	cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	cmdPoolInfo.pNext = 0;
	vkCreateCommandPool(gfx->device, &cmdPoolInfo, 0, &gfx->cmdPool);

	struct CommandBuffer* pCommandBuffer = &gfx->cmdBuffer;
	VkCommandBufferAllocateInfo commandBufferAllocateInfo;
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.commandPool = gfx->cmdPool;
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocateInfo.commandBufferCount = gfx->bufferCount;
	commandBufferAllocateInfo.pNext = 0;

	vkAllocateCommandBuffers(gfx->device, &commandBufferAllocateInfo, pCommandBuffer->drawCmdBuffers);

	VkFenceCreateInfo fenceCreateInfo;
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	// Create in signaled state so we don't wait on first render of each command buffer
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	fenceCreateInfo.pNext = 0;
	//(*cmd)->waitFences.resize((*cmd)->drawCmdBuffers.size());
	for (int i = 0; i < 3; i++)
	{
		vkCreateFence(gfx->device, &fenceCreateInfo, 0, &pCommandBuffer->waitFences[i]);
	}

	//return NxtResult::NXT_OK;

	return gfx;
}

int
gfx_load_resource_pack(struct gfx* gfx, const struct resource_pack* pack)
{
	/*
	bg_load(&gfx->bg, pack);

	gfx->sprite_shadow.program = load_shader(pack->shaders.glsl.shadow, attr_bindings);
	if (gfx->sprite_shadow.program == 0)
		goto glsl_shadow_failed;

	gfx->sprite_shadow.uAspectRatio = glGetUniformLocation(gfx->sprite_shadow.program, "uAspectRatio");
	gfx->sprite_shadow.uPosCameraSpace = glGetUniformLocation(gfx->sprite_shadow.program, "uPosCameraSpace");
	gfx->sprite_shadow.uDir = glGetUniformLocation(gfx->sprite_shadow.program, "uDir");
	gfx->sprite_shadow.uSize = glGetUniformLocation(gfx->sprite_shadow.program, "uSize");
	gfx->sprite_shadow.uAnim = glGetUniformLocation(gfx->sprite_shadow.program, "uAnim");
	gfx->sprite_shadow.sNM = glGetUniformLocation(gfx->sprite_shadow.program, "sNM");

	gfx->sprite_mat.program = load_shader(pack->shaders.glsl.sprite, attr_bindings);
	if (gfx->sprite_mat.program == 0)
		goto glsl_sprite_failed;

	gfx->sprite_mat.uAspectRatio = glGetUniformLocation(gfx->sprite_mat.program, "uAspectRatio");
	gfx->sprite_mat.uPosCameraSpace = glGetUniformLocation(gfx->sprite_mat.program, "uPosCameraSpace");
	gfx->sprite_mat.uDir = glGetUniformLocation(gfx->sprite_mat.program, "uDir");
	gfx->sprite_mat.uSize = glGetUniformLocation(gfx->sprite_mat.program, "uSize");
	gfx->sprite_mat.uAnim = glGetUniformLocation(gfx->sprite_mat.program, "uAnim");
	gfx->sprite_mat.sCol = glGetUniformLocation(gfx->sprite_mat.program, "sCol");
	gfx->sprite_mat.sNM = glGetUniformLocation(gfx->sprite_mat.program, "sNM");

	return 0;

glsl_sprite_failed:
	glDeleteProgram(gfx->sprite_shadow.program);
glsl_shadow_failed:
	return -1;*/

	return 0;
}

void
gfx_step_anim(struct gfx* gfx, int sim_tick_rate)
{
	static int frame_update;
	if (frame_update-- == 0)
	{
		frame_update = 3;

		/*
		gfx->head0_base.frame++;
		if (gfx->head0_base.frame >= gfx->head0_base.tile_count)
			gfx->head0_base.frame = 0;

		gfx->body0_base.frame++;
		if (gfx->body0_base.frame >= gfx->body0_base.tile_count)
			gfx->body0_base.frame = 0;*/
	}
}


void
gfx_deinit(void)
{
	glfwTerminate();
}

void
gfx_destroy(struct gfx* gfx)
{
	/*
	glDeleteTextures(1, &gfx->body0_base.texDiffuse);
	glDeleteTextures(1, &gfx->body0_base.texNM);

	glDeleteTextures(1, &gfx->head0_gather.texDiffuse);
	glDeleteTextures(1, &gfx->head0_gather.texNM);

	glDeleteTextures(1, &gfx->head0_base.texDiffuse);
	glDeleteTextures(1, &gfx->head0_base.texNM);

	glDeleteProgram(gfx->sprite_mat.program);
	glDeleteProgram(gfx->sprite_shadow.program);
	glDeleteBuffers(1, &gfx->sprite_mesh.ibo);
	glDeleteBuffers(1, &gfx->sprite_mesh.vbo);

	bg_deinit(&gfx->bg);

	glfwDestroyWindow(gfx->window);
	FREE(gfx);*/
}


void
gfx_poll_input(struct gfx* gfx, struct input* input)
{
	glfwPollEvents();
	*input = gfx->input_buffer;
	gfx->input_buffer.scroll = 0;  /* Clear deltas */

	if (glfwWindowShouldClose(gfx->window))
		input->quit = 1;
}

struct command
	gfx_input_to_command(
		struct command prev,
		const struct input* input,
		const struct gfx* gfx,
		const struct camera* camera,
		struct qwpos snake_head)
{
	float a, d, dx, dy;
	int max_dist;
	struct spos snake_head_screen;

	/* world -> camera space */
	struct qwpos pos_cameraSpace = {
		qw_mul(qw_sub(snake_head.x, camera->pos.x), camera->scale),
		qw_mul(qw_sub(snake_head.y, camera->pos.y), camera->scale)
	};

	/* camera space -> screen space + keep aspect ratio */
	if (gfx->width < gfx->height)
	{
		int pad = (gfx->height - gfx->width) / 2;
		snake_head_screen.x = qw_mul_to_int(pos_cameraSpace.x, make_qw(gfx->width)) + (gfx->width / 2);
		snake_head_screen.y = qw_mul_to_int(pos_cameraSpace.y, make_qw(-gfx->width)) + (gfx->width / 2 + pad);
	}
	else
	{
		int pad = (gfx->width - gfx->height) / 2;
		snake_head_screen.x = qw_mul_to_int(pos_cameraSpace.x, make_qw(gfx->height)) + (gfx->height / 2 + pad);
		snake_head_screen.y = qw_mul_to_int(pos_cameraSpace.y, make_qw(-gfx->height)) + (gfx->height / 2);
	}

	/* Scale the speed vector to a quarter of the screen's size */
	max_dist = gfx->width > gfx->height ? gfx->height / 4 : gfx->width / 4;

	/* Calc angle and distance from mouse position and snake head position */
	dx = input->mousex - snake_head_screen.x;
	dy = snake_head_screen.y - input->mousey;
	a = atan2(dy, dx);
	d = sqrt(dx * dx + dy * dy);
	if (d > max_dist)
		d = max_dist;

	return command_make(prev, a, d, input->boost ? COMMAND_ACTION_BOOST : COMMAND_ACTION_NONE);
}

void
gfx_draw_world(struct gfx* gfx, const struct world* world, const struct camera* camera)
{
	BeginRecording(gfx);
	CmdBeginRenderPass(gfx);
	CmdEndRenderPass(gfx);
	EndRecording(gfx);
	SubmitCommandBuffers(gfx);
}

static
int
BeginRecording(struct gfx* gfx) {

	struct CommandBuffer* pCommandBuffer = &gfx->cmdBuffer;

	vkAcquireNextImageKHR(gfx->device, gfx->swapChain.swapChain, UINT64_MAX, gfx->presentComplete[gfx->currentBuffer], (VkFence)NULL, &gfx->currentBuffer);
	int i = gfx->currentBuffer;

	// Wait for the fence to signal that command buffer has finished executing
	vkWaitForFences(gfx->device, 1, &pCommandBuffer->waitFences[i], VK_TRUE, DEFAULT_FENCE_TIMEOUT);
	vkResetFences(gfx->device, 1, &pCommandBuffer->waitFences[i]);

	vkResetCommandBuffer(pCommandBuffer->drawCmdBuffers[i], 0);

	VkCommandBufferBeginInfo cmdBufInfo;
	memset(&cmdBufInfo, 0, sizeof(VkCommandBufferBeginInfo));
	cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBufInfo.pNext = NULL;

	vkBeginCommandBuffer(pCommandBuffer->drawCmdBuffers[i], &cmdBufInfo);

	// Ending the render pass will add an implicit barrier transitioning the frame buffer color attachment to
	// VK_IMAGE_LAYOUT_PRESENT_SRC_KHR for presenting it to the windowing system

	return 1;
}

static
int
CmdBeginRenderPass(struct gfx* gfx) {

	struct CommandBuffer* cmdBuffer = &gfx->cmdBuffer;
	struct RenderPass* renderPass = &gfx->renderPass;
	int curr = gfx->currentBuffer;

	VkClearValue clearValues;
	memset(&clearValues, 0, sizeof(VkClearValue));
	clearValues.color.float32[0] = 0.2f;
	clearValues.color.float32[1] = 0.4f;
	clearValues.color.float32[2] = 0.6f;
	clearValues.color.float32[3] = 1.0f;// = { { 0.2f, 0.4f, 0.6f, 1.0f } };
		
	VkRenderPassBeginInfo renderPassBeginInfo;
	renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassBeginInfo.pNext = 0;
	renderPassBeginInfo.framebuffer = gfx->frameBuffers[curr];
	renderPassBeginInfo.renderPass = renderPass->renderPass;
	renderPassBeginInfo.renderArea.offset.x = 0;
	renderPassBeginInfo.renderArea.offset.y = 0;
	renderPassBeginInfo.renderArea.extent.width = gfx->width;
	renderPassBeginInfo.renderArea.extent.height = gfx->height;
	renderPassBeginInfo.clearValueCount = 1;
	renderPassBeginInfo.pClearValues = &clearValues;
	//info.pDepthAttachment = (renderPass->depthTarget) ? &depthAttachment : nullptr;

	VkImageMemoryBarrier image_memory_barrier;
	memset(&image_memory_barrier, 0, sizeof(VkImageMemoryBarrier));
	image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	image_memory_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	image_memory_barrier.image = renderPass->renderTargets[0].texture[curr].image;
	image_memory_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	image_memory_barrier.subresourceRange.baseMipLevel = 0;
	image_memory_barrier.subresourceRange.levelCount = 1;
	image_memory_barrier.subresourceRange.baseArrayLayer = 0;
	image_memory_barrier.subresourceRange.layerCount = 1;

	vkCmdPipelineBarrier(cmdBuffer->drawCmdBuffers[curr], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,  // srcStageMask
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // dstStageMask
		0,
		0,
		0,
		0,
		0,
		1, // imageMemoryBarrierCount
		&image_memory_barrier // pImageMemoryBarriers
	);

	// Set target frame buffer
	vkCmdBeginRenderPass(cmdBuffer->drawCmdBuffers[curr], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
	//(cmdBuffer->drawCmdBuffers[curr], &info);

	VkViewport viewport;
	viewport.width = gfx->width;
	viewport.height = gfx->height;
	viewport.minDepth = 0.1f;
	viewport.maxDepth = 1.0f;
	viewport.x = 0.0f;
	viewport.y = 0.0f;

	vkCmdSetViewport(cmdBuffer->drawCmdBuffers[curr], 0, 1, &viewport);

	VkRect2D scissor;
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = gfx->width;
	scissor.extent.height = gfx->height;

	vkCmdSetScissor(cmdBuffer->drawCmdBuffers[curr], 0, 1, &scissor);

	return 1;
}

static
int
CmdEndRenderPass(struct gfx* gfx) {

	struct RenderPass* pRenderPass = &gfx->renderPass;
	struct CommandBuffer* pCommandBuffer = &gfx->cmdBuffer;

	int i = gfx->currentBuffer;
	vkCmdEndRenderPass(pCommandBuffer->drawCmdBuffers[i]);

	VkImageMemoryBarrier image_memory_barrier;
	memset(&image_memory_barrier, 0, sizeof(VkImageMemoryBarrier));
	image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	image_memory_barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	image_memory_barrier.image = pRenderPass->renderTargets[0].texture[i].image;
	image_memory_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	image_memory_barrier.subresourceRange.baseMipLevel = 0;
	image_memory_barrier.subresourceRange.levelCount = 1;
	image_memory_barrier.subresourceRange.baseArrayLayer = 0;
	image_memory_barrier.subresourceRange.layerCount = 1;

	vkCmdPipelineBarrier(pCommandBuffer->drawCmdBuffers[i],
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,  // srcStageMask
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, // dstStageMask
		0,
		0,
		0,
		0,
		0,
		1, // imageMemoryBarrierCount
		&image_memory_barrier // pImageMemoryBarriers
	);

	return 1;
}

static int
EndRecording(struct gfx* gfx) {
	//assert(commandBuffer != VK_NULL_HANDLE);

	struct CommandBuffer* pCommandBuffer = &gfx->cmdBuffer;

	int i = gfx->currentBuffer;
	vkEndCommandBuffer(pCommandBuffer->drawCmdBuffers[i]);

	return 1;
}

static int
SubmitCommandBuffers(struct gfx* gfx) {

	struct CommandBuffer* pCommandBuffer = &gfx->cmdBuffer;

	uint32_t idx = gfx->currentBuffer;
	VkCommandBuffer vkCmdBuffers[128];
	//for (int i = 0; i < numCommandBuffers; i++) {
	//	vkCmdBuffers[i] = cmdBuffers[i]->drawCmdBuffers[idx];
	//}

	vkCmdBuffers[0] = pCommandBuffer->drawCmdBuffers[idx];

	// Pipeline stage at which the queue submission will wait (via pWaitSemaphores)
	VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	// The submit info structure specifies a command buffer queue submission batch
	VkSubmitInfo submitInfo;
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pWaitDstStageMask = &waitStageMask;               // Pointer to the list of pipeline stages that the semaphore waits will occur at
	submitInfo.pWaitSemaphores = &gfx->presentComplete[idx];      // Semaphore(s) to wait upon before the submitted command buffer starts executing
	submitInfo.waitSemaphoreCount = 1;                           // One wait semaphore
	submitInfo.pSignalSemaphores = &gfx->renderComplete[idx];     // Semaphore(s) to be signaled when command buffers have completed
	submitInfo.signalSemaphoreCount = 1;                         // One signal semaphore
	submitInfo.pCommandBuffers = vkCmdBuffers; // Command buffers(s) to execute in this batch (submission)
	submitInfo.commandBufferCount = 1;
	submitInfo.pNext = 0;
	// One command buffer

    // Submit to the queue
	vkQueueSubmit(gfx->queue, 1, &submitInfo, pCommandBuffer->waitFences[idx]);

	VkPresentInfoKHR presentInfo;
	memset(&presentInfo, 0, sizeof(VkPresentInfoKHR));
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = NULL;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &gfx->swapChain.swapChain;
	presentInfo.pImageIndices = &idx;
	// Check if a wait semaphore has been specified to wait for before presenting the image
	if (gfx->renderComplete[idx] != VK_NULL_HANDLE)
	{
		presentInfo.pWaitSemaphores = &gfx->renderComplete[idx];
		presentInfo.waitSemaphoreCount = 1;
	}
	vkQueuePresentKHR(gfx->queue, &presentInfo);

	return 1;
}
