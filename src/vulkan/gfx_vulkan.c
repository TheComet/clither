
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.hpp>
#include <unordered_map>

#include "VulkanRenderTargetManager.h"
#include "../ResourceManager.h"

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

namespace nxt {

	struct WindowHandle {
		HWND hWnd;
		HINSTANCE hInstance;
	};

	struct Texture {
		VkImage image;
		VkImageView view;
	};

	struct SwapChain {
		VkSurfaceKHR surface;
		VkFormat colorFormat;
		VkColorSpaceKHR colorSpace;
		VkSwapchainKHR swapChain = VK_NULL_HANDLE;
		uint32_t imageCount;
		std::vector<VkImage> images;
		std::vector<Texture> buffers;
		uint32_t queueNodeIndex = UINT32_MAX;
	} *pSwapChain;

	struct Renderer {
		VkInstance instance;
		std::vector<std::string> supportedInstanceExtensions;
		std::vector<VkQueueFamilyProperties> queueFamilyProperties;
		std::vector<std::string> supportedExtensions;

		VkPhysicalDevice physicalDevice;
		VkPhysicalDeviceProperties deviceProperties;
		VkPhysicalDeviceFeatures deviceFeatures;
		VkPhysicalDeviceMemoryProperties deviceMemoryProperties;
		VkPhysicalDeviceFeatures enabledFeatures{};
		std::vector<const char*> enabledDeviceExtensions;
		std::vector<const char*> enabledInstanceExtensions;

		void* deviceCreatepNextChain = nullptr;
		VkDevice device;
		VkQueue queue;

		/** @brief Pipeline stages used to wait at for graphics queue submissions */
		VkPipelineStageFlags submitPipelineStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		// Contains command buffers and semaphores to be presented to the queue
		VkSubmitInfo submitInfo;
		// Command buffers used for rendering
		std::vector<VkCommandBuffer> drawCmdBuffers;
		// Global render pass for frame buffer writes

		RenderPass* renderPass;
		// List of available frame buffers (same as number of swap chain images)
		std::vector<VkFramebuffer>frameBuffers;
		// Active frame buffer index
		uint32_t currentBuffer = 0;
		uint32_t bufferCount;

		VkSemaphore renderComplete[3];
		VkSemaphore presentComplete[3];
		uint32_t presentIdx = 0;

		// Wraps the swap chain to present images (framebuffers) to the windowing system
		SwapChain* pSwapChain;
		VkFormat depthFormat;
		VkCommandPool cmdPool;
		unsigned int width, height;

		struct
		{
			uint32_t graphics;
			uint32_t compute;
			uint32_t transfer;
		} queueFamilyIndices;
		// Synchronization semaphores

	} *pRenderer;

	struct CommandBuffer {
		std::vector<VkFence> waitFences;
		std::vector<VkCommandBuffer> drawCmdBuffers;
	};

	struct RenderPass {
		uint32_t renderTargetCount;
		RenderTarget* pRenderTargets[8];
		RenderTarget* depthTarget;
		LoadOperations loadOperations[8];
		float clearColor[4];

		RenderPass** renderPass;
	};

	ResourceManager* resourceManager;
	IRenderTargetManager* iRenderTargetManager;
};

using namespace nxt;

VkAccessFlagBits ConvertNxtResourceState(ResourceState resourceState) {
	switch (resourceState) {
	case ResourceState::RENDER_TARGET:
		return VkAccessFlagBits::VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	case ResourceState::PRESENT:
		return VkAccessFlagBits::VK_ACCESS_NONE;
	}
}

VkFormat ConvertNxtFormat(Format format) {
	switch (format) {
	case Format::R8G8B8A8_UNORM:
		return VkFormat::VK_FORMAT_R8G8B8A8_UNORM;
	case Format::D32:
		return VkFormat::VK_FORMAT_D32_SFLOAT;
	}
}

uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(pRenderer->physicalDevice, &memProperties);

	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
		if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}
}

uint32_t getQueueFamilyIndex(const std::vector<VkQueueFamilyProperties>& queueFamilyProperties, VkQueueFlagBits queueFlags)
{
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
	}

	// For other queue types or if no separate compute queue is present, return the first one to support the requested flags
	for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++)
	{
		if (queueFamilyProperties[i].queueFlags & queueFlags)
		{
			return i;
		}
	}

	throw std::runtime_error("Could not find a matching queue family index");
}

