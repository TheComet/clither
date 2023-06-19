
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
#define MAX_SNAKES 512

struct VulkanTexture {
	VkImage image;
	VkImageLayout imageLayout;
	VkDeviceMemory deviceMemory;
	VkImageView view;
	VkDescriptorImageInfo descriptor;
};

struct VulkanBuffer {
	VkBuffer buffer;
	VkDeviceMemory memory;
};
/*
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
	//VkBufferUsageFlags usageFlags;
	/** @brief Memory property flags to be filled by external source at buffer creation (to query at some later point) */
	//VkMemoryPropertyFlags memoryPropertyFlags;
	//VkResult map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
	//void unmap();
	//VkResult bind(VkDeviceSize offset = 0);
	//void setupDescriptor(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
	//void copyTo(void* data, VkDeviceSize size);
	//VkResult flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
	//VkResult invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
	//void destroy();
//};

struct BufferDesc {
	size_t size;
	void* pData;
};

struct sprite_mesh
{
	struct VulkanBuffer vbo;
	struct VulkanBuffer ibo;
};

struct UniformBufferObject {
	struct VulkanBuffer buffer;
	VkDescriptorBufferInfo bufferDescrptor;
	void* pData;
};

struct bgUbo {
	float uAspectRatio[4];
	float uCamera[3];
	float pad0;
	float uShadowInvRes[2];
	float pad1[2];
};

/*


	// Contains the shadow mask (white = shadow, black = no shadow)
	// It spans the entire screen and aspect ratio adjustments have
	// already been made
	uniform sampler2D sShadow;

	// Color and normal map
	uniform sampler2D sCol;
	uniform sampler2D sNM;


*/

struct bg
{
	VkPipeline bgPipeline;
	struct VulkanBuffer vbo;
	struct VulkanBuffer ibo;
	//GLuint fbo;
	struct VulkanTexture texShadow;
	struct VulkanTexture texCol;
	struct VulkanTexture texNor;

	VkDescriptorSetLayout layout;
	VkDescriptorSet bgDescriptorSet;
	VkDescriptorPool bgDescriptorPool;
	VkPipelineLayout pipelineLayout;

	struct UniformBufferObject ubo[BUFFER_COUNT];
	struct bgUbo bgubo;

	/*
	GLuint uAspectRatio;
	GLuint uCamera;
	GLuint uShadowInvRes;
	GLuint sShadow;
	GLuint sCol;
	GLuint sNM;*/
};

struct sprite_shadow
{
	VkPipeline spriteShadowPipeline;
	/*
	GLuint uAspectRatio;
	GLuint uPosCameraSpace;
	GLuint uDir;
	GLuint uSize;
	GLuint uAnim;
	GLuint sNM;*/
};

struct sprite_mat
{
	VkPipeline spriteMaterialPipeline;
	/*
	GLuint uAspectRatio;
	GLuint uPosCameraSpace;
	GLuint uDir;
	GLuint uSize;
	GLuint uAnim;
	GLuint sCol;
	GLuint sNM;*/
};

struct sprite_tex
{
	struct VulkanTexture texDiffuse;
	struct VulkanTexture texNM;
	int8_t tile_x, tile_y, tile_count, frame;
};

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

	VkDescriptorSet uboDynamicDescriptorSet;
	VkDescriptorPool pool;

	VkSampler linearSampler;
	struct CommandBuffer cmdBuffer;

	struct
	{
		uint32_t graphics;
		uint32_t compute;
		uint32_t transfer;
	} queueFamilyIndices;

	struct input input_buffer;

	struct sprite_mesh sprite_mesh;
	
	struct bg bg;
	struct sprite_mat sprite_mat;
	struct sprite_shadow sprite_shadow;
	struct sprite_tex head0_base;
	struct sprite_tex head0_gather;
	struct sprite_tex body0_base;
	struct sprite_tex tail0_base;
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


static
void CreateDescriptorSets(struct gfx* gfx) {
	VkDescriptorSetLayoutBinding binding[3];
	memset(&binding, 0, sizeof(VkDescriptorSetLayoutBinding) * 3);
	binding[0].binding = 0;
	binding[0].descriptorCount = 1;
	binding[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	binding[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	binding[1].binding = 1;
	binding[1].descriptorCount = 1;
	binding[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binding[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	binding[2].binding = 2;
	binding[2].descriptorCount = 1;
	binding[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	binding[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutCreateInfo layoutInfo;
	memset(&layoutInfo, 0, sizeof(VkDescriptorSetLayoutCreateInfo));
	layoutInfo.bindingCount = 3;
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.pBindings = &binding;

	vkCreateDescriptorSetLayout(gfx->device, &layoutInfo, 0, &gfx->bg.layout);

	VkDescriptorPoolSize pool[2];
	memset(&pool, 0, sizeof(VkDescriptorPoolSize) * 2);
	pool[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
	pool[0].descriptorCount = BUFFER_COUNT;

	pool[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	pool[1].descriptorCount = BUFFER_COUNT * BUFFER_COUNT;

	VkDescriptorPoolCreateInfo info;
	memset(&info, 0, sizeof(VkDescriptorPoolCreateInfo));
	info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	info.poolSizeCount = 2;
	info.pPoolSizes = &pool;
	info.maxSets = 1;
	vkCreateDescriptorPool(gfx->device, &info, 0, &gfx->bg.bgDescriptorPool);

	VkDescriptorSetAllocateInfo allocateInfo;
	memset(&allocateInfo, 0, sizeof(VkDescriptorSetAllocateInfo));
	allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocateInfo.descriptorPool = gfx->bg.bgDescriptorPool;
	allocateInfo.descriptorSetCount = 1;
	allocateInfo.pSetLayouts = &gfx->bg.layout;
	VK_CHECK_RESULT(vkAllocateDescriptorSets(gfx->device, &allocateInfo, &gfx->bg.bgDescriptorSet));
}

static int
bg_init(struct gfx* gfx, struct bg* bg, int fbwidth, int fbheight)
{
	memset(bg, 0, sizeof * bg);

	/* Set up shadow framebuffer */

	/*
	glGenTextures(1, &bg->texShadow);
	glBindTexture(GL_TEXTURE_2D, bg->texShadow);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, fbwidth / SHADOW_MAP_SIZE_FACTOR, fbheight / SHADOW_MAP_SIZE_FACTOR, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);

	glGenFramebuffers(1, &bg->fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, bg->fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, bg->texShadow, 0);
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		log_err("Incomplete framebuffer!\n");
		goto incomplete_shadow_framebuffer;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	/* Set up quad mesh */
	/*
	glGenBuffers(1, &bg->vbo);
	glBindBuffer(GL_ARRAY_BUFFER, bg->vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(bg_vertices), bg_vertices, GL_STATIC_DRAW);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	glGenBuffers(1, &bg->ibo);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, bg->ibo);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(bg_indices), bg_indices, GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);*/

	struct BufferDesc vbDesc;
	vbDesc.size = sizeof(bg_vertices);
	vbDesc.pData = bg_vertices;

	struct BufferDesc ibDesc;
	ibDesc.size = sizeof(bg_indices);
	ibDesc.pData = bg_indices;

	create_geometry_buffers(gfx, &bg->vbo, &bg->ibo, &vbDesc, &ibDesc);

	size_t dynamicAlignment;
	size_t minUboAlignment = gfx->deviceProperties.limits.minUniformBufferOffsetAlignment;
	dynamicAlignment = sizeof(struct bgUbo);
	if (minUboAlignment > 0) {
		dynamicAlignment = (dynamicAlignment + minUboAlignment - 1) & ~(minUboAlignment - 1);
	}

	CreateDescriptorSets(gfx);


	
	/* Prepare background textures */
	/*
	glGenTextures(1, &bg->texCol);
	glBindTexture(GL_TEXTURE_2D, bg->texCol);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);

	glGenTextures(1, &bg->texNor);
	glBindTexture(GL_TEXTURE_2D, bg->texNor);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_2D, 0);*/

	return 0;

incomplete_shadow_framebuffer:
	//glBindFramebuffer(GL_FRAMEBUFFER, 0);
	//glDeleteFramebuffers(1, &bg->fbo);
	//glDeleteTextures(1, &bg->texShadow);

	return -1;
}


VkShaderModule loadSPIRVShader(struct gfx* gfx, VkShaderStageFlagBits* flag, const char* filename)
{
	size_t shaderSize;
	char* shaderCode = NULL;

#if defined(__ANDROID__)
	// Load shader from compressed asset
	AAsset* asset = AAssetManager_open(androidApp->activity->assetManager, filename.c_str(), AASSET_MODE_STREAMING);
	assert(asset);
	shaderSize = AAsset_getLength(asset);
	assert(shaderSize > 0);

	shaderCode = new char[shaderSize];
	AAsset_read(asset, shaderCode, shaderSize);
	AAsset_close(asset);
#else

	int length;
	GLuint shader;
	void* source = fs_map_file(filename, &length);
	//if (source == NULL)
	//	goto load_shaders_failed;
	//

#endif
	if (source)
	{
		*flag = string_ends_with(filename, ".vsh") ? VK_SHADER_STAGE_VERTEX_BIT : VK_SHADER_STAGE_FRAGMENT_BIT;

		// Create a new shader module that will be used for pipeline creation
		VkShaderModuleCreateInfo moduleCreateInfo;
		//memset(&moduleCreateInfo, 0, sizeof(VkShaderModuleCreateInfo));
		moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		moduleCreateInfo.codeSize = length;
		moduleCreateInfo.pCode = (uint32_t*)source;
		moduleCreateInfo.flags = 0;
		moduleCreateInfo.pNext = 0;

		VkShaderModule shaderModule;
		VK_CHECK_RESULT(vkCreateShaderModule(gfx->device, &moduleCreateInfo, NULL, &shaderModule));

		return shaderModule;
	}
	fs_unmap_file(source, length);
}


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

static int VK_CHECK_RESULT(VkResult vkResult) {
	if (vkResult != VK_SUCCESS)
		return 0;
}

static int
load_sprite_tex(struct gfx* gfx,
	struct sprite_tex* mat,
	const char* col,
	const char* nm,
	int8_t tile_x, int8_t tile_y, int8_t tile_count) {

	int cc = 4;
	int width, height;
	void* pData = stbi_loadf(col, &width, &height, &cc, 4);
	assert(cc == 4);

	load_texture(gfx, &mat->texDiffuse, pData, width, height, cc);

	pData = stbi_loadf(nm, &width, &height, &cc, 4);
	assert(cc == 4);

	load_texture(gfx, &mat->texNM, pData, width, height, cc);

	mat->tile_x = tile_x;
	mat->tile_y = tile_y;
	mat->tile_count = tile_count;
	mat->frame = 0;

	return 0;
}


uint32_t getMemoryType(struct gfx* gfx, uint32_t typeBits, VkMemoryPropertyFlags properties, VkBool32* memTypeFound)
{

	VkPhysicalDeviceMemoryProperties memProperties;
	vkGetPhysicalDeviceMemoryProperties(gfx->physicalDevice, &memProperties);
	for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
	{
		if ((typeBits & 1) == 1)
		{
			if ((memProperties.memoryTypes[i].propertyFlags & properties) == properties)
			{
				if (memTypeFound)
				{
					*memTypeFound = 1;
				}
				return i;
			}
		}
		typeBits >>= 1;
	}

	if (memTypeFound)
	{
		*memTypeFound = 0;
		return 0;
	}
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
VkPipeline CreatePipeline(struct gfx* gfx, const char** shaders) {

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState;
	memset(&inputAssemblyState, 0, sizeof(VkPipelineInputAssemblyStateCreateInfo));
	inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyState.primitiveRestartEnable = VK_FALSE;
	inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	VkPipelineRasterizationStateCreateInfo rasterizationState;
	memset(&rasterizationState, 0, sizeof(VkPipelineRasterizationStateCreateInfo));
	rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationState.cullMode = VK_CULL_MODE_NONE;
	rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizationState.lineWidth = 1.0f;
	rasterizationState.depthClampEnable = VK_FALSE;

	VkPipelineColorBlendAttachmentState blendAttachmentState;
	memset(&blendAttachmentState, 0, sizeof(VkPipelineColorBlendAttachmentState));
	blendAttachmentState.colorWriteMask = 0xf;
	blendAttachmentState.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo colorBlendState;
	memset(&colorBlendState, 0, sizeof(VkPipelineColorBlendStateCreateInfo));
	colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendState.attachmentCount = 1;
	colorBlendState.pAttachments = &blendAttachmentState;

	//VkPipelineDepthStencilStateCreateInfo depthStencilState =
	//	vks::initializers::pipelineDepthStencilStateCreateInfo(
	//		VK_TRUE,
	//		VK_TRUE,
	//		VK_COMPARE_OP_LESS_OR_EQUAL);

	VkPipelineViewportStateCreateInfo viewportState;
	memset(&viewportState, 0, sizeof(VkPipelineViewportStateCreateInfo));
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;
	viewportState.flags = 0;

	VkPipelineMultisampleStateCreateInfo multisampleState;
	memset(&multisampleState, 0, sizeof(VkPipelineMultisampleStateCreateInfo));
	multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampleState.flags = 0;

	VkDynamicState dynamicStateEnables[2] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};
	VkPipelineDynamicStateCreateInfo dynamicState;
	memset(&dynamicState, 0, sizeof(VkPipelineDynamicStateCreateInfo));
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.pDynamicStates = dynamicStateEnables;
	dynamicState.dynamicStateCount = 2;
	dynamicState.flags = 0;

	// Load shaders
	VkPipelineShaderStageCreateInfo shaderStages[2];
	uint32_t shaderStageCount = 0;
	for (int i = 0; shaders[i]; ++i)
	{
		memset(&shaderStages[i], 0, sizeof(VkPipelineShaderStageCreateInfo));
		shaderStages[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStages[i].module = loadSPIRVShader(gfx, &shaderStages[i].stage, shaders[i]);
		shaderStages[i].pName = "main";

		shaderStageCount++;
	}

	VkGraphicsPipelineCreateInfo pipelineCreateInfo;
	memset(&pipelineCreateInfo, 0, sizeof(VkGraphicsPipelineCreateInfo));
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.basePipelineIndex = -1;
	pipelineCreateInfo.flags = 0;
	pipelineCreateInfo.renderPass = gfx->renderPass.renderPass;
	pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;

	VkVertexInputBindingDescription vertexInputBindingDescription;
	vertexInputBindingDescription.binding = 0;
	vertexInputBindingDescription.stride = sizeof(struct vertex);
	vertexInputBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription vInputAttribDescription[2];
	vInputAttribDescription[0].location = 0;
	vInputAttribDescription[0].binding = 0;
	vInputAttribDescription[0].format = VK_FORMAT_R32G32_SFLOAT;
	vInputAttribDescription[0].offset = 0;

	vInputAttribDescription[1].location = 1;
	vInputAttribDescription[1].binding = 0;
	vInputAttribDescription[1].format = VK_FORMAT_R32G32_SFLOAT;
	vInputAttribDescription[1].offset = 8;

	VkPipelineVertexInputStateCreateInfo ia;
	memset(&ia, 0, sizeof(VkPipelineVertexInputStateCreateInfo));
	ia.vertexBindingDescriptionCount = 1;
	ia.pVertexBindingDescriptions = &vertexInputBindingDescription;
	ia.vertexAttributeDescriptionCount = 2;
	ia.pVertexAttributeDescriptions = vInputAttribDescription;
	ia.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	VkPipelineLayout layout;
	VkPipelineLayoutCreateInfo pipelineLayoutInfo;
	memset(&pipelineLayoutInfo, 0, sizeof(VkPipelineLayoutCreateInfo));
	pipelineLayoutInfo.setLayoutCount = 1;
	pipelineLayoutInfo.pSetLayouts = &gfx->bg.layout;
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

	vkCreatePipelineLayout(gfx->device, &pipelineLayoutInfo, 0, &layout);
	gfx->bg.pipelineLayout = layout;

	pipelineCreateInfo.pVertexInputState = &ia;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
	pipelineCreateInfo.pRasterizationState = &rasterizationState;
	pipelineCreateInfo.pColorBlendState = &colorBlendState;
	pipelineCreateInfo.pMultisampleState = &multisampleState;
	pipelineCreateInfo.pViewportState = &viewportState;
	pipelineCreateInfo.pDepthStencilState = 0;
	pipelineCreateInfo.pDynamicState = &dynamicState;
	pipelineCreateInfo.stageCount = shaderStageCount;
	pipelineCreateInfo.pStages = shaderStages;
	pipelineCreateInfo.layout = layout;

	VkPipeline pipeline;
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(gfx->device, 0, 1, &pipelineCreateInfo, 0, &pipeline));

	return pipeline;
}

static
VkResult createLogicalDevice(struct gfx* gfx, int useSwapChain, VkQueueFlags requestedQueueTypes)
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
	* 
	* 
	*  As we are only supporting graphics queues we will not fill out creation structures for other queue types
	* 
	* 
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

	deviceExtensions[0] = (VK_KHR_SWAPCHAIN_EXTENSION_NAME);
	
	deviceCreateInfo.enabledExtensionCount = 1;
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions;
	
	VkResult result = vkCreateDevice(gfx->physicalDevice, &deviceCreateInfo, 0, &gfx->device);
	if (result != VK_SUCCESS)
	{
		return result;
	}

	return result;
}

struct gfx*
	gfx_create(int initial_width, int initial_height) {

	//allocate and initialize gfx
	struct gfx* gfx = MALLOC(sizeof * gfx);
	gfx->bufferCount = BUFFER_COUNT;
	gfx->currentBuffer = 0;
	gfx->device = VK_NULL_HANDLE;
	gfx->instance = VK_NULL_HANDLE;
	gfx->swapChain.surface = VK_NULL_HANDLE;
	gfx->swapChain.swapChain = VK_NULL_HANDLE;
	gfx->renderPass.renderPass = VK_NULL_HANDLE;
	gfx->cmdPool = VK_NULL_HANDLE;
	for (int i = 0; i < gfx->bufferCount; i++) {
		gfx->frameBuffers[i] = VK_NULL_HANDLE;
		gfx->cmdBuffer.drawCmdBuffers[i] = VK_NULL_HANDLE;
		gfx->cmdBuffer.waitFences[i] = VK_NULL_HANDLE;
		gfx->renderComplete[i] = VK_NULL_HANDLE;
		gfx->presentComplete[i] = VK_NULL_HANDLE;
		gfx->swapChain.buffers[i].view = VK_NULL_HANDLE;
		gfx->swapChain.buffers[i].image = VK_NULL_HANDLE;
	}

	const char* instance_layers = "VK_LAYER_KHRONOS_validation";
	PFN_vkCreateInstance pfnCreateInstance = (PFN_vkCreateInstance)
		glfwGetInstanceProcAddress(NULL, "vkCreateInstance");

	uint32_t count;
	const char** extensions = glfwGetRequiredInstanceExtensions(&count);
	if (extensions == NULL) {
		log_err("No supported vulkan extensions found\n");
		goto vk_extension_load_failure;
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

	gfx->instance = VK_NULL_HANDLE;
	VkResult vkResult = vkCreateInstance(&ici, 0, &gfx->instance);
	if (vkResult != VK_SUCCESS) {
		log_err("Unable to create vulkan instance\n");
		goto vk_instance_creation_failure;
	}

	uint32_t gpuCount = 0;
	vkEnumeratePhysicalDevices(gfx->instance, &gpuCount, NULL);
	if (gpuCount == 0) {
		log_err("No device with vulkan support found\n");
		goto vk_instance_creation_failure;
	}

	// Enumerate devices
	VkPhysicalDevice physicalDevices[MAX_GPU_COUNT];
	vkEnumeratePhysicalDevices(gfx->instance, &gpuCount, physicalDevices);
	assert(gpuCount > 0 && gpuCount <= MAX_GPU_COUNT);
	
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
	assert(queueFamilyCount > 0 && queueFamilyCount <= MAX_QUEUE_FAMILIES);

	vkGetPhysicalDeviceQueueFamilyProperties(gfx->physicalDevice, &queueFamilyCount, gfx->queueFamilyProperties);

	// Get list of supported extensions
	uint32_t extCount = 0;
	vkEnumerateDeviceExtensionProperties(gfx->physicalDevice, NULL, &extCount, NULL);
	assert(extCount > 0 && extCount <= MAX_EXT_COUNT);

	VkExtensionProperties extensionProps[MAX_EXT_COUNT];
	if (vkEnumerateDeviceExtensionProperties(gfx->physicalDevice, NULL, &extCount, &extensionProps) == VK_SUCCESS)
	{
		for (int i = 0; i < extCount; i++)
		{
			gfx->supportedExtensions[i] = extensionProps[i].extensionName;
		}
	}
	
	vkResult = createLogicalDevice(gfx, 1, VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT);
	if (vkResult != VK_SUCCESS) {
		log_err("Failed to create logical vulkan device.\n");
		goto vk_logical_device_failure;
	}

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
		VkResult res = vkCreateSemaphore(gfx->device, &semaphoreCreateInfo, NULL, &gfx->presentComplete[i]);
		if (res != VK_SUCCESS) {
			log_err("Failed to create vulkan present complete semaphore.\n");
			goto vk_present_complete_semaphore_failure;
		}

		// Create a semaphore used to synchronize command submission
		// Ensures that the image is not presented until all commands have been submitted and executed
		res = vkCreateSemaphore(gfx->device, &semaphoreCreateInfo, NULL, &gfx->renderComplete[i]);
		if (res != VK_SUCCESS) {
			log_err("Failed to create vulkan render complete semaphore.\n");
			goto vk_render_complete_semaphore_failure;
		}
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

	GLFWwindow* window = NULL;
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	window = glfwCreateWindow(initial_width, initial_height, appInfo.pApplicationName, 0, 0);
	if (window == NULL) {
		log_err("Failed to create glfw window for vulkan.\n");
		goto vk_render_complete_semaphore_failure;
	}

	// make sure we indeed get the surface size we want.
	glfwGetFramebufferSize(window, &initial_width, &initial_height);
	vkResult = glfwCreateWindowSurface(gfx->instance, window, 0, &gfx->swapChain.surface);
	if (VK_SUCCESS != vkResult) {
		log_err("Failed to create glfw window for vulkan.\n");
		goto vk_glfw_create_surface_failure;
	}

	// Get available queue family properties
	uint32_t queueCount;
	vkGetPhysicalDeviceQueueFamilyProperties(gfx->physicalDevice, &queueCount, NULL);
	assert(queueCount > 0 && queueCount <= MAX_QUEUE_COUNT);

	VkQueueFamilyProperties queueProps[MAX_QUEUE_COUNT];
	vkGetPhysicalDeviceQueueFamilyProperties(gfx->physicalDevice, &queueCount, queueProps);

	// Iterate over each queue to learn whether it supports presenting:
	// Find a queue with present support
	// Will be used to present the swap chain images to the windowing system
	struct SwapChain* pSwapChain = &gfx->swapChain;
	VkBool32 supportsPresent[MAX_QUEUE_COUNT];
	for (uint32_t i = 0; i < queueCount; i++)
	{
		VkResult res = fpGetPhysicalDeviceSurfaceSupportKHR(gfx->physicalDevice, i, pSwapChain->surface, &supportsPresent[i]);
		if (res != VK_SUCCESS) {
			log_err("Failed to query swapchain present support for queue.\n");
			goto vk_glfw_create_surface_failure;
		}
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
		log_err("Failed to find vulkan queue that supports graphics and presenting.\n");
		goto vk_glfw_create_surface_failure;
	}

	// todo : Add support for separate graphics and presenting queue
	if (graphicsQueueNodeIndex != presentQueueNodeIndex)
	{
		log_err("Selected vulkan graphics queue does not support presenting\n");
		goto vk_glfw_create_surface_failure;
	}

	pSwapChain->queueNodeIndex = graphicsQueueNodeIndex;

	// Get list of supported surface formats
	uint32_t formatCount;
	vkResult = fpGetPhysicalDeviceSurfaceFormatsKHR(gfx->physicalDevice, pSwapChain->surface, &formatCount, NULL);
	if (vkResult != VK_SUCCESS) {
		log_err("Unable to query supported surface formats for physical vulkan device.\n");
		goto vk_glfw_create_surface_failure;
	}
	assert(formatCount > 0 && formatCount <= MAX_SURFACE_FORMATS);

	VkSurfaceFormatKHR surfaceFormats[MAX_SURFACE_FORMATS];
	vkResult = fpGetPhysicalDeviceSurfaceFormatsKHR(gfx->physicalDevice, pSwapChain->surface, &formatCount, surfaceFormats);
	if (vkResult != VK_SUCCESS) {
		log_err("Unable to query supported surface formats for physical vulkan device.\n");
		goto vk_glfw_create_surface_failure;
	}

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
	vkResult = fpGetPhysicalDeviceSurfaceCapabilitiesKHR(gfx->physicalDevice, pSwapChain->surface, &surfCaps);
	if (vkResult != VK_SUCCESS) {
		log_err("Unable to retrieve vulkan surface capabilities.\n");
		goto vk_glfw_create_surface_failure;
	}
	assert(surfCaps.maxImageCount >= BUFFER_COUNT);


	// Get available present modes
	uint32_t presentModeCount;
	vkResult = fpGetPhysicalDeviceSurfacePresentModesKHR(gfx->physicalDevice, pSwapChain->surface, &presentModeCount, NULL);
	if (vkResult != VK_SUCCESS) {
		log_err("Failed to get vulkan surface present modes.\n");
		goto vk_glfw_create_surface_failure;
	}
	assert(presentModeCount > 0 && presentModeCount <= MAX_SWAP_CHAIN_PRESENT_MODES);

	VkPresentModeKHR presentModes[MAX_SWAP_CHAIN_PRESENT_MODES];
	vkResult = fpGetPhysicalDeviceSurfacePresentModesKHR(gfx->physicalDevice, pSwapChain->surface, &presentModeCount, presentModes);
	if (vkResult != VK_SUCCESS) {
		log_err("Failed to get vulkan surface present modes.\n");
		goto vk_glfw_create_surface_failure;
	}

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
	swapchainCI.oldSwapchain = oldSwapchain;
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

	vkResult = fpCreateSwapchainKHR(gfx->device, &swapchainCI, 0, &pSwapChain->swapChain);
	if (vkResult != VK_SUCCESS) {
		log_err("Failed to create vulkan swapchain.\n");
		goto vk_swapchain_creation_failure;
	}
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
	vkResult = fpGetSwapchainImagesKHR(gfx->device, pSwapChain->swapChain, &bufferCount, NULL);
	if (vkResult != VK_SUCCESS) {
		log_err("Failed to get vulkan swapchain images.\n");
		goto vk_swapchain_creation_failure;
	}
	assert(bufferCount == 3);

	// Get the swap chain images
	vkResult = fpGetSwapchainImagesKHR(gfx->device, pSwapChain->swapChain, &bufferCount, pSwapChain->images);
	if (vkResult != VK_SUCCESS) {
		log_err("Failed to get vulkan swapchain images.\n");
		goto vk_swapchain_creation_failure;
	}

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

		VkResult vkResult = vkCreateImageView(gfx->device, &colorAttachmentView, 0, &pSwapChain->buffers[i].view);
		if (vkResult != VK_SUCCESS) {
			log_err("Failed to create image view for vulkan swapchain image.\n");
			goto vk_image_view_creation_failure;
		}

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

	vkResult = vkCreateRenderPass(gfx->device, &renderPassInfo, 0, &gfx->renderPass.renderPass);
	if (vkResult != VK_SUCCESS) {
		log_err("Failed to create vulkan renderpass.\n");
		goto vk_renderpass_creation_failure;
	}

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
		VkResult vkResult = vkCreateFramebuffer(gfx->device, &frameBufferCreateInfo, 0, &gfx->frameBuffers[i]);
		if (vkResult != VK_SUCCESS) {
			log_err("Failed to create vulkan framebuffer.\n");
			goto vk_framebuffer_creation_failure;
		}
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
	vkResult = vkCreateCommandPool(gfx->device, &cmdPoolInfo, 0, &gfx->cmdPool);
	if (vkResult != VK_SUCCESS) {
		log_err("Failed to create vulkan command pool.\n");
		goto vk_commandpool_creation_failure;
	}

	struct CommandBuffer* pCommandBuffer = &gfx->cmdBuffer;
	VkCommandBufferAllocateInfo commandBufferAllocateInfo;
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.commandPool = gfx->cmdPool;
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocateInfo.commandBufferCount = gfx->bufferCount;
	commandBufferAllocateInfo.pNext = 0;

	vkResult = vkAllocateCommandBuffers(gfx->device, &commandBufferAllocateInfo, pCommandBuffer->drawCmdBuffers);
	if (vkResult != VK_SUCCESS) {
		log_err("Failed to allocate vulkan command buffers.\n");
		goto vk_commandpool_creation_failure;
	}

	VkFenceCreateInfo fenceCreateInfo;
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	// Create in signaled state so we don't wait on first render of each command buffer
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	fenceCreateInfo.pNext = 0;
	//(*cmd)->waitFences.resize((*cmd)->drawCmdBuffers.size());
	for (int i = 0; i < 3; i++)
	{
		vkResult = vkCreateFence(gfx->device, &fenceCreateInfo, 0, &pCommandBuffer->waitFences[i]);
		if (vkResult != VK_SUCCESS) {
			log_err("Failed to create vulkan fence.\n");
			goto vk_fence_creation_failure;
		}
	}

	create_sprite_mesh_buffers(gfx);

	// Create a texture sampler
	// In Vulkan textures are accessed by samplers
	// This separates all the sampling information from the texture data. This means you could have multiple sampler objects for the same texture with different settings
	// Note: Similar to the samplers available with OpenGL 3.3
	VkSamplerCreateInfo sampler;
	memset(&sampler, 0, sizeof(VkSamplerCreateInfo));
	sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler.magFilter = VK_FILTER_LINEAR;
	sampler.minFilter = VK_FILTER_LINEAR;
	sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler.mipLodBias = 0.0f;
	sampler.compareOp = VK_COMPARE_OP_NEVER;
	sampler.minLod = 0.0f;
	// Set max level-of-detail to mip level count of the texture
	sampler.maxLod = 1;

	// The device does not support anisotropic filtering
	sampler.maxAnisotropy = 1.0;
	sampler.anisotropyEnable = VK_FALSE;

	sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	VK_CHECK_RESULT(vkCreateSampler(gfx->device, &sampler, 0, &gfx->linearSampler));

	bg_init(gfx, &gfx->bg, gfx->width, gfx->height);

	load_sprite_tex(gfx, &gfx->head0_base, "packs/horror/head0/base_col.png", "packs/horror/head0/base_nm.png", 4, 3, 12);
	load_sprite_tex(gfx, &gfx->head0_gather, "packs/horror/head0/gather_col.png", "packs/horror/head0/gather_nm.png", 4, 3, 12);
	load_sprite_tex(gfx, &gfx->body0_base, "packs/horror/body0/base_col.png", "packs/horror/body0/base_nm.png", 4, 3, 12);

	return gfx;

//We check against VK_NULL_HANDLES to account for the unlikely case that vulkan performs an allocation yet the call still fails
vk_fence_creation_failure:
	for (int i = 0; i < BUFFER_COUNT; i++) {
		if (pCommandBuffer->waitFences[i] != VK_NULL_HANDLE)
			vkDestroyFence(gfx->device, pCommandBuffer->waitFences[i], 0);
	}
vk_commandpool_creation_failure:
	if (gfx->cmdPool != VK_NULL_HANDLE) {
		vkDestroyCommandPool(gfx->device, gfx->cmdPool, 0);
	}
vk_framebuffer_creation_failure:
	for (int i = 0; i < BUFFER_COUNT; i++) {
		if (gfx->frameBuffers[i] != VK_NULL_HANDLE)
			vkDestroyFramebuffer(gfx->device, gfx->frameBuffers[i], 0);
	}
vk_renderpass_creation_failure:
	if (gfx->renderPass.renderPass != VK_NULL_HANDLE)
		vkDestroyRenderPass(gfx->device, gfx->renderPass.renderPass, 0);
vk_image_view_creation_failure:
	for (int i = 0; i < BUFFER_COUNT; i++) {
		if (pSwapChain->buffers[i].view != VK_NULL_HANDLE)
			vkDestroyImageView(gfx->device, pSwapChain->buffers[i].view, 0);
	}
vk_swapchain_creation_failure:
	if (gfx->swapChain.swapChain != VK_NULL_HANDLE)
		vkDestroySwapchainKHR(gfx->device, gfx->swapChain.swapChain, 0);
vk_glfw_create_surface_failure:
	glfwDestroyWindow(window); //presumably destroys gfx->swapchain.surface as well
vk_present_complete_semaphore_failure:
	for (int i = 0; i < BUFFER_COUNT; i++) {
		if (gfx->presentComplete[i] != VK_NULL_HANDLE)
			vkDestroySemaphore(gfx->device, gfx->presentComplete[i], 0);
	}
vk_render_complete_semaphore_failure:
	for (int i = 0; i < BUFFER_COUNT; i++) {
		if (gfx->renderComplete[i] != VK_NULL_HANDLE)
			vkDestroySemaphore(gfx->device, gfx->renderComplete[i], 0);
	}
vk_logical_device_failure:
	if (gfx->device != VK_NULL_HANDLE)
		vkDestroyDevice(gfx->device, 0);
vk_instance_creation_failure:
	if (gfx->instance != VK_NULL_HANDLE)
		vkDestroyInstance(gfx->instance, 0);
vk_extension_load_failure:
	FREE(gfx);
	return NULL;
}

/*

	We first create buffers in staging memory (basically a area in system memory that the cpu can write to, and the gpu can read from). 
	We map them, acquire our pointer, and memcpy our vertices to that staging memory.
	We then issue a command to vk's command buffer to then read from that cpu/gpu accessible staging memory.
	This is so vulkan can then copy that data to its device accessible only memory (i.e. vram). Since our vertices dont change,
	this is ideal for performance...though much more boilerplatey
	
*/

static int 
create_geometry_buffers(struct gfx* gfx, struct VulkanBuffer* vb, const struct VulkanBuffer* ib, const struct BufferDesc* vbDesc, const struct BufferDesc* ibDesc) {
	
	VkMemoryAllocateInfo memAlloc;
	memset(&memAlloc, 0, sizeof(VkMemoryAllocateInfo));
	memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	VkMemoryRequirements memReqs;

	VkBool32* memFound = 0;
	struct VulkanBuffer staging_vb;
	struct VulkanBuffer staging_ib;

	VkBufferCreateInfo vertexBufferInfo;
	memset(&vertexBufferInfo, 0, sizeof(VkBufferCreateInfo));
	vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	vertexBufferInfo.size = vbDesc->size;
	vertexBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

	vkCreateBuffer(gfx->device, &vertexBufferInfo, 0, &staging_vb.buffer);

	vkGetBufferMemoryRequirements(gfx->device, staging_vb.buffer, &memReqs);
	memAlloc.allocationSize = memReqs.size;
	memAlloc.memoryTypeIndex = getMemoryType(gfx, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &memFound);
	VK_CHECK_RESULT(vkAllocateMemory(gfx->device, &memAlloc, 0, &staging_vb.memory));

	//struct sprite_mesh* p_sprite_mesh = &gfx->sprite_mesh;
	// Map and copy
	void* pData;
	vkMapMemory(gfx->device, staging_vb.memory, 0, memAlloc.allocationSize, 0, &pData);
	memcpy(pData, vbDesc->pData, vbDesc->size);
	vkUnmapMemory(gfx->device, staging_vb.memory);
	VK_CHECK_RESULT(vkBindBufferMemory(gfx->device, staging_vb.buffer, staging_vb.memory, 0));

	vertexBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	VK_CHECK_RESULT(vkCreateBuffer(gfx->device, &vertexBufferInfo, 0, &vb->buffer));
	vkGetBufferMemoryRequirements(gfx->device, vb->buffer, &memReqs);
	memAlloc.allocationSize = memReqs.size;
	memAlloc.memoryTypeIndex = getMemoryType(gfx, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memFound);
	VK_CHECK_RESULT(vkAllocateMemory(gfx->device, &memAlloc, 0, &vb->memory));
	VK_CHECK_RESULT(vkBindBufferMemory(gfx->device, vb->buffer, vb->memory, 0));

	// Index buffer
	VkBufferCreateInfo indexbufferInfo;
	memset(&indexbufferInfo, 0, sizeof(VkBufferCreateInfo));
	indexbufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	indexbufferInfo.size = ibDesc->size;
	indexbufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	// Copy index data to a buffer visible to the host (staging buffer)

	VK_CHECK_RESULT(vkCreateBuffer(gfx->device, &indexbufferInfo, 0, &staging_ib.buffer));
	vkGetBufferMemoryRequirements(gfx->device, staging_ib.buffer, &memReqs);
	memAlloc.allocationSize = memReqs.size;
	memAlloc.memoryTypeIndex = getMemoryType(gfx, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &memFound);
	VK_CHECK_RESULT(vkAllocateMemory(gfx->device, &memAlloc, 0, &staging_ib.memory));
	VK_CHECK_RESULT(vkMapMemory(gfx->device, staging_ib.memory, 0, ibDesc->size, 0, &pData));
	memcpy(pData, ibDesc->pData, ibDesc->size);
	vkUnmapMemory(gfx->device, staging_ib.memory);
	VK_CHECK_RESULT(vkBindBufferMemory(gfx->device, staging_ib.buffer, staging_ib.memory, 0));

	// Create destination buffer with device only visibility
	indexbufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	VK_CHECK_RESULT(vkCreateBuffer(gfx->device, &indexbufferInfo, 0, &ib->buffer));
	vkGetBufferMemoryRequirements(gfx->device, ib->buffer, &memReqs);
	memAlloc.allocationSize = memReqs.size;
	memAlloc.memoryTypeIndex = getMemoryType(gfx, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memFound);
	VK_CHECK_RESULT(vkAllocateMemory(gfx->device, &memAlloc, 0, &ib->memory));
	VK_CHECK_RESULT(vkBindBufferMemory(gfx->device, ib->buffer, ib->memory, 0));

	VkCommandBuffer copyCmd = gfx->cmdBuffer.drawCmdBuffers[0];
	VkCommandBufferBeginInfo info;
	memset(&info, 0, sizeof(VkCommandBufferBeginInfo));
	info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

	vkBeginCommandBuffer(copyCmd, &info);

	VkBufferCopy copyRegion;
	memset(&copyRegion, 0, sizeof(VkBufferCopy));
	//We issue a command to the command buffer instructing vulkan to copy our data over to vram
	copyRegion.size = sizeof(struct vertex) * 4;
	vkCmdCopyBuffer(copyCmd, staging_vb.buffer, vb->buffer, 1, &copyRegion);

	copyRegion.size = sizeof(short) * 6;
	vkCmdCopyBuffer(copyCmd, staging_ib.buffer, ib->buffer, 1, &copyRegion);

	//We force a wait here. This could be an opportunity to do some async copy in the future, but for now we keep it dead simple.
	VK_CHECK_RESULT(vkEndCommandBuffer(copyCmd));

	VkSubmitInfo submitInfo;
	memset(&submitInfo, 0, sizeof(VkSubmitInfo));
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &copyCmd;

	// Submit to the queue
	vkResetFences(gfx->device, 1, &gfx->cmdBuffer.waitFences[0]);
	VK_CHECK_RESULT(vkQueueSubmit(gfx->queue, 1, &submitInfo, gfx->cmdBuffer.waitFences[0]));
	//VK_CHECK_RESULT(vkWaitForFences(gfx->device, 1, &gfx->cmdBuffer.waitFences[0], VK_TRUE, DEFAULT_FENCE_TIMEOUT));
	vkQueueWaitIdle(gfx->queue);
	//VkResult res = vkResetFences(gfx->device, 1, &gfx->cmdBuffer.waitFences[0]);
	int z = 1;

	// Destroy staging buffers
	// Note: Staging buffer must not be deleted before the copies have been submitted and executed
	//vkDestroyBuffer(device, stagingBuffers.vertices.buffer, nullptr);
	//vkFreeMemory(device, stagingBuffers.vertices.memory, nullptr);
	//vkDestroyBuffer(device, stagingBuffers.indices.buffer, nullptr);
	//vkFreeMemory(device, stagingBuffers.indices.memory, nullptr);
}

static int
create_sprite_mesh_buffers(struct gfx* gfx) {

	struct BufferDesc vbDesc;
	vbDesc.size = sizeof(struct vertex) * 4;
	vbDesc.pData = sprite_vertices;

	struct BufferDesc ibDesc;
	ibDesc.size = sizeof(short) * 6;
	ibDesc.pData = sprite_indices;

	create_geometry_buffers(gfx, &gfx->sprite_mesh.vbo, &gfx->sprite_mesh.ibo, &vbDesc, &ibDesc);
}


static int load_texture(struct gfx* gfx, struct VulkanTexture* vulkanTexture, const void* pData, uint32_t width, uint32_t height, uint32_t cc) {
	VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;
	VkBool32 memFound;

	// We prefer using staging to copy the texture data to a device local optimal image
	//VkBool32 useStaging = true;
	size_t textureSize = width * height * 4;
	VkMemoryAllocateInfo memAllocInfo;
	memset(&memAllocInfo, 0, sizeof(VkMemoryAllocateInfo));
	memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAllocInfo.allocationSize = textureSize;

	VkMemoryRequirements memReqs;
	memset(&memReqs, 0, sizeof(VkMemoryRequirements));

	// This buffer will be the data source for copying texture data to the optimal tiled image on the device
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingMemory;

	VkBufferCreateInfo bufferCreateInfo;
	memset(&bufferCreateInfo, 0, sizeof(VkBufferCreateInfo));
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = textureSize;
	// This buffer is used as a transfer source for the buffer copy
	bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VK_CHECK_RESULT(vkCreateBuffer(gfx->device, &bufferCreateInfo, 0, &stagingBuffer));

	// Get memory requirements for the staging buffer (alignment, memory type bits)
	vkGetBufferMemoryRequirements(gfx->device, stagingBuffer, &memReqs);
	memAllocInfo.allocationSize = memReqs.size;
	// Get memory type index for a host visible buffer
	memAllocInfo.memoryTypeIndex = getMemoryType(gfx, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &memFound);
	VK_CHECK_RESULT(vkAllocateMemory(gfx->device, &memAllocInfo, 0, &stagingMemory));
	VK_CHECK_RESULT(vkBindBufferMemory(gfx->device, stagingBuffer, stagingMemory, 0));

	// Copy texture data into host local staging buffer
	uint8_t* data;
	VK_CHECK_RESULT(vkMapMemory(gfx->device, stagingMemory, 0, memReqs.size, 0, (void**)&data));
	memcpy(data, pData, textureSize);
	vkUnmapMemory(gfx->device, stagingMemory);

	// Setup a buffer image copy structure for the current mip level
	VkBufferImageCopy bufferCopyRegion;
	memset(&bufferCopyRegion, 0, sizeof(VkBufferImageCopy));
	bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	bufferCopyRegion.imageSubresource.mipLevel = 0;
	bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
	bufferCopyRegion.imageSubresource.layerCount = 1;
	bufferCopyRegion.imageExtent.width = width;
	bufferCopyRegion.imageExtent.height = height;
	bufferCopyRegion.imageExtent.depth = 1;
	bufferCopyRegion.bufferOffset = 0;

	// Create optimal tiled target image on the device
	VkImageCreateInfo imageCreateInfo;
	memset(&imageCreateInfo, 0, sizeof(VkImageCreateInfo));
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.format = format;
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	// Set initial layout of the image to undefined
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageCreateInfo.extent.width = width;
	imageCreateInfo.extent.height = height;
	imageCreateInfo.extent.depth = 1;
	imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	VK_CHECK_RESULT(vkCreateImage(gfx->device, &imageCreateInfo, 0, &vulkanTexture->image));

	vkGetImageMemoryRequirements(gfx->device, vulkanTexture->image, &memReqs);
	memAllocInfo.allocationSize = memReqs.size;
	memAllocInfo.memoryTypeIndex = getMemoryType(gfx, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memFound);
	VK_CHECK_RESULT(vkAllocateMemory(gfx->device, &memAllocInfo, 0, &vulkanTexture->deviceMemory));
	VK_CHECK_RESULT(vkBindImageMemory(gfx->device, vulkanTexture->image, vulkanTexture->deviceMemory, 0));

	VkCommandBuffer copyCmd = gfx->cmdBuffer.drawCmdBuffers[0];

	VkCommandBufferBeginInfo info;
	memset(&info, 0, sizeof(VkCommandBufferBeginInfo));
	info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	vkBeginCommandBuffer(copyCmd, &info);

	// Image memory barriers for the texture image

	// The sub resource range describes the regions of the image that will be transitioned using the memory barriers below
	VkImageSubresourceRange subresourceRange;
	memset(&subresourceRange, 0, sizeof(VkImageSubresourceRange));
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = 1;
	subresourceRange.layerCount = 1;

	// Transition the texture image layout to transfer target, so we can safely copy our buffer data to it.
	VkImageMemoryBarrier imageMemoryBarrier;
	memset(&imageMemoryBarrier, 0, sizeof(VkImageMemoryBarrier));
	imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageMemoryBarrier.image = vulkanTexture->image;
	imageMemoryBarrier.subresourceRange = subresourceRange;
	imageMemoryBarrier.srcAccessMask = 0;
	imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

	// Insert a memory dependency at the proper pipeline stages that will execute the image layout transition
	// Source pipeline stage is host write/read execution (VK_PIPELINE_STAGE_HOST_BIT)
	// Destination pipeline stage is copy command execution (VK_PIPELINE_STAGE_TRANSFER_BIT)
	vkCmdPipelineBarrier(
		copyCmd,
		VK_PIPELINE_STAGE_HOST_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		0,
		0, 0,
		0, 0,
		1, &imageMemoryBarrier);

	// Copy mip levels from staging buffer
	vkCmdCopyBufferToImage(
		copyCmd,
		stagingBuffer,
		vulkanTexture->image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&bufferCopyRegion);

	// Once the data has been uploaded we transfer to the texture image to the shader read layout, so it can be sampled from
	imageMemoryBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	// Insert a memory dependency at the proper pipeline stages that will execute the image layout transition
	// Source pipeline stage is copy command execution (VK_PIPELINE_STAGE_TRANSFER_BIT)
	// Destination pipeline stage fragment shader access (VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT)
	vkCmdPipelineBarrier(
		copyCmd,
		VK_PIPELINE_STAGE_TRANSFER_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
		0,
		0, 0,
		0, 0,
		1, &imageMemoryBarrier);

	// Store current layout for later reuse
	//texture.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	VK_CHECK_RESULT(vkEndCommandBuffer(copyCmd));

	VkSubmitInfo submitInfo;
	memset(&submitInfo, 0, sizeof(VkSubmitInfo));
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &copyCmd;

	// Submit to the queue
	vkResetFences(gfx->device, 1, &gfx->cmdBuffer.waitFences[0]);
	VK_CHECK_RESULT(vkQueueSubmit(gfx->queue, 1, &submitInfo, gfx->cmdBuffer.waitFences[0]));
	vkQueueWaitIdle(gfx->queue);
	//VK_CHECK_RESULT(vkWaitForFences(gfx->device, 1, &gfx->cmdBuffer.waitFences[0], VK_TRUE, DEFAULT_FENCE_TIMEOUT));
	//VkResult res = vkResetFences(gfx->device, 1, &gfx->cmdBuffer.waitFences[0]);

	// Clean up staging resources
	vkFreeMemory(gfx->device, stagingMemory, 0);
	vkDestroyBuffer(gfx->device, stagingBuffer, 0);


	//ktxTexture_Destroy(ktxTexture);

	// Create image view
	// Textures are not directly accessed by the shaders and
	// are abstracted by image views containing additional
	// information and sub resource ranges
	VkImageViewCreateInfo view;
	memset(&view, 0, sizeof(VkImageViewCreateInfo));
	view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view.format = format;
	// The subresource range describes the set of mip levels (and array layers) that can be accessed through this image view
	// It's possible to create multiple image views for a single image referring to different (and/or overlapping) ranges of the image
	view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	view.subresourceRange.baseMipLevel = 0;
	view.subresourceRange.baseArrayLayer = 0;
	view.subresourceRange.layerCount = 1;
	// Linear tiling usually won't support mip maps
	// Only set mip map count if optimal tiling is used
	view.subresourceRange.levelCount = 1;
	// The view will be based on the texture's image
	view.image = vulkanTexture->image;
	VK_CHECK_RESULT(vkCreateImageView(gfx->device, &view, 0, &vulkanTexture->view));

	VkDescriptorImageInfo imageDescriptor;
	imageDescriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageDescriptor.imageView = vulkanTexture->view;
	imageDescriptor.sampler = gfx->linearSampler;

	vulkanTexture->descriptor = imageDescriptor;

	return 0;
}

static int
create_ubo(struct gfx* gfx) {

	for (int i = 0; i < BUFFER_COUNT; i++) {
		VkBufferCreateInfo vertexBufferInfo;
		memset(&vertexBufferInfo, 0, sizeof(VkBufferCreateInfo));
		vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		vertexBufferInfo.size = sizeof(struct bgUbo);
		vertexBufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

		vkCreateBuffer(gfx->device, &vertexBufferInfo, 0, &gfx->bg.ubo[i].buffer.buffer);

		VkBool32* memFound = 0;
		VkMemoryRequirements memReqs;
		VkMemoryAllocateInfo memAlloc;
		memset(&memAlloc, 0, sizeof(VkMemoryAllocateInfo));

		memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		vkGetBufferMemoryRequirements(gfx->device, gfx->bg.ubo[i].buffer.buffer, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = getMemoryType(gfx, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &memFound);
		VK_CHECK_RESULT(vkAllocateMemory(gfx->device, &memAlloc, 0, &gfx->bg.ubo[i].buffer.memory));

		VK_CHECK_RESULT(vkBindBufferMemory(gfx->device, gfx->bg.ubo[i].buffer.buffer, gfx->bg.ubo[i].buffer.memory, 0));
		vkMapMemory(gfx->device, gfx->bg.ubo[i].buffer.memory, 0, memAlloc.allocationSize, 0, &gfx->bg.ubo[i].pData);

		VkDescriptorBufferInfo descriptor;
		descriptor.buffer = gfx->bg.ubo[i].buffer.buffer;
		descriptor.range = sizeof(struct bgUbo);
		descriptor.offset = 0;

		gfx->bg.ubo[i].bufferDescrptor = descriptor;
	}
}

static int
bg_load(struct gfx* gfx, struct bg* bg, const struct resource_pack* pack)
{
	int img_width, img_height, img_channels;
	stbi_uc* img_data;

	///* Clean up previous data */
	//if (bg->program != 0)
	//	glDeleteProgram(bg->program);
	//bg->program = 0;

	int cc = 4;
	int width, height;
	void* pData = stbi_load(pack->sprites.background[0]->texture0, &width, &height, &cc, 4);
	//assert(cc == 4);

	load_texture(gfx, &bg->texCol, pData, width, height, cc);

	pData = stbi_load(pack->sprites.background[0]->texture1, &width, &height, &cc, 4);
	//assert(cc == 4);

	load_texture(gfx, &bg->texNor, pData, width, height, cc);

	/* For now we only support a single background layer */
	if (pack->sprites.background[0] == NULL)
	{
		log_warn("No background texture defined\n");
		return -1;
	}

	/* Load shaders */
	bg->bgPipeline = CreatePipeline(gfx, pack->shaders.glsl.background);

	create_ubo(gfx);
	//if (bg->program == 0)
	//	return -1;

	//bg->uAspectRatio = get_uniform_location_and_warn(bg->program, "uAspectRatio");
	//bg->uCamera = get_uniform_location_and_warn(bg->program, "uCamera");
	//bg->uShadowInvRes = get_uniform_location_and_warn(bg->program, "uShadowInvRes");
	//bg->sShadow = get_uniform_location_and_warn(bg->program, "sShadow");
	//bg->sCol = get_uniform_location_and_warn(bg->program, "sCol");
	//bg->sNM = get_uniform_location_and_warn(bg->program, "sNM");

	return 0;
}

int
gfx_load_resource_pack(struct gfx* gfx, const struct resource_pack* pack)
{
	bg_load(gfx, &gfx->bg, pack);



	//gfx->sprite_shadow.spriteShadowPipeline = CreatePipeline(pack->shaders.glsl.shadow, attr_bindings);
	
	
	//if (gfx->sprite_shadow.program == 0)
	//	goto glsl_shadow_failed;

	/*
	gfx->sprite_shadow.uAspectRatio = glGetUniformLocation(gfx->sprite_shadow.program, "uAspectRatio");
	gfx->sprite_shadow.uPosCameraSpace = glGetUniformLocation(gfx->sprite_shadow.program, "uPosCameraSpace");
	gfx->sprite_shadow.uDir = glGetUniformLocation(gfx->sprite_shadow.program, "uDir");
	gfx->sprite_shadow.uSize = glGetUniformLocation(gfx->sprite_shadow.program, "uSize");
	gfx->sprite_shadow.uAnim = glGetUniformLocation(gfx->sprite_shadow.program, "uAnim");
	gfx->sprite_shadow.sNM = glGetUniformLocation(gfx->sprite_shadow.program, "sNM");*/


	/*
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
	*/
	return 0;

glsl_sprite_failed:
	//glDeleteProgram(gfx->sprite_shadow.program);
glsl_shadow_failed:

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


static void
UpdateDescriptorSets(struct gfx* gfx, uint32_t curr) {
	VkWriteDescriptorSet writeDescriptor[2];
	memset(&writeDescriptor, 0, sizeof(VkWriteDescriptorSet) * 2);
	writeDescriptor[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescriptor[0].descriptorCount = 1;
	writeDescriptor[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writeDescriptor[0].pBufferInfo = &gfx->bg.ubo[curr].bufferDescrptor;
	writeDescriptor[0].dstSet = gfx->bg.bgDescriptorSet;
	writeDescriptor[0].dstBinding = 0;

	VkDescriptorImageInfo images[2] = {
		gfx->bg.texCol.descriptor,
		gfx->bg.texNor.descriptor
	};
	writeDescriptor[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescriptor[1].descriptorCount = 2;
	writeDescriptor[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeDescriptor[1].pImageInfo = &images;
	writeDescriptor[1].dstSet = gfx->bg.bgDescriptorSet;
	writeDescriptor[1].dstBinding = 1;

	vkUpdateDescriptorSets(gfx->device, 2, &writeDescriptor, 0, 0);
}

static void
draw_background(struct gfx* gfx, const struct camera* camera, const struct aspect_ratio* ar, uint32_t curr)
{

	/*
	glBindBuffer(GL_ARRAY_BUFFER, gfx->bg.vbo);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(struct vertex), (void*)offsetof(struct vertex, pos));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(struct vertex), (void*)offsetof(struct vertex, uv));
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gfx->bg.ibo);*/

	//VkDeviceOffsets
	VkDeviceSize offsets[1] = { 0 };
	vkCmdBindVertexBuffers(gfx->cmdBuffer.drawCmdBuffers[curr], 0, 1, &gfx->bg.vbo.buffer, offsets);
	vkCmdBindIndexBuffer(gfx->cmdBuffer.drawCmdBuffers[curr], gfx->bg.ibo.buffer, 0, VK_INDEX_TYPE_UINT16);

	UpdateDescriptorSets(gfx, curr);
	vkCmdBindDescriptorSets(gfx->cmdBuffer.drawCmdBuffers[curr], VK_PIPELINE_BIND_POINT_GRAPHICS, gfx->bg.pipelineLayout, 0, 1, &gfx->bg.bgDescriptorSet, 0, 0);

	vkCmdBindPipeline(gfx->cmdBuffer.drawCmdBuffers[curr], VK_PIPELINE_BIND_POINT_GRAPHICS, gfx->bg.bgPipeline);

	/*
	glUseProgram(gfx->bg.bgPipeline);
	glBindTexture(GL_TEXTURE_2D, gfx->bg.texShadow);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, gfx->bg.texCol);
	glActiveTexture(GL_TEXTURE2);
	glBindTexture(GL_TEXTURE_2D, gfx->bg.texNor);

	*/
	gfx->bg.bgubo.uAspectRatio[0] = ar->scale_x; gfx->bg.bgubo.uAspectRatio[1] = ar->scale_y; gfx->bg.bgubo.uAspectRatio[2] = ar->pad_x; gfx->bg.bgubo.uAspectRatio[3] = ar->pad_y;
	gfx->bg.bgubo.uCamera[0] = qw_to_float(camera->pos.x); gfx->bg.bgubo.uCamera[1] = qw_to_float(camera->pos.y); gfx->bg.bgubo.uCamera[2] = qw_to_float(camera->scale);
	gfx->bg.bgubo.uShadowInvRes[0] = (GLfloat)SHADOW_MAP_SIZE_FACTOR / gfx->width; gfx->bg.bgubo.uShadowInvRes[1] = (GLfloat)SHADOW_MAP_SIZE_FACTOR / gfx->height;

	memcpy(gfx->bg.ubo[curr].pData, &gfx->bg.bgubo, sizeof(struct bgUbo));
	//gfx->bg.bgubo.sCol, 1);
	//gfx->bg.bgubo.sNM, 2);

	vkCmdDrawIndexed(gfx->cmdBuffer.drawCmdBuffers[curr], 6, 1, 0, 0, 0);

	/*
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE1);
	glBindTexture(GL_TEXTURE_2D, 0);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glUseProgram(0);*/
}


void
gfx_draw_world(struct gfx* gfx, const struct world* world, const struct camera* camera)
{
	BeginRecording(gfx);
	CmdBeginRenderPass(gfx);

	struct aspect_ratio ar = { 1.0, 1.0, 0.0, 0.0 };
	if (gfx->width > gfx->height)
	{
		ar.scale_x = (GLfloat)gfx->width / gfx->height;
		ar.pad_x = (ar.scale_x - 1.0) / 2.0;
	}
	else
	{
		ar.scale_y = (GLfloat)gfx->height / gfx->width;
		ar.pad_y = (ar.scale_y - 1.0) / 2.0;
	}

	//WORLD_FOR_EACH_SNAKE(world, snake_id, snake)
	//	draw_snake(snake, gfx, camera, &ar, 1);
	//WORLD_END_EACH

	int curr = gfx->currentBuffer;
	draw_background(gfx, camera, &ar, curr);
	//draw_0_0(gfx, camera, &ar);

	//WORLD_FOR_EACH_SNAKE(world, snake_id, snake)
	//	draw_snake(snake, gfx, camera, &ar, 0);
	//WORLD_END_EACH

	CmdEndRenderPass(gfx);
	EndRecording(gfx);
	SubmitCommandBuffers(gfx);
}


static
int
BeginRecording(struct gfx* gfx) {

	struct CommandBuffer* pCommandBuffer = &gfx->cmdBuffer;

	VkResult vkResult = vkAcquireNextImageKHR(gfx->device, gfx->swapChain.swapChain, UINT64_MAX, gfx->presentComplete[gfx->currentBuffer], (VkFence)NULL, &gfx->currentBuffer);
	if (vkResult != VK_SUCCESS)
		return -1;

	int i = gfx->currentBuffer;

	// Wait for the fence to signal that command buffer has finished executing
	vkResult = vkWaitForFences(gfx->device, 1, &pCommandBuffer->waitFences[i], VK_TRUE, DEFAULT_FENCE_TIMEOUT);
	//if (vkResult != VK_SUCCESS)
	//	return -1;
	vkResult = vkResetFences(gfx->device, 1, &pCommandBuffer->waitFences[i]);
	if (vkResult != VK_SUCCESS)
		return -1;
	vkResult = vkResetCommandBuffer(pCommandBuffer->drawCmdBuffers[i], 0);
	if (vkResult != VK_SUCCESS)
		return -1;

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
	fpQueuePresentKHR(gfx->queue, &presentInfo);

	return 1;
}