VkResult createLogicalDevice(VkPhysicalDeviceFeatures enabledFeatures, std::vector<const char*> enabledExtensions, void* pNextChain, bool useSwapChain, VkQueueFlags requestedQueueTypes)
{
	// Desired queues need to be requested upon logical device creation
	// Due to differing queue family configurations of Vulkan implementations this can be a bit tricky, especially if the application
	// requests different queue types

	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos{};

	// Get queue family indices for the requested queue family types
	// Note that the indices may overlap depending on the implementation

	const float defaultQueuePriority(0.0f);

	// Graphics queue
	if (requestedQueueTypes & VK_QUEUE_GRAPHICS_BIT)
	{
		pRenderer->queueFamilyIndices.graphics = getQueueFamilyIndex(pRenderer->queueFamilyProperties, VK_QUEUE_GRAPHICS_BIT);
		VkDeviceQueueCreateInfo queueInfo{};
		queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueInfo.queueFamilyIndex = pRenderer->queueFamilyIndices.graphics;
		queueInfo.queueCount = 1;
		queueInfo.pQueuePriorities = &defaultQueuePriority;
		queueCreateInfos.push_back(queueInfo);
	}
	else
	{
		pRenderer->queueFamilyIndices.graphics = 0;
	}

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

	// Create the logical device representation
	std::vector<const char*> deviceExtensions(enabledExtensions);
	if (useSwapChain)
	{
		// If the device will be used for presenting to a display via a swapchain we need to request the swapchain extension
		deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
	}

	deviceExtensions.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);

#if defined(VK_USE_PLATFORM_MACOS_MVK) && (VK_HEADER_VERSION >= 216)
	deviceExtensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
#endif

	VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering_feature{};
	dynamic_rendering_feature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
	dynamic_rendering_feature.dynamicRendering = VK_TRUE;


	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());;
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
	deviceCreateInfo.pEnabledFeatures = &enabledFeatures;
	deviceCreateInfo.pNext = &dynamic_rendering_feature;

	// If a pNext(Chain) has been passed, we need to add it to the device creation info
	VkPhysicalDeviceFeatures2 physicalDeviceFeatures2{};
	if (pNextChain) {
		physicalDeviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		physicalDeviceFeatures2.features = enabledFeatures;
		physicalDeviceFeatures2.pNext = pNextChain;
		deviceCreateInfo.pEnabledFeatures = nullptr;
		deviceCreateInfo.pNext = &physicalDeviceFeatures2;
	}

	// Enable the debug marker extension if it is present (likely meaning a debugging tool is present)
	//if (extensionSupported(VK_EXT_DEBUG_MARKER_EXTENSION_NAME))
	//{
	//	deviceExtensions.push_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
	//	enableDebugMarkers = true;
	//}

	if (deviceExtensions.size() > 0)
	{
		for (const char* enabledExtension : deviceExtensions)
		{
			//if (!extensionSupported(enabledExtension)) {
				//std::cerr << "Enabled device extension \"" << enabledExtension << "\" is not present at device level\n";
			//}
		}

		deviceCreateInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
		deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
	}

	pRenderer->enabledFeatures = enabledFeatures;

	VkResult result = vkCreateDevice(pRenderer->physicalDevice, &deviceCreateInfo, nullptr, &pRenderer->device);
	if (result != VK_SUCCESS)
	{
		return result;
	}

	return result;
}

VkBool32 getSupportedDepthFormat(VkPhysicalDevice physicalDevice, VkFormat* depthFormat)
{
	// Since all depth formats may be optional, we need to find a suitable depth format to use
	// Start with the highest precision packed format
	std::vector<VkFormat> depthFormats = {
		VK_FORMAT_D32_SFLOAT_S8_UINT,
		VK_FORMAT_D32_SFLOAT,
		VK_FORMAT_D24_UNORM_S8_UINT,
		VK_FORMAT_D16_UNORM_S8_UINT,
		VK_FORMAT_D16_UNORM
	};

	for (auto& format : depthFormats)
	{
		VkFormatProperties formatProps;
		vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &formatProps);
		// Format must support depth stencil attachment for optimal tiling
		if (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
		{
			*depthFormat = format;
			return true;
		}
	}

	return false;
}


Texture* nxt::CreateTexture(TextureDesc textureDesc, bool depth, bool upload) {

	VkImageCreateInfo vkImageCreateInfo{};
	vkImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	vkImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	vkImageCreateInfo.extent.width = textureDesc.width;
	vkImageCreateInfo.extent.height = textureDesc.height;
	vkImageCreateInfo.extent.depth = 1;
	vkImageCreateInfo.mipLevels = 1;
	vkImageCreateInfo.arrayLayers = 1;
	vkImageCreateInfo.format = ConvertNxtFormat(textureDesc.format);
	vkImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	vkImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	vkImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	vkImageCreateInfo.usage = textureDesc.flags;

	Texture* texture = new Texture();
	vkCreateImage(pRenderer->device, &vkImageCreateInfo, nullptr, &texture->image);

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(pRenderer->device, texture->image, &memRequirements);

	VkMemoryAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VkDeviceMemory imageMemory;
	if (vkAllocateMemory(pRenderer->device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate image memory!");
	}

	vkBindImageMemory(pRenderer->device, texture->image, imageMemory, 0);
	//textureDesc.flags = (renderPassDesc.rtDesc[i].depth) ? D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL : D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	VkImageViewCreateInfo vkImageViewInfo{};
	vkImageViewInfo.format = ConvertNxtFormat(textureDesc.format);
	vkImageViewInfo.image = texture->image;
	vkImageViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	vkImageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	vkImageViewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	vkImageViewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	vkImageViewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	vkImageViewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	vkImageViewInfo.subresourceRange.aspectMask = (depth) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
	vkImageViewInfo.subresourceRange.baseMipLevel = 0;
	vkImageViewInfo.subresourceRange.levelCount = 1;
	vkImageViewInfo.subresourceRange.baseArrayLayer = 0;
	vkImageViewInfo.subresourceRange.layerCount = 1;

	vkCreateImageView(pRenderer->device, &vkImageViewInfo, nullptr, &texture->view);

	return texture;
}


NxtResult nxt::InitializeRenderer(RendererDesc rendererDesc) {

	pRenderer = new Renderer();
	pSwapChain = new SwapChain();

	pRenderer->bufferCount = rendererDesc.bufferCount;

	// Validation can also be forced via a define
#if defined(_VALIDATION)
	this->settings.validation = true;
#endif

	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "ParkemupDerby";
	appInfo.pEngineName = "NXT";
	appInfo.apiVersion = VK_API_VERSION_1_3;

	std::vector<const char*> instanceExtensions = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME };

	// Get extensions supported by the instance and store for later use
	uint32_t extCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
	if (extCount > 0)
	{
		std::vector<VkExtensionProperties> extensions(extCount);
		if (vkEnumerateInstanceExtensionProperties(nullptr, &extCount, &extensions.front()) == VK_SUCCESS)
		{
			for (VkExtensionProperties extension : extensions)
			{
				pRenderer->supportedInstanceExtensions.push_back(extension.extensionName);
			}
		}
	}

	// Enabled requested instance extensions
	if (pRenderer->enabledInstanceExtensions.size() > 0)
	{
		for (const char* enabledExtension : pRenderer->enabledInstanceExtensions)
		{
			// Output message if requested extension is not available
			if (std::find(pRenderer->supportedInstanceExtensions.begin(), pRenderer->supportedInstanceExtensions.end(), enabledExtension) == pRenderer->supportedInstanceExtensions.end())
			{
				//std::cerr << "Enabled instance extension \"" << enabledExtension << "\" is not present at instance level\n";
			}
			instanceExtensions.push_back(enabledExtension);
		}
	}

	VkInstanceCreateInfo instanceCreateInfo = {};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pNext = NULL;
	instanceCreateInfo.pApplicationInfo = &appInfo;

#if defined(VK_USE_PLATFORM_MACOS_MVK) && (VK_HEADER_VERSION >= 216)
	instanceCreateInfo.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

	if (instanceExtensions.size() > 0)
	{
		if (true)
		{
			instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
			instanceExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
		}
		instanceCreateInfo.enabledExtensionCount = (uint32_t)instanceExtensions.size();
		instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();
	}

	// The VK_LAYER_KHRONOS_validation contains all current validation functionality.
	// Note that on Android this layer requires at least NDK r20
	const char* validationLayerName = "VK_LAYER_KHRONOS_validation";
	if (true)
	{
		// Check if this layer is available at instance level
		uint32_t instanceLayerCount;
		vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr);
		std::vector<VkLayerProperties> instanceLayerProperties(instanceLayerCount);
		vkEnumerateInstanceLayerProperties(&instanceLayerCount, instanceLayerProperties.data());
		bool validationLayerPresent = false;
		for (VkLayerProperties layer : instanceLayerProperties) {
			if (strcmp(layer.layerName, validationLayerName) == 0) {
				validationLayerPresent = true;
				break;
			}
		}
		if (validationLayerPresent) {
			instanceCreateInfo.ppEnabledLayerNames = &validationLayerName;
			instanceCreateInfo.enabledLayerCount = 1;
		}
		else {
		}
	}

	vkCreateInstance(&instanceCreateInfo, nullptr, &pRenderer->instance);


	// Physical device
	uint32_t gpuCount = 0;
	// Get number of available physical devices
	vkEnumeratePhysicalDevices(pRenderer->instance, &gpuCount, nullptr);
	if (gpuCount == 0) {
		//vks::tools::exitFatal("No device with Vulkan support found", -1);
		//return false;
	}
	// Enumerate devices
	std::vector<VkPhysicalDevice> physicalDevices(gpuCount);
	vkEnumeratePhysicalDevices(pRenderer->instance, &gpuCount, physicalDevices.data());
	//if (err) {
	//	vks::tools::exitFatal("Could not enumerate physical devices : \n" + vks::tools::errorString(err), err);
	//	return false;
	//}

	// GPU selection

	// Select physical device to be used for the Vulkan example
	// Defaults to the first device unless specified by command line
	uint32_t selectedDevice = 1;

	pRenderer->physicalDevice = physicalDevices[selectedDevice];

	// Store properties (including limits), features and memory properties of the physical device (so that examples can check against them)
	vkGetPhysicalDeviceProperties(pRenderer->physicalDevice, &pRenderer->deviceProperties);
	vkGetPhysicalDeviceFeatures(pRenderer->physicalDevice, &pRenderer->deviceFeatures);
	vkGetPhysicalDeviceMemoryProperties(pRenderer->physicalDevice, &pRenderer->deviceMemoryProperties);

	// Derived examples can override this to set actual features (based on above readings) to enable for logical device creation
	//getEnabledFeatures();

	// Vulkan device creation
	// This is handled by a separate class that gets a logical device representation
	// and encapsulates functions related to a device

	uint32_t queueFamilyCount;
	vkGetPhysicalDeviceQueueFamilyProperties(pRenderer->physicalDevice, &queueFamilyCount, nullptr);
	assert(queueFamilyCount > 0);
	pRenderer->queueFamilyProperties.resize(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(pRenderer->physicalDevice, &queueFamilyCount, pRenderer->queueFamilyProperties.data());

	// Get list of supported extensions
	extCount = 0;
	vkEnumerateDeviceExtensionProperties(pRenderer->physicalDevice, nullptr, &extCount, nullptr);
	if (extCount > 0)
	{
		std::vector<VkExtensionProperties> extensions(extCount);
		if (vkEnumerateDeviceExtensionProperties(pRenderer->physicalDevice, nullptr, &extCount, &extensions.front()) == VK_SUCCESS)
		{
			for (auto ext : extensions)
			{
				pRenderer->supportedExtensions.push_back(ext.extensionName);
			}
		}
	}

	createLogicalDevice(pRenderer->enabledFeatures, pRenderer->enabledDeviceExtensions, pRenderer->deviceCreatepNextChain, true, VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT);

	// Get a graphics queue from the device
	vkGetDeviceQueue(pRenderer->device, pRenderer->queueFamilyIndices.graphics, 0, &pRenderer->queue);

	// Find a suitable depth format
	VkBool32 validDepthFormat = getSupportedDepthFormat(pRenderer->physicalDevice, &pRenderer->depthFormat);
	assert(validDepthFormat);

	// Create synchronization objects
	VkSemaphoreCreateInfo semaphoreCreateInfo{};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	// Create a semaphore used to synchronize image presentation
	// Ensures that the image is displayed before we start submitting new commands to the queue

	for (int i = 0; i < 3; i++) {
		vkCreateSemaphore(pRenderer->device, &semaphoreCreateInfo, nullptr, &pRenderer->presentComplete[i]);
		// Create a semaphore used to synchronize command submission
		// Ensures that the image is not presented until all commands have been submitted and executed
		vkCreateSemaphore(pRenderer->device, &semaphoreCreateInfo, nullptr, &pRenderer->renderComplete[i]);
	}
	// Set up submit info structure
	// Semaphores will stay the same during application lifetime
	// Command buffer submission info is set by each example

	//this->physicalDevice = physicalDevice;
	//this->device = device;
	fpGetPhysicalDeviceSurfaceSupportKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceSupportKHR>(vkGetInstanceProcAddr(pRenderer->instance, "vkGetPhysicalDeviceSurfaceSupportKHR"));
	fpGetPhysicalDeviceSurfaceCapabilitiesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(vkGetInstanceProcAddr(pRenderer->instance, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));
	fpGetPhysicalDeviceSurfaceFormatsKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceFormatsKHR>(vkGetInstanceProcAddr(pRenderer->instance, "vkGetPhysicalDeviceSurfaceFormatsKHR"));
	fpGetPhysicalDeviceSurfacePresentModesKHR = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfacePresentModesKHR>(vkGetInstanceProcAddr(pRenderer->instance, "vkGetPhysicalDeviceSurfacePresentModesKHR"));

	fpCreateSwapchainKHR = reinterpret_cast<PFN_vkCreateSwapchainKHR>(vkGetDeviceProcAddr(pRenderer->device, "vkCreateSwapchainKHR"));
	fpDestroySwapchainKHR = reinterpret_cast<PFN_vkDestroySwapchainKHR>(vkGetDeviceProcAddr(pRenderer->device, "vkDestroySwapchainKHR"));
	fpGetSwapchainImagesKHR = reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(vkGetDeviceProcAddr(pRenderer->device, "vkGetSwapchainImagesKHR"));
	fpAcquireNextImageKHR = reinterpret_cast<PFN_vkAcquireNextImageKHR>(vkGetDeviceProcAddr(pRenderer->device, "vkAcquireNextImageKHR"));
	fpQueuePresentKHR = reinterpret_cast<PFN_vkQueuePresentKHR>(vkGetDeviceProcAddr(pRenderer->device, "vkQueuePresentKHR"));
	fpWaitSemaphoresKHR = reinterpret_cast<PFN_vkWaitSemaphoresKHR>(vkGetDeviceProcAddr(pRenderer->device, "vkWaitSemaphoresKHR"));
	fpGetSemaphoreCounterValueKHR = reinterpret_cast<PFN_vkGetSemaphoreCounterValueKHR>(vkGetDeviceProcAddr(pRenderer->device, "vkGetSemaphoreCounterValueKHR"));

#if defined(_WIN32)
	instanceExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);

	VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {};
	surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	surfaceCreateInfo.hinstance = (HINSTANCE)rendererDesc.pWindowHandle->hInstance;
	surfaceCreateInfo.hwnd = (HWND)rendererDesc.pWindowHandle->hWnd;
	vkCreateWin32SurfaceKHR(pRenderer->instance, &surfaceCreateInfo, nullptr, &pSwapChain->surface);
#endif

	// Get available queue family properties
	uint32_t queueCount;
	vkGetPhysicalDeviceQueueFamilyProperties(pRenderer->physicalDevice, &queueCount, NULL);
	assert(queueCount >= 1);

	std::vector<VkQueueFamilyProperties> queueProps(queueCount);
	vkGetPhysicalDeviceQueueFamilyProperties(pRenderer->physicalDevice, &queueCount, queueProps.data());

	// Iterate over each queue to learn whether it supports presenting:
	// Find a queue with present support
	// Will be used to present the swap chain images to the windowing system
	std::vector<VkBool32> supportsPresent(queueCount);
	for (uint32_t i = 0; i < queueCount; i++)
	{
		fpGetPhysicalDeviceSurfaceSupportKHR(pRenderer->physicalDevice, i, pSwapChain->surface, &supportsPresent[i]);
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
	fpGetPhysicalDeviceSurfaceFormatsKHR(pRenderer->physicalDevice, pSwapChain->surface, &formatCount, NULL);
	assert(formatCount > 0);

	std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
	fpGetPhysicalDeviceSurfaceFormatsKHR(pRenderer->physicalDevice, pSwapChain->surface, &formatCount, surfaceFormats.data());

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
		bool found_B8G8R8A8_UNORM = false;
		for (auto&& surfaceFormat : surfaceFormats)
		{
			if (surfaceFormat.format == VK_FORMAT_B8G8R8A8_UNORM)
			{
				pSwapChain->colorFormat = surfaceFormat.format;
				pSwapChain->colorSpace = surfaceFormat.colorSpace;
				found_B8G8R8A8_UNORM = true;
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
	VkSwapchainKHR oldSwapchain = pSwapChain->swapChain;

	// Get physical device surface properties and formats
	VkSurfaceCapabilitiesKHR surfCaps;
	fpGetPhysicalDeviceSurfaceCapabilitiesKHR(pRenderer->physicalDevice, pSwapChain->surface, &surfCaps);

	// Get available present modes
	uint32_t presentModeCount;
	fpGetPhysicalDeviceSurfacePresentModesKHR(pRenderer->physicalDevice, pSwapChain->surface, &presentModeCount, NULL);
	assert(presentModeCount > 0);

	std::vector<VkPresentModeKHR> presentModes(presentModeCount);
	fpGetPhysicalDeviceSurfacePresentModesKHR(pRenderer->physicalDevice, pSwapChain->surface, &presentModeCount, presentModes.data());

	VkExtent2D swapchainExtent = {};
	// If width (and height) equals the special value 0xFFFFFFFF, the size of the surface will be set by the swapchain
	if (surfCaps.currentExtent.width == (uint32_t)-1)
	{
		// If the surface size is undefined, the size is set to
		// the size of the images requested.
		swapchainExtent.width = rendererDesc.width;
		swapchainExtent.height = rendererDesc.height;
	}
	else
	{
		// If the surface size is defined, the swap chain size must match
		swapchainExtent = surfCaps.currentExtent;
		rendererDesc.width = surfCaps.currentExtent.width;
		rendererDesc.height = surfCaps.currentExtent.height;
	}

	pRenderer->width = rendererDesc.width;
	pRenderer->height = rendererDesc.height;
	// Select a present mode for the swapchain

	// The VK_PRESENT_MODE_FIFO_KHR mode must always be present as per spec
	// This mode waits for the vertical blank ("v-sync")
	VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;

	// If v-sync is not requested, try to find a mailbox mode
	// It's the lowest latency non-tearing present mode available
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
	}

	// Determine the number of images
	uint32_t desiredNumberOfSwapchainImages = rendererDesc.bufferCount;
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
	std::vector<VkCompositeAlphaFlagBitsKHR> compositeAlphaFlags = {
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
		VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
		VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
	};
	for (auto& compositeAlphaFlag : compositeAlphaFlags) {
		if (surfCaps.supportedCompositeAlpha & compositeAlphaFlag) {
			compositeAlpha = compositeAlphaFlag;
			break;
		};
	}

	VkSwapchainCreateInfoKHR swapchainCI = {};
	swapchainCI.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainCI.surface = pSwapChain->surface;
	swapchainCI.minImageCount = desiredNumberOfSwapchainImages;
	swapchainCI.imageFormat = pSwapChain->colorFormat;
	swapchainCI.imageColorSpace = pSwapChain->colorSpace;
	swapchainCI.imageExtent = { swapchainExtent.width, swapchainExtent.height };
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

	fpCreateSwapchainKHR(pRenderer->device, &swapchainCI, nullptr, &pSwapChain->swapChain);
	pSwapChain->imageCount = rendererDesc.bufferCount;
	// If an existing swap chain is re-created, destroy the old swap chain
	// This also cleans up all the presentable images
	if (oldSwapchain != VK_NULL_HANDLE)
	{
		for (uint32_t i = 0; i < rendererDesc.bufferCount; i++)
		{
			vkDestroyImageView(pRenderer->device, pSwapChain->buffers[i].view, nullptr);
		}
		fpDestroySwapchainKHR(pRenderer->device, oldSwapchain, nullptr);
	}
	fpGetSwapchainImagesKHR(pRenderer->device, pSwapChain->swapChain, (uint32_t*)&rendererDesc.bufferCount, NULL);

	// Get the swap chain images
	pSwapChain->images.resize(rendererDesc.bufferCount);
	fpGetSwapchainImagesKHR(pRenderer->device, pSwapChain->swapChain, (uint32_t*)&rendererDesc.bufferCount, pSwapChain->images.data());

	// Get the swap chain buffers containing the image and imageview
	RenderTarget* renderTarget = new RenderTarget();
	pSwapChain->buffers.resize(rendererDesc.bufferCount);
	for (uint32_t i = 0; i < rendererDesc.bufferCount; i++)
	{
		VkImageViewCreateInfo colorAttachmentView = {};
		colorAttachmentView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		colorAttachmentView.pNext = NULL;
		colorAttachmentView.format = pSwapChain->colorFormat;
		colorAttachmentView.components = {
			VK_COMPONENT_SWIZZLE_R,
			VK_COMPONENT_SWIZZLE_G,
			VK_COMPONENT_SWIZZLE_B,
			VK_COMPONENT_SWIZZLE_A
		};
		colorAttachmentView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		colorAttachmentView.subresourceRange.baseMipLevel = 0;
		colorAttachmentView.subresourceRange.levelCount = 1;
		colorAttachmentView.subresourceRange.baseArrayLayer = 0;
		colorAttachmentView.subresourceRange.layerCount = 1;
		colorAttachmentView.viewType = VK_IMAGE_VIEW_TYPE_2D;
		colorAttachmentView.flags = 0;

		pSwapChain->buffers[i].image = pSwapChain->images[i];
		colorAttachmentView.image = pSwapChain->buffers[i].image;

		vkCreateImageView(pRenderer->device, &colorAttachmentView, nullptr, &pSwapChain->buffers[i].view);

		renderTarget->texture[i] = new nxt::Texture();
		renderTarget->texture[i]->image = pSwapChain->buffers[i].image;
		renderTarget->texture[i]->view = pSwapChain->buffers[i].view;
	}

	iRenderTargetManager = new VulkanRenderTargetManager();
	resourceManager = new ResourceManager();

	pRenderer->pSwapChain = pSwapChain;

	pRenderer->renderPass = new RenderPass();
	pRenderer->renderPass->pRenderTargets[0] = renderTarget;
	pRenderer->renderPass->renderTargetCount = 1;
	pRenderer->renderPass->clearColor[0] = 0.2f;
	pRenderer->renderPass->clearColor[1] = 0.4f;
	pRenderer->renderPass->clearColor[2] = 0.6f;
	pRenderer->renderPass->clearColor[3] = 1.0f;
	pRenderer->renderPass->loadOperations[0] = LoadOperations::CLEAR;
	// Create a frame buffer for every image in the swapchain

	VkCommandPoolCreateInfo cmdPoolInfo = {};
	cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolInfo.queueFamilyIndex = pRenderer->pSwapChain->queueNodeIndex;
	cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	vkCreateCommandPool(pRenderer->device, &cmdPoolInfo, nullptr, &pRenderer->cmdPool);

	return NxtResult::NXT_OK;
}

RenderPass* nxt::GetPrimaryRenderPass() {
	return pRenderer->renderPass;
}

NxtResult nxt::CreateRenderPass(RenderPassDesc renderPassDesc, RenderPass** renderPass) {

	RenderTarget* rtv[8];
	RenderTarget* depth = nullptr;

	LoadOperations loadOperations[8];
	ResourceState srcStates[8];
	ResourceState dstStates[8];

	VkAttachmentDescription attachments[8];
	VkAttachmentReference attachRef[8];

	VkSubpassDependency dependencies[8];
	VkSubpassDescription vkSubpassDesc[8];
	uint32_t numDependencies = 0;
	uint32_t numSubpasses = 0;

	for (int i = 0; i < renderPassDesc.attachmentCount; i++) {

		std::string resourceName = renderPassDesc.rtDesc[i].name;
		IResource* res = resourceManager->addResource(resourceName, iRenderTargetManager, renderPassDesc.rtDesc[i]);
		RenderTarget* renderTarget = static_cast<RenderTarget*>(res);

		if (renderTarget)
			depth = renderTarget;
		else
			rtv[i] = renderTarget;
	}

	*renderPass = new RenderPass();
	(*renderPass)->renderTargetCount = renderPassDesc.attachmentCount;
	(*renderPass)->depthTarget = depth;
	memcpy((*renderPass)->loadOperations, loadOperations, sizeof(LoadOperations) * renderPassDesc.attachmentCount);
	memcpy((*renderPass)->pRenderTargets, rtv, sizeof(RenderTarget) * renderPassDesc.attachmentCount);
	memcpy((*renderPass)->clearColor, renderPassDesc.clearColor, sizeof(float) * 4);

	return NxtResult::NXT_OK;
}

NxtResult nxt::CreateCommandBuffer(CommandBuffer** cmd) {

	*cmd = new CommandBuffer();
	(*cmd)->drawCmdBuffers.resize(pRenderer->bufferCount);

	VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.commandPool = pRenderer->cmdPool;
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocateInfo.commandBufferCount = pRenderer->bufferCount;

	vkAllocateCommandBuffers(pRenderer->device, &commandBufferAllocateInfo, (*cmd)->drawCmdBuffers.data());

	//if (begin)
	//{
	//	VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();
	 //   vkBeginCommandBuffer((*cmd)->drawCmdBuffers[0], &cmdBufInfo);
	//}

	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	// Create in signaled state so we don't wait on first render of each command buffer
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	(*cmd)->waitFences.resize((*cmd)->drawCmdBuffers.size());
	for (auto& fence : (*cmd)->waitFences)
	{
		vkCreateFence(pRenderer->device, &fenceCreateInfo, nullptr, &fence);
	}

	return NxtResult::NXT_OK;
}

NxtResult nxt::BeginRecording(CommandBuffer* cmdBuffer) {

	vkAcquireNextImageKHR(pRenderer->device, pRenderer->pSwapChain->swapChain, UINT64_MAX, pRenderer->presentComplete[pRenderer->currentBuffer], (VkFence)nullptr, &pRenderer->currentBuffer);
	int i = pRenderer->currentBuffer;

	// Wait for the fence to signal that command buffer has finished executing
	vkWaitForFences(pRenderer->device, 1, &cmdBuffer->waitFences[i], VK_TRUE, DEFAULT_FENCE_TIMEOUT);
	vkResetFences(pRenderer->device, 1, &cmdBuffer->waitFences[i]);

	vkResetCommandBuffer(cmdBuffer->drawCmdBuffers[i], 0);

	VkCommandBufferBeginInfo cmdBufInfo = {};
	cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBufInfo.pNext = nullptr;

	vkBeginCommandBuffer(cmdBuffer->drawCmdBuffers[i], &cmdBufInfo);

	// Ending the render pass will add an implicit barrier transitioning the frame buffer color attachment to
	// VK_IMAGE_LAYOUT_PRESENT_SRC_KHR for presenting it to the windowing system

	return NxtResult::NXT_OK;
}

NxtResult nxt::CmdBeginRenderPass(CommandBuffer* cmdBuffer, RenderPass* renderPass) {

	int curr = pRenderer->currentBuffer;

	VkClearValue clearValues[1];
	clearValues[0].color = { { 0.2f, 0.4f, 0.6f, 1.0f } };

	VkRenderingAttachmentInfoKHR attachments[8];
	VkRenderingAttachmentInfoKHR depthAttachment;

	if (renderPass->depthTarget) {
		depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
		depthAttachment.imageView = renderPass->depthTarget->texture[curr]->view;
		depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL_KHR;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		depthAttachment.clearValue.depthStencil = { 1.0f, 0 };
	}

	uint32_t colorAttachmentCount = 0;
	for (int i = 0; i < renderPass->renderTargetCount; i++) {
		attachments[i] = {};
		attachments[i].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
		attachments[i].imageView = renderPass->pRenderTargets[i]->texture[curr]->view;
		attachments[i].imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR;
		attachments[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[i].clearValue = clearValues[0];
		colorAttachmentCount++;
	}

	VkRenderingInfoKHR info{};
	info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
	info.renderArea.offset.x = 0;
	info.renderArea.offset.y = 0;
	info.renderArea.extent.width = pRenderer->width;
	info.renderArea.extent.height = pRenderer->height;
	info.layerCount = 1;
	info.colorAttachmentCount = colorAttachmentCount;
	info.pColorAttachments = attachments;
	info.pNext = nullptr;
	info.pDepthAttachment = (renderPass->depthTarget) ? &depthAttachment : nullptr;

	VkImageMemoryBarrier image_memory_barrier{};
	image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	image_memory_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	image_memory_barrier.image = renderPass->pRenderTargets[0]->texture[curr]->image;
	image_memory_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	image_memory_barrier.subresourceRange.baseMipLevel = 0;
	image_memory_barrier.subresourceRange.levelCount = 1;
	image_memory_barrier.subresourceRange.baseArrayLayer = 0;
	image_memory_barrier.subresourceRange.layerCount = 1;

	vkCmdPipelineBarrier(cmdBuffer->drawCmdBuffers[curr], VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,  // srcStageMask
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // dstStageMask
		0,
		0,
		nullptr,
		0,
		nullptr,
		1, // imageMemoryBarrierCount
		&image_memory_barrier // pImageMemoryBarriers
	);

	// Set target frame buffer
	vkCmdBeginRendering(cmdBuffer->drawCmdBuffers[curr], &info);

	VkViewport viewport{};
	viewport.width = pRenderer->width;
	viewport.height = pRenderer->height;
	viewport.minDepth = 0.1f;
	viewport.maxDepth = 1.0f;
	viewport.x = 0.0f;
	viewport.y = 0.0f;

	vkCmdSetViewport(cmdBuffer->drawCmdBuffers[curr], 0, 1, &viewport);

	VkRect2D scissor;
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	scissor.extent.width = pRenderer->width;
	scissor.extent.height = pRenderer->height;

	vkCmdSetScissor(cmdBuffer->drawCmdBuffers[curr], 0, 1, &scissor);

	return NxtResult::NXT_OK;
}

NxtResult nxt::CmdEndRenderPass(CommandBuffer* cmdBuffer, RenderPass* renderPass) {

	int i = pRenderer->currentBuffer;
	vkCmdEndRendering(cmdBuffer->drawCmdBuffers[i]);

	VkImageMemoryBarrier image_memory_barrier{};
	image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	image_memory_barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	image_memory_barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	image_memory_barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	image_memory_barrier.image = renderPass->pRenderTargets[0]->texture[i]->image;
	image_memory_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	image_memory_barrier.subresourceRange.baseMipLevel = 0;
	image_memory_barrier.subresourceRange.levelCount = 1;
	image_memory_barrier.subresourceRange.baseArrayLayer = 0;
	image_memory_barrier.subresourceRange.layerCount = 1;

	vkCmdPipelineBarrier(cmdBuffer->drawCmdBuffers[i],
		VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,  // srcStageMask
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, // dstStageMask
		0,
		0,
		nullptr,
		0,
		nullptr,
		1, // imageMemoryBarrierCount
		&image_memory_barrier // pImageMemoryBarriers
	);


	return NxtResult::NXT_OK;
}

NxtResult nxt::EndRecording(CommandBuffer* cmdBuffer) {
	//assert(commandBuffer != VK_NULL_HANDLE);

	int i = pRenderer->currentBuffer;
	vkEndCommandBuffer(cmdBuffer->drawCmdBuffers[i]);

	return NxtResult::NXT_OK;
}

NxtResult nxt::SubmitCommandBuffers(uint8_t numCommandBuffers, CommandBuffer** cmdBuffers) {

	uint32_t idx = pRenderer->currentBuffer;
	VkCommandBuffer vkCmdBuffers[128];
	for (int i = 0; i < numCommandBuffers; i++) {
		vkCmdBuffers[i] = cmdBuffers[i]->drawCmdBuffers[idx];
	}

	// Pipeline stage at which the queue submission will wait (via pWaitSemaphores)
	VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	// The submit info structure specifies a command buffer queue submission batch
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pWaitDstStageMask = &waitStageMask;               // Pointer to the list of pipeline stages that the semaphore waits will occur at
	submitInfo.pWaitSemaphores = &pRenderer->presentComplete[idx];      // Semaphore(s) to wait upon before the submitted command buffer starts executing
	submitInfo.waitSemaphoreCount = 1;                           // One wait semaphore
	submitInfo.pSignalSemaphores = &pRenderer->renderComplete[idx];     // Semaphore(s) to be signaled when command buffers have completed
	submitInfo.signalSemaphoreCount = 1;                         // One signal semaphore
	submitInfo.pCommandBuffers = vkCmdBuffers; // Command buffers(s) to execute in this batch (submission)
	submitInfo.commandBufferCount = numCommandBuffers;
	// One command buffer

	// Create fence to ensure that the command buffer has finished executing
	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = 0;
	VkFence fence;
	//  vkCreateFence(pRenderer->device, &fenceCreateInfo, nullptr, &cmdB);

	  // Submit to the queue
	vkQueueSubmit(pRenderer->queue, 1, &submitInfo, cmdBuffers[0]->waitFences[idx]);

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pNext = NULL;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &pRenderer->pSwapChain->swapChain;
	presentInfo.pImageIndices = &idx;
	// Check if a wait semaphore has been specified to wait for before presenting the image
	if (pRenderer->renderComplete[idx] != VK_NULL_HANDLE)
	{
		presentInfo.pWaitSemaphores = &pRenderer->renderComplete[idx];
		presentInfo.waitSemaphoreCount = 1;
	}
	vkQueuePresentKHR(pRenderer->queue, &presentInfo);

	return NxtResult::NXT_OK;
}
#endif
