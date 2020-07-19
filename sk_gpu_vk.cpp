#ifdef SKR_VULKAN
#include "sk_gpu.h"

///////////////////////////////////////////

#pragma comment(lib,"vulkan-1.lib")
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include <stdio.h>

///////////////////////////////////////////

template <typename T> struct array_t {
	T     *data;
	size_t count;
	size_t capacity;

	size_t      add        (const T &item)           { if (count+1 > capacity) { resize(capacity * 2 < 4 ? 4 : capacity * 2); } data[count] = item; count += 1; return count - 1; }
	void        insert     (size_t at, const T &item){ if (count+1 > capacity) resize(capacity<1?1:capacity*2); memmove(&data[at+1], &data[at], (count-at)*sizeof(T)); memcpy(&data[at], &item, sizeof(T)); count += 1;}
	void        trim       ()                        { resize(count); }
	void        remove     (size_t at)               { memmove(&data[at], &data[at+1], (count - (at + 1))*sizeof(T)); count -= 1; }
	void        pop        ()                        { remove(count - 1); }
	void        clear      ()                        { count = 0; }
	T          &last       () const                  { return data[count - 1]; }
	inline void set        (size_t id, const T &val) { data[id] = val; }
	inline T   &get        (size_t id) const         { return data[id]; }
	inline T   &operator[] (size_t id) const         { return data[id]; }
	void        reverse    ()                        { for(size_t i=0; i<count/2; i+=1) {T tmp = get(i);set(i, get(count-i-1));set(count-i-1, tmp);}};
	array_t<T>  copy       () const                  { array_t<T> result = {malloc(sizeof(T) * capacity),count,capacity}; memcpy(result.data, data, sizeof(T) * count); return result; }
	void        each       (void (*e)(T &))          { for (size_t i=0; i<count; i++) e(data[i]); }
	void        free       ()                        { ::free(data); *this = {}; }
	void        resize     (size_t to_capacity)      { if (count > to_capacity) count = to_capacity; void *old = data; void *new_mem = malloc(sizeof(T) * to_capacity); memcpy(new_mem, old, sizeof(T) * count); data = (T*)new_mem; ::free(old); capacity = to_capacity; }
	int64_t     index_of   (const T &item) const     { for (size_t i = 0; i < count; i++) if (memcmp(data[i], item, sizeof(T)) == 0) return i; return -1; }
	template <typename T, typename D>
	int64_t     index_of   (const D T::*key, const D &item) const { const size_t offset = (size_t)&((T*)0->*key); for (size_t i = 0; i < count; i++) if (memcmp(((uint8_t *)&data[i]) + offset, &item, sizeof(D)) == 0) return i; return -1; }
};

uint64_t hash_fnv64_data(const void *data, size_t data_size, uint64_t start_hash = 14695981039346656037) {
	uint64_t hash = start_hash;
	uint8_t *bytes = (uint8_t *)data;
	for (size_t i = 0; i < data_size; i++)
		hash = (hash ^ bytes[i]) * 1099511628211;
	return hash;
}

//////////////////////////////////////

struct skr_device_t {
	VkSurfaceKHR     surface;
	VkPhysicalDevice phys_device;
	VkDevice         device;
	VkQueue          queue_gfx;
	VkQueue          queue_present;
	uint32_t         queue_gfx_index;
	uint32_t         queue_present_index;
};

struct vk_swapchain_t {
	VkSurfaceFormatKHR format;
	VkSwapchainKHR     swapchain;
	uint32_t           img_count;
	VkImage            imgs[4];
	VkExtent2D         extents;
};

skr_device_t skr_device = {};

//////////////////////////////////////
// Pipeline & Renderpass Info       //
//////////////////////////////////////

struct vk_renderpass_t {
	uint64_t     hash;
	int32_t      ref_count;
	VkRenderPass renderpass;
};
struct vk_pipeline_info_t {
	VkGraphicsPipelineCreateInfo           create;
	VkPipelineColorBlendStateCreateInfo    blend_info;
	VkPipelineColorBlendAttachmentState    blend_attch;
	VkPipelineShaderStageCreateInfo        shader_stages[2];
	VkPipelineVertexInputStateCreateInfo   vertex_info;
	VkPipelineInputAssemblyStateCreateInfo input_asm;
	VkPipelineRasterizationStateCreateInfo rasterizer;
	VkPipelineMultisampleStateCreateInfo   multisample;
	VkPipelineDepthStencilStateCreateInfo  depth_info;
	VkDynamicState                         dynamic_states[1];
	VkPipelineDynamicStateCreateInfo       dynamic_state;
};
struct vk_pipeline_t {
	uint64_t            hash;
	int32_t             ref_count;
	array_t<VkPipeline> pipelines;
	vk_pipeline_info_t  info;
};
array_t<vk_renderpass_t> vk_renderpass_cache = {};
array_t<vk_pipeline_t>   vk_pipeline_cache   = {};

//////////////////////////////////////
// Pipelines                        //
//////////////////////////////////////

uint64_t _vk_pipeline_hash(VkGraphicsPipelineCreateInfo &info) {
	uint64_t hash = hash_fnv64_data(&info.flags, sizeof(VkPipelineCreateFlags));
	hash = hash_fnv64_data(&info.basePipelineIndex, sizeof(int32_t),  hash);
	hash = hash_fnv64_data(&info.stageCount,        sizeof(uint32_t), hash);
	hash = hash_fnv64_data(&info.subpass,           sizeof(uint32_t), hash);
	if (info.pDepthStencilState ) hash = hash_fnv64_data(info.pDepthStencilState,  sizeof(VkPipelineDepthStencilStateCreateInfo),  hash);
	if (info.pInputAssemblyState) hash = hash_fnv64_data(info.pInputAssemblyState, sizeof(VkPipelineInputAssemblyStateCreateInfo), hash);
	if (info.pMultisampleState  ) hash = hash_fnv64_data(info.pMultisampleState,   sizeof(VkPipelineMultisampleStateCreateInfo),   hash);
	if (info.pRasterizationState) hash = hash_fnv64_data(info.pRasterizationState, sizeof(VkPipelineRasterizationStateCreateInfo), hash);
	for (size_t i = 0; i < info.stageCount; i++) {
		hash = hash_fnv64_data(&info.pStages[i], sizeof(VkPipelineShaderStageCreateInfo), hash);
	}
	return hash;
}
void _vk_pipeline_copy(vk_pipeline_info_t &dest, vk_pipeline_info_t &from) {
	memcpy(&dest, &from, sizeof(vk_pipeline_info_t));
	dest.dynamic_state.pDynamicStates = dest.dynamic_states;
	dest.blend_info.pAttachments = &dest.blend_attch;
	dest.create.pColorBlendState   = &dest.blend_info;
	dest.create.pDepthStencilState = &dest.depth_info;
	dest.create.pDynamicState      = &dest.dynamic_state;
	dest.create.pInputAssemblyState= &dest.input_asm;
	dest.create.pMultisampleState  = &dest.multisample;
	dest.create.pRasterizationState= &dest.rasterizer;
	dest.create.pStages            =  dest.shader_stages;
	//dest.create.pTessellationState
	dest.create.pVertexInputState  = &dest.vertex_info;
}
void _vk_pipelines_addpass(int64_t pass) {
	for (size_t i = 0; i < vk_pipeline_cache.count; i++) {
		while (vk_pipeline_cache[i].pipelines.count < pass) vk_pipeline_cache[i].pipelines.add({});
		vk_pipeline_cache[i].info.create.renderPass = vk_renderpass_cache[pass].renderpass;
		vkCreateGraphicsPipelines(skr_device.device, VK_NULL_HANDLE, 1, &vk_pipeline_cache[i].info.create, nullptr, &vk_pipeline_cache[i].pipelines[pass]);
	}
}
void _vk_pipelines_rempass(int64_t pass) {
	for (size_t i = 0; i < vk_pipeline_cache.count; i++) {
		if (vk_pipeline_cache[i].pipelines.count < pass) continue;
		vk_pipeline_cache[i].info.create.renderPass = vk_renderpass_cache[pass].renderpass;
		vkDestroyPipeline(skr_device.device, vk_pipeline_cache[i].pipelines[pass], nullptr);
	}
}

int64_t vk_pipeline_ref(vk_pipeline_info_t &info) {
	uint64_t hash  = _vk_pipeline_hash(info.create);
	int64_t  index = vk_pipeline_cache.index_of(&vk_pipeline_t::hash, hash);
	if (index < 0) {
		index = vk_pipeline_cache.count;
		vk_pipeline_cache.add({});
		vk_pipeline_cache[index].hash = hash;
		_vk_pipeline_copy(vk_pipeline_cache[index].info, info);
	}
	if (vk_pipeline_cache[index].ref_count == 0) {
		for (size_t i = 0; i < vk_renderpass_cache.count; i++) {
			VkPipeline pipeline = {};
			vk_pipeline_cache[index].info.create.renderPass = vk_renderpass_cache[i].renderpass;
			vkCreateGraphicsPipelines(skr_device.device, VK_NULL_HANDLE, 1, &vk_pipeline_cache[index].info.create, nullptr, &pipeline);
			vk_pipeline_cache[index].pipelines.add(pipeline);
		}
	}
	vk_pipeline_cache[index].ref_count += 1;
	return index;
}
void vk_pipeline_release(int64_t id) {
	vk_pipeline_cache[id].ref_count -= 1;
	if (vk_pipeline_cache[id].ref_count == 0) {
		for (size_t i = 0; i < vk_pipeline_cache[id].pipelines.count; i++) {
			vkDestroyPipeline(skr_device.device, vk_pipeline_cache[id].pipelines[i], nullptr);
		}
		vk_pipeline_cache[id].pipelines.free();
	}
}

//////////////////////////////////////
// Renderpasses                     //
//////////////////////////////////////

uint64_t _vk_renderpass_hash(VkRenderPassCreateInfo &info) {
	uint64_t hash = hash_fnv64_data(&info.flags, sizeof(VkRenderPassCreateFlags));
	hash = hash_fnv64_data(&info.attachmentCount, sizeof(uint32_t), hash);
	hash = hash_fnv64_data(&info.dependencyCount, sizeof(uint32_t), hash);
	hash = hash_fnv64_data(&info.subpassCount,    sizeof(uint32_t), hash);

	for (uint32_t i = 0; i < info.attachmentCount; i++) hash = hash_fnv64_data(&info.pAttachments [i], sizeof(VkAttachmentDescription), hash);
	for (uint32_t i = 0; i < info.dependencyCount; i++) hash = hash_fnv64_data(&info.pDependencies[i], sizeof(VkSubpassDependency), hash);
	for (uint32_t i = 0; i < info.subpassCount; i++) {
		const VkSubpassDescription *pass = &info.pSubpasses[i];
		hash = hash_fnv64_data(&pass->flags, sizeof(VkSubpassDescriptionFlags), hash);
		if (pass->pDepthStencilAttachment) hash = hash_fnv64_data(&pass->pDepthStencilAttachment, sizeof(VkAttachmentReference), hash);
		for (uint32_t t = 0; t < pass->colorAttachmentCount;    t++) hash = hash_fnv64_data(&pass->pColorAttachments[t], sizeof(VkAttachmentReference), hash);
		for (uint32_t t = 0; t < pass->inputAttachmentCount;    t++) hash = hash_fnv64_data(&pass->pInputAttachments[t], sizeof(VkAttachmentReference), hash);
		for (uint32_t t = 0; t < pass->preserveAttachmentCount; t++) hash = hash_fnv64_data(&pass->pPreserveAttachments[t], sizeof(uint32_t), hash);
	}
	return hash;
}
int64_t vk_renderpass_ref(VkRenderPassCreateInfo &info) {
	uint64_t hash  = _vk_renderpass_hash(info);
	int64_t  index = vk_renderpass_cache.index_of(&vk_renderpass_t::hash, hash);
	if (index < 0) {
		index = vk_renderpass_cache.count;
		vk_renderpass_cache.add({});
		vk_renderpass_cache[index].hash = hash;
	}
	if (vk_renderpass_cache[index].ref_count == 0) {
		vkCreateRenderPass(skr_device.device, &info, nullptr, &vk_renderpass_cache[index].renderpass);
		_vk_pipelines_addpass(index);
	}
	vk_renderpass_cache[index].ref_count += 1;
	return index;
}
void vk_renderpass_release(int64_t id) {
	vk_renderpass_cache[id].ref_count -= 1;
	if (vk_renderpass_cache[id].ref_count == 0) {
		_vk_pipelines_rempass(id);
		vkDestroyRenderPass(skr_device.device, vk_renderpass_cache[id].renderpass, nullptr);
	}
}

//////////////////////////////////////

#define D3D_FRAME_COUNT 2

VkInstance    vk_inst               = VK_NULL_HANDLE;
VkCommandPool vk_cmd_pool           = VK_NULL_HANDLE;
VkCommandPool vk_cmd_pool_transient = VK_NULL_HANDLE;

// Vertex layout info
VkVertexInputBindingDescription      skr_vert_bind    = { 0, sizeof(skr_vert_t), VK_VERTEX_INPUT_RATE_VERTEX };
VkVertexInputAttributeDescription    skr_vert_attrs[] = {
	{ 0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(skr_vert_t, pos)  },
	{ 1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(skr_vert_t, norm) },
	{ 2, 0, VK_FORMAT_R32G32_SFLOAT,    offsetof(skr_vert_t, uv)   },
	{ 3, 0, VK_FORMAT_R8G8B8A8_UNORM,   offsetof(skr_vert_t, col)  }, };
VkPipelineVertexInputStateCreateInfo skr_vertex_layout = { 
	VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO, nullptr, 0,
	1,                       &skr_vert_bind,
	_countof(skr_vert_attrs), skr_vert_attrs };

const skr_tex_t *skr_active_rendertarget = nullptr;
VkPipeline *vk_active_pipeline = nullptr;

skr_tex_fmt_ skr_native_to_tex_fmt(VkFormat format);

///////////////////////////////////////////

bool vk_create_instance(const char *app_name, VkInstance *out_inst) {
	VkApplicationInfo app_info = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
	app_info.pApplicationName   = app_name;
	app_info.pEngineName        = "No Engine";
	app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
	app_info.apiVersion         = VK_API_VERSION_1_0;

	// Create instance
	const char *ext[] = { 
		
		VK_KHR_SURFACE_EXTENSION_NAME,
		VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
		VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
		 };
	const char *layers[] = {
		"VK_LAYER_KHRONOS_validation"
	};
	VkInstanceCreateInfo create_info = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	create_info.pApplicationInfo        = &app_info;
	create_info.enabledExtensionCount   = _countof(ext);
	create_info.ppEnabledExtensionNames = ext;
	create_info.enabledLayerCount       = _countof(layers);
	create_info.ppEnabledLayerNames     = layers;

	VkResult result = vkCreateInstance(&create_info, 0, out_inst);

	VkDebugReportCallbackCreateInfoEXT callback_info = { VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT };
	callback_info.flags = 
		//VK_DEBUG_REPORT_INFORMATION_BIT_EXT |
		VK_DEBUG_REPORT_WARNING_BIT_EXT | 
		VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT  | 
		VK_DEBUG_REPORT_ERROR_BIT_EXT  | 
		VK_DEBUG_REPORT_DEBUG_BIT_EXT ;
	callback_info.pfnCallback = [](
		VkDebugReportFlagsEXT                       flags,
		VkDebugReportObjectTypeEXT                  objectType,
		uint64_t                                    object,
		size_t                                      location,
		int32_t                                     messageCode,
		const char*                                 pLayerPrefix,
		const char*                                 pMessage,
		void*                                       pUserData) {
			printf("%s\n",pMessage);
			return (VkBool32)VK_FALSE;
	};
	VkDebugReportCallbackEXT callback;

	PFN_vkCreateDebugReportCallbackEXT debugCallback = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(*out_inst, "vkCreateDebugReportCallbackEXT");
	debugCallback(*out_inst, &callback_info, nullptr, &callback);

	return result == VK_SUCCESS;
}

///////////////////////////////////////////

bool vk_create_device(VkInstance inst, void *app_hwnd, skr_device_t *out_device) {
	*out_device = {};

	// Create win32 surface
	VkWin32SurfaceCreateInfoKHR surface_info = { VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
	surface_info.hinstance = GetModuleHandle(0);
	surface_info.hwnd      = (HWND)app_hwnd;
	if (vkCreateWin32SurfaceKHR(inst, &surface_info, nullptr, &out_device->surface) != VK_SUCCESS)
		return false;

	// Get physical device list
	uint32_t          device_count;
	VkPhysicalDevice *device_handles;
	vkEnumeratePhysicalDevices(inst, &device_count, 0);
	if (device_count == 0)
		return false;
	device_handles = (VkPhysicalDevice*)malloc(sizeof(VkPhysicalDevice) * device_count);
	vkEnumeratePhysicalDevices(inst, &device_count, device_handles);

	// Pick a physical device that meets our requirements
	VkQueueFamilyProperties         *queue_props;
	VkPhysicalDeviceProperties       device_props;
	VkPhysicalDeviceFeatures         device_features;
	VkPhysicalDeviceMemoryProperties device_mem_props;
	int32_t          max_score         = 0;
	uint32_t         max_gfx_index     = 0;
	uint32_t         max_present_index = 0;
	VkPhysicalDevice max_device        = {};
	for (uint32_t i = 0; i < device_count; i++) {
		int32_t score = 10;
		int32_t gfx_index = -1;
		int32_t present_index = -1;

		// Check if it has a queue for presenting, and graphics
		uint32_t queue_count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device_handles[i], &queue_count, NULL);
		queue_props = (VkQueueFamilyProperties*)malloc(sizeof(VkQueueFamilyProperties) * queue_count);
		vkGetPhysicalDeviceQueueFamilyProperties(device_handles[i], &queue_count, queue_props);
		for (uint32_t j = 0; j < queue_count; ++j) {
			VkBool32 supports_present = VK_FALSE;
			vkGetPhysicalDeviceSurfaceSupportKHR(device_handles[i], j, out_device->surface, &supports_present);
			
			if (supports_present)
				present_index = j;

			if (queue_props[j].queueFlags & VK_QUEUE_GRAPHICS_BIT)
				gfx_index = j;
		}

		// Get information about the device
		vkGetPhysicalDeviceProperties      (device_handles[i], &device_props);
		vkGetPhysicalDeviceFeatures        (device_handles[i], &device_features);
		vkGetPhysicalDeviceMemoryProperties(device_handles[i], &device_mem_props);

		// Score the device
		if (device_props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
			score += 1000;
		score += device_props.limits.maxImageDimension2D;
		if (gfx_index == -1 || present_index == -1)
			score = 0;

		// And record it if it was the best scoring device
		free(queue_props);
		if (score > max_score) {
			max_score         = score;
			max_gfx_index     = gfx_index;
			max_present_index = present_index;
			max_device        = device_handles[i];
			break;
		}
	}
	out_device->phys_device         = max_device;
	out_device->queue_gfx_index     = max_gfx_index;
	out_device->queue_present_index = max_present_index;
	free(device_handles);
	if (max_score == 0)
		return false;

	// Create a logical device from the physical device
	const float queue_priority = 1.0f;
	VkDeviceQueueCreateInfo device_queue_info[2] = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
	device_queue_info[0] = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
	device_queue_info[0].queueFamilyIndex = out_device->queue_gfx_index;
	device_queue_info[0].queueCount       = 1;
	device_queue_info[0].pQueuePriorities = &queue_priority;
	device_queue_info[1] = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
	device_queue_info[1].queueFamilyIndex = out_device->queue_present_index;
	device_queue_info[1].queueCount       = 1;
	device_queue_info[1].pQueuePriorities = &queue_priority;

	const char *enabled_exts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	VkDeviceCreateInfo device_create_info = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	device_create_info.queueCreateInfoCount    = _countof(device_queue_info);
	device_create_info.pQueueCreateInfos       = device_queue_info;
	device_create_info.enabledExtensionCount   = _countof(enabled_exts);
	device_create_info.ppEnabledExtensionNames = enabled_exts;

	if (vkCreateDevice(out_device->phys_device, &device_create_info, NULL, &out_device->device) != VK_SUCCESS)
		return false;

	vkGetDeviceQueue(out_device->device, out_device->queue_gfx_index,     0, &out_device->queue_gfx);
	vkGetDeviceQueue(out_device->device, out_device->queue_present_index, 0, &out_device->queue_present);
	return true;
}

///////////////////////////////////////////

VkSurfaceFormatKHR vk_get_preferred_fmt(skr_device_t &device) {
	VkSurfaceFormatKHR result;
	uint32_t format_count = 1;
	vkGetPhysicalDeviceSurfaceFormatsKHR(device.phys_device, device.surface, &format_count, nullptr);
	VkSurfaceFormatKHR *formats = (VkSurfaceFormatKHR *)malloc(format_count * sizeof(VkSurfaceFormatKHR));
	vkGetPhysicalDeviceSurfaceFormatsKHR(device.phys_device, device.surface, &format_count, formats);
	result = formats[0];
	free(formats);
	result.format = result.format == VK_FORMAT_UNDEFINED ? VK_FORMAT_B8G8R8A8_UNORM : result.format;
	return result;
}

///////////////////////////////////////////

VkPresentModeKHR vk_get_presentation_mode(skr_device_t &device) {
	VkPresentModeKHR  result     = VK_PRESENT_MODE_FIFO_KHR; // always supported.
	uint32_t          mode_count = 0;
	VkPresentModeKHR *modes      = nullptr;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device.phys_device, device.surface, &mode_count, NULL);
	modes = (VkPresentModeKHR*)malloc(sizeof(VkPresentModeKHR) * mode_count);
	vkGetPhysicalDeviceSurfacePresentModesKHR(device.phys_device, device.surface, &mode_count, modes);

	for (uint32_t i = 0; i < mode_count; i++) {
		if (modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
			result = VK_PRESENT_MODE_MAILBOX_KHR;
			break;
		}
	}

	free(modes);
	return result;
}

///////////////////////////////////////////

int32_t skr_init(const char *app_name, void *app_hwnd, void *adapter_id) {
	VkResult result = VK_ERROR_INITIALIZATION_FAILED;

	if (!vk_create_instance (app_name, &vk_inst)) return -1;
	if (!vk_create_device   (vk_inst, app_hwnd, &skr_device)) return -2;

	VkCommandPoolCreateInfo cmd_pool_info = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	cmd_pool_info.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	cmd_pool_info.queueFamilyIndex = skr_device.queue_gfx_index;
	vkCreateCommandPool(skr_device.device, &cmd_pool_info, 0, &vk_cmd_pool);

	// A command pool for short-lived command buffers
	cmd_pool_info = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	cmd_pool_info.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
	cmd_pool_info.queueFamilyIndex = skr_device.queue_gfx_index;
	vkCreateCommandPool(skr_device.device, &cmd_pool_info, 0, &vk_cmd_pool_transient);

	return 1;
}

///////////////////////////////////////////

void skr_shutdown() {
	vkDeviceWaitIdle(skr_device.device);
	vkDestroyCommandPool(skr_device.device, vk_cmd_pool_transient, 0);
	vkDestroyCommandPool(skr_device.device, vk_cmd_pool, 0);
	vkDestroySurfaceKHR(vk_inst, skr_device.surface, 0);
	vkDestroyDevice(skr_device.device, 0);
	vkDestroyInstance(vk_inst, 0);
}

///////////////////////////////////////////

void skr_draw_begin() {
}

///////////////////////////////////////////

void skr_set_render_target(float clear_color[4], const skr_tex_t *render_target, const skr_tex_t *depth_target) {
	/*VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width  = (float) render_target->width;
	viewport.height = (float) render_target->height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset = {0, 0};
	scissor.extent.width  = render_target->width;
	scissor.extent.height = render_target->height;

	VkPipelineViewportStateCreateInfo viewport_state{};
	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.viewportCount = 1;
	viewport_state.pViewports = &viewport;
	viewport_state.scissorCount = 1;
	viewport_state.pScissors = &scissor;

	skr_draw_hack();*/
	skr_active_rendertarget = render_target;

	VkCommandBufferBeginInfo beginInfo = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	beginInfo.flags           = 0; // Optional
	beginInfo.pInheritanceInfo = nullptr; // Optional

	if (vkBeginCommandBuffer(render_target->rt_commandbuffer, &beginInfo) != VK_SUCCESS) {
		printf("failed to begin recording command buffer!\n");
	}

	VkClearValue clear_values = {};
	memcpy(&clear_values.color, clear_color, sizeof(float) * 4);

	VkRenderPassBeginInfo renderPassInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
	renderPassInfo.renderPass  = vk_renderpass_cache[render_target->rt_renderpass].renderpass;
	renderPassInfo.framebuffer = render_target->rt_framebuffer;
	renderPassInfo.renderArea.offset = {0, 0};
	renderPassInfo.renderArea.extent.width  = render_target->width;
	renderPassInfo.renderArea.extent.height = render_target->height;
	renderPassInfo.clearValueCount = 1;
	renderPassInfo.pClearValues    = &clear_values;

	vkCmdBeginRenderPass(render_target->rt_commandbuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
}

///////////////////////////////////////////

void skr_draw (int32_t index_start, int32_t index_count, int32_t instance_count) {
	vkCmdDrawIndexed(skr_active_rendertarget->rt_commandbuffer, index_count, instance_count, index_start, 0, 0);
}

///////////////////////////////////////////
// Buffer                                //
///////////////////////////////////////////

int32_t vk_find_mem_type(uint32_t type_filter, VkMemoryPropertyFlags properties) {
	VkPhysicalDeviceMemoryProperties props;
	vkGetPhysicalDeviceMemoryProperties(skr_device.phys_device, &props);
	for (uint32_t i = 0; i < props.memoryTypeCount; i++) {
		if ((type_filter & (1 << i)) && (props.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}
	return -1;
}

void vk_create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer *out_buffer, VkDeviceMemory *out_memory) {
	VkBufferCreateInfo buff_info = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
	buff_info.size        = size;
	buff_info.usage       = usage;
	buff_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateBuffer(skr_device.device, &buff_info, nullptr, out_buffer) != VK_SUCCESS) {
		printf("failed to create buffer!");
	}

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(skr_device.device, *out_buffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
	allocInfo.allocationSize  = memRequirements.size;
	allocInfo.memoryTypeIndex = vk_find_mem_type(memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(skr_device.device, &allocInfo, nullptr, out_memory) != VK_SUCCESS) {
		printf("failed to allocate buffer memory!");
	}

	vkBindBufferMemory(skr_device.device, *out_buffer, *out_memory, 0);
}

void vk_copy_buffer(VkBuffer src, VkBuffer dest, VkDeviceSize size) {
	VkCommandBufferAllocateInfo allocInfo = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
	allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandPool        = vk_cmd_pool_transient;
	allocInfo.commandBufferCount = 1;

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(skr_device.device, &allocInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	VkBufferCopy copyRegion = {};
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, src, dest, 1, &copyRegion);

	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit  (skr_device.queue_gfx, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(skr_device.queue_gfx);
	vkFreeCommandBuffers(skr_device.device, vk_cmd_pool_transient, 1, &commandBuffer);
}

skr_buffer_t skr_buffer_create(const void *data, uint32_t size_bytes, skr_buffer_type_ type, skr_use_ use) {
	skr_buffer_t result = {};
	result.type = type;
	result.use  = use;

	VkBufferUsageFlags usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
	switch (type) {
	case skr_buffer_type_vertex:   usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;  break;
	case skr_buffer_type_index:    usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;   break;
	case skr_buffer_type_constant: usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT; break;
	}

	VkBuffer       stage_buffer;
	VkDeviceMemory stage_memory;
	vk_create_buffer(size_bytes, 
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&stage_buffer, &stage_memory);

	void* gpu_data;
	vkMapMemory(skr_device.device, stage_memory, 0, size_bytes, 0, &gpu_data);
	memcpy(gpu_data, data, (size_t)size_bytes);
	vkUnmapMemory(skr_device.device, stage_memory);

	vk_create_buffer(size_bytes, 
		VK_BUFFER_USAGE_TRANSFER_DST_BIT |
		usage,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&result.buffer, &result.memory);

	vk_copy_buffer(stage_buffer, result.buffer, size_bytes);

	vkDestroyBuffer(skr_device.device, stage_buffer, nullptr);
	vkFreeMemory   (skr_device.device, stage_memory, nullptr);

	return result;
}

///////////////////////////////////////////

void skr_buffer_update(skr_buffer_t *buffer, const void *data, uint32_t size_bytes) {
}

///////////////////////////////////////////

void skr_buffer_set(const skr_buffer_t *buffer, uint32_t slot, uint32_t stride, uint32_t offset) {
	vkCmdBindPipeline(skr_active_rendertarget->rt_commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *vk_active_pipeline);

	
	switch (buffer->type) {
	case skr_buffer_type_vertex: {
		VkBuffer     buffers[] = {buffer->buffer};
		VkDeviceSize offsets[] = {offset};
		vkCmdBindVertexBuffers(skr_active_rendertarget->rt_commandbuffer, 0, 1, &buffer->buffer, offsets);
	} break;
	case skr_buffer_type_index : vkCmdBindIndexBuffer  (skr_active_rendertarget->rt_commandbuffer, buffer->buffer, offset, VK_INDEX_TYPE_UINT32); break;
	//case skr_buffer_type_vertex: vkCmdBindVertexBuffers(skr_active_rendertarget->rt_commandbuffer, 0, 1, &buffer->buffer, offsets); break;
	};
}

///////////////////////////////////////////

void skr_buffer_destroy(skr_buffer_t *buffer) {
	vkDestroyBuffer(skr_device.device, buffer->buffer, nullptr);
	*buffer = {};
}

///////////////////////////////////////////
// Mesh                                  //
///////////////////////////////////////////

skr_mesh_t skr_mesh_create(const skr_buffer_t *vert_buffer, const skr_buffer_t *ind_buffer) {
	skr_mesh_t result = {};
	return result;
}
void skr_mesh_set(const skr_mesh_t *mesh) {
}
void skr_mesh_destroy(skr_mesh_t *mesh) {
}

///////////////////////////////////////////
// Shader                                //
///////////////////////////////////////////

skr_shader_t skr_shader_create(const uint8_t *shader_data, size_t shader_size, skr_shader_ type) {
	skr_shader_t result = {};
	result.type = type;

	VkShaderModuleCreateInfo shader_info = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
	shader_info.codeSize = shader_size;
	shader_info.pCode    = (uint32_t*)shader_data;

	if (vkCreateShaderModule(skr_device.device, &shader_info, nullptr, &result.module) != VK_SUCCESS) {
		printf("Failed to create shader module!");
	}

	return result;
}
void skr_shader_destroy(skr_shader_t *shader) {
	vkDestroyShaderModule(skr_device.device, shader->module, nullptr);
	*shader = {};
}

///////////////////////////////////////////
// Shader Program                        //
///////////////////////////////////////////

skr_shader_program_t skr_shader_program_create(const skr_shader_t *vertex, const skr_shader_t *pixel) {
	skr_shader_program_t result = {};

	vk_pipeline_info_t info = {};

	info.vertex_info = skr_vertex_layout;

	info.shader_stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	info.shader_stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
	info.shader_stages[0].module = vertex->module;
	info.shader_stages[0].pName  = "vs";
	info.shader_stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	info.shader_stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
	info.shader_stages[1].module = pixel->module;
	info.shader_stages[1].pName  = "ps";

	info.input_asm.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	info.input_asm.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	info.input_asm.primitiveRestartEnable = VK_FALSE;

	info.rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	info.rasterizer.depthClampEnable        = VK_FALSE;
	info.rasterizer.rasterizerDiscardEnable = VK_FALSE;
	info.rasterizer.polygonMode             = VK_POLYGON_MODE_FILL;
	info.rasterizer.lineWidth               = 1.0f;
	info.rasterizer.cullMode                = VK_CULL_MODE_BACK_BIT;
	info.rasterizer.frontFace               = VK_FRONT_FACE_CLOCKWISE;
	info.rasterizer.depthBiasEnable         = VK_FALSE;
	info.rasterizer.depthBiasConstantFactor = 0.0f; // Optional
	info.rasterizer.depthBiasClamp          = 0.0f; // Optional
	info.rasterizer.depthBiasSlopeFactor    = 0.0f; // Optional

	info.multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	info.multisample.sampleShadingEnable   = VK_FALSE;
	info.multisample.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT;
	info.multisample.minSampleShading      = 1.0f; // Optional
	info.multisample.pSampleMask           = nullptr; // Optional
	info.multisample.alphaToCoverageEnable = VK_FALSE; // Optional
	info.multisample.alphaToOneEnable      = VK_FALSE; // Optional

	info.depth_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	// Ugh, stuff here, look it up later

	info.blend_attch.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	info.blend_attch.blendEnable         = VK_FALSE;
	info.blend_attch.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
	info.blend_attch.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
	info.blend_attch.colorBlendOp        = VK_BLEND_OP_ADD; // Optional
	info.blend_attch.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
	info.blend_attch.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
	info.blend_attch.alphaBlendOp        = VK_BLEND_OP_ADD; // Optional

	info.blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	info.blend_info.logicOpEnable     = VK_FALSE;
	info.blend_info.logicOp           = VK_LOGIC_OP_COPY; // Optional
	info.blend_info.attachmentCount   = 1;
	info.blend_info.pAttachments      = &info.blend_attch;
	info.blend_info.blendConstants[0] = 0.0f; // Optional
	info.blend_info.blendConstants[1] = 0.0f; // Optional
	info.blend_info.blendConstants[2] = 0.0f; // Optional
	info.blend_info.blendConstants[3] = 0.0f; // Optional

	info.dynamic_states[0] = VK_DYNAMIC_STATE_VIEWPORT;

	info.dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	info.dynamic_state.dynamicStateCount = 0;// 1;
	info.dynamic_state.pDynamicStates    = info.dynamic_states;

	VkPipelineLayoutCreateInfo pipe_layout = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
	pipe_layout.setLayoutCount         = 0; // Optional
	pipe_layout.pSetLayouts            = nullptr; // Optional
	pipe_layout.pushConstantRangeCount = 0; // Optional
	pipe_layout.pPushConstantRanges    = nullptr; // Optional

	if (vkCreatePipelineLayout(skr_device.device, &pipe_layout, nullptr, &result.pipeline_layout) != VK_SUCCESS) {
		printf("failed to create pipeline layout!\n");
	}

	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width  = 100;
	viewport.height = 100;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset = {0, 0};
	scissor.extent.width  = 100;
	scissor.extent.height = 100;

	VkPipelineViewportStateCreateInfo viewport_state{};
	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.viewportCount = 1;
	viewport_state.pViewports = &viewport;
	viewport_state.scissorCount = 1;
	viewport_state.pScissors = &scissor;

	info.create.sType = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
	info.create.stageCount          = 2;
	info.create.pStages             = info.shader_stages;
	info.create.pVertexInputState   = &info.vertex_info;
	info.create.pInputAssemblyState = &info.input_asm;
	info.create.pViewportState      = &viewport_state;
	info.create.pRasterizationState = &info.rasterizer;
	info.create.pMultisampleState   = &info.multisample;
	info.create.pDepthStencilState  = &info.depth_info;
	info.create.pColorBlendState    = &info.blend_info;
	info.create.pDynamicState       = &info.dynamic_state;
	info.create.layout              = result.pipeline_layout;
	info.create.subpass             = 0;
	info.create.basePipelineHandle  = VK_NULL_HANDLE; // Optional
	info.create.basePipelineIndex   = -1; // Optional

	result.pipeline = vk_pipeline_ref(info);

	return result;
}
void skr_shader_program_set(const skr_shader_program_t *program) {
	vk_active_pipeline = &vk_pipeline_cache[program->pipeline].pipelines[skr_active_rendertarget->rt_renderpass];
	vkCmdBindPipeline(skr_active_rendertarget->rt_commandbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, *vk_active_pipeline);
}
void skr_shader_program_destroy(skr_shader_program_t *program) {
	if (program->pipeline)        vk_pipeline_release(program->pipeline);
	if (program->pipeline_layout) vkDestroyPipelineLayout(skr_device.device, program->pipeline_layout, nullptr);
	*program = {};
}

///////////////////////////////////////////
// Swapchain                             //
///////////////////////////////////////////

skr_swapchain_t skr_swapchain_create(skr_tex_fmt_ format, skr_tex_fmt_ depth_format, int32_t width, int32_t height) {
	skr_swapchain_t  result = {};
	VkPresentModeKHR mode   = vk_get_presentation_mode(skr_device);
	result.format           = vk_get_preferred_fmt    (skr_device);

	VkSurfaceCapabilitiesKHR surface_caps;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(skr_device.phys_device, skr_device.surface, &surface_caps);

	result.extents = surface_caps.currentExtent;
	if (result.extents.width == UINT32_MAX) {
		if (width < surface_caps.minImageExtent.width)
			width = surface_caps.minImageExtent.width;
		if (width > surface_caps.maxImageExtent.width)
			width = surface_caps.maxImageExtent.width;
		result.extents.width = width;
		
		if (height < surface_caps.minImageExtent.height)
			height = surface_caps.minImageExtent.height;
		if (height > surface_caps.maxImageExtent.height)
			height = surface_caps.maxImageExtent.height;
		result.extents.height = height;
	}
	result.width  = width;
	result.height = height;
	
	VkSwapchainCreateInfoKHR swapchain_info = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
	swapchain_info.surface          = skr_device.surface;
	swapchain_info.minImageCount    = surface_caps.maxImageCount > 0 && surface_caps.minImageCount+1 > surface_caps.maxImageCount 
		? surface_caps.minImageCount
		: surface_caps.minImageCount + 1;
	swapchain_info.imageFormat      = result.format.format;
	swapchain_info.imageColorSpace  = result.format.colorSpace;
	swapchain_info.imageExtent      = result.extents;
	swapchain_info.imageArrayLayers = 1; // 2 for stereo;
	swapchain_info.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapchain_info.preTransform     = surface_caps.currentTransform;
	swapchain_info.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchain_info.presentMode      = mode;
	swapchain_info.clipped          = VK_TRUE;

	// Exclusive mode is faster, but can't be used if the presentation queue
	// and graphics queue are separate.
	if (skr_device.queue_gfx_index != skr_device.queue_present_index) {
		uint32_t queue_indices[] = { skr_device.queue_gfx_index, skr_device.queue_present_index };
		swapchain_info.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
		swapchain_info.queueFamilyIndexCount = _countof(queue_indices);
		swapchain_info.pQueueFamilyIndices   = queue_indices;
	}

	VkResult call_result = vkCreateSwapchainKHR(skr_device.device, &swapchain_info, 0, &result.swapchain);
	if (call_result != VK_SUCCESS)
		printf("Failed to create swapchain!");

	vkGetSwapchainImagesKHR(skr_device.device, result.swapchain, &result.img_count, nullptr);
	result.imgs      = (VkImage   *)malloc(sizeof(VkImage  ) * result.img_count);
	result.textures  = (skr_tex_t *)malloc(sizeof(skr_tex_t) * result.img_count);
	result.img_fence = (VkFence   *)malloc(sizeof(VkFence  ) * result.img_count);
	vkGetSwapchainImagesKHR(skr_device.device, result.swapchain, &result.img_count, result.imgs);

	VkFenceCreateInfo fence_info = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
	fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	for (uint32_t i = 0; i < result.img_count; i++) {
		result.textures[i] = skr_tex_from_native(&result.imgs[i], skr_tex_type_rendertarget, skr_native_to_tex_fmt(result.format.format), width, height);
		vkCreateFence(skr_device.device, &fence_info, nullptr, &result.img_fence[i]);
	}

	// Create synchronization objects
	VkSemaphoreCreateInfo sem_info = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
	for (size_t i = 0; i < 2; i++) {
		if (vkCreateSemaphore(skr_device.device, &sem_info,   nullptr, &result.sem_available[i]) != VK_SUCCESS ||
			vkCreateSemaphore(skr_device.device, &sem_info,   nullptr, &result.sem_finished [i]) != VK_SUCCESS ||
			vkCreateFence    (skr_device.device, &fence_info, nullptr, &result.fence_flight [i]) != VK_SUCCESS) {

			printf("failed to create synchronization objects for a frame!\n");
		}
	}

	return result;
}
void skr_swapchain_resize(skr_swapchain_t *swapchain, int32_t width, int32_t height) {}
void skr_swapchain_present(skr_swapchain_t *swapchain) {
	vkCmdEndRenderPass(skr_active_rendertarget->rt_commandbuffer);
	vkEndCommandBuffer(skr_active_rendertarget->rt_commandbuffer);

	VkSubmitInfo         submitInfo       = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
	VkSemaphore          waitSemaphores[] = {swapchain->sem_available[swapchain->sync_index]};
	VkPipelineStageFlags waitStages[]     = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores    = waitSemaphores;
	submitInfo.pWaitDstStageMask  = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers    = &skr_active_rendertarget->rt_commandbuffer;

	VkSemaphore signalSemaphores[] = {swapchain->sem_finished[swapchain->sync_index]};
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores    = signalSemaphores;

	vkResetFences(skr_device.device, 1, &swapchain->fence_flight[swapchain->sync_index]);
	if (vkQueueSubmit(skr_device.queue_gfx, 1, &submitInfo, swapchain->fence_flight[swapchain->sync_index]) != VK_SUCCESS) {
		printf("failed to submit draw command buffer!");
	}

	VkSwapchainKHR   swapChains[] = {swapchain->swapchain};
	VkPresentInfoKHR presentInfo  = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores    = signalSemaphores;
	presentInfo.swapchainCount     = 1;
	presentInfo.pSwapchains        = swapChains;
	presentInfo.pImageIndices      = &swapchain->img_active;
	presentInfo.pResults           = nullptr; // Optional

	vkQueuePresentKHR(skr_device.queue_present, &presentInfo);

	swapchain->sync_index = (swapchain->sync_index + 1) % 2;
}
const skr_tex_t *skr_swapchain_get_target(const skr_swapchain_t *swapchain) {
	return nullptr;
}
const skr_tex_t *skr_swapchain_get_depth(const skr_swapchain_t *swapchain) {
	return nullptr;
}

void skr_swapchain_get_next(skr_swapchain_t *swapchain, const skr_tex_t **out_target, const skr_tex_t **out_depth) {
	vkWaitForFences(skr_device.device, 1, &swapchain->fence_flight[swapchain->sync_index], VK_TRUE, UINT64_MAX);
	vkAcquireNextImageKHR(skr_device.device, swapchain->swapchain, UINT64_MAX, swapchain->sem_available[swapchain->sync_index], VK_NULL_HANDLE, &swapchain->img_active);
	*out_target = &swapchain->textures[swapchain->img_active];
	*out_depth  = nullptr;

	if (swapchain->img_fence[swapchain->img_active] != VK_NULL_HANDLE) {
		vkWaitForFences(skr_device.device, 1, &swapchain->img_fence[swapchain->img_active], VK_TRUE, UINT64_MAX);
	}
	swapchain->img_fence[swapchain->img_active] = swapchain->fence_flight[swapchain->sync_index];
}

void skr_swapchain_destroy(skr_swapchain_t *swapchain) {
	for (size_t i = 0; i < 2; i++) {
		vkDestroySemaphore(skr_device.device, swapchain->sem_finished [i], nullptr);
		vkDestroySemaphore(skr_device.device, swapchain->sem_available[i], nullptr);
		vkDestroyFence    (skr_device.device, swapchain->fence_flight [i], nullptr);
	}

	for (uint32_t i = 0; i < swapchain->img_count; i++) {
		swapchain->textures[i].texture = nullptr;
		skr_tex_destroy(&swapchain->textures[i]);
		vkDestroyFence(skr_device.device, swapchain->img_fence[i], nullptr);
	}
	vkDestroySwapchainKHR(skr_device.device, swapchain->swapchain, 0);
	free(swapchain->imgs);
	free(swapchain->textures);
	free(swapchain->img_fence);
}

///////////////////////////////////////////
// Texture                               //
///////////////////////////////////////////

void skr_tex_create_views(skr_tex_t *tex) {
	VkImageViewCreateInfo view_info = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	view_info.image    = tex->texture;
	view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view_info.format   = (VkFormat)skr_tex_fmt_to_native(tex->format);
	view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

	view_info.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
	view_info.subresourceRange.baseMipLevel   = 0;
	view_info.subresourceRange.levelCount     = 1;
	view_info.subresourceRange.baseArrayLayer = 0;
	view_info.subresourceRange.layerCount     = 1;

	if (vkCreateImageView(skr_device.device, &view_info, nullptr, &tex->view) != VK_SUCCESS)
		printf("vkCreateImageView failed");

	if (tex->type == skr_tex_type_rendertarget) {
		VkAttachmentDescription color_attch = {};
		color_attch.format         = view_info.format;
		color_attch.samples        = VK_SAMPLE_COUNT_1_BIT;
		color_attch.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
		color_attch.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
		color_attch.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		color_attch.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		color_attch.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
		color_attch.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference color_attch_ref = {};
		color_attch_ref.attachment = 0;
		color_attch_ref.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments    = &color_attch_ref;

		VkSubpassDependency dependency = {};
		dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass    = 0;
		dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		VkRenderPassCreateInfo pass_info = {VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
		pass_info.attachmentCount = 1;
		pass_info.pAttachments    = &color_attch;
		pass_info.subpassCount    = 1;
		pass_info.pSubpasses      = &subpass;
		pass_info.dependencyCount = 1;
		pass_info.pDependencies   = &dependency;

		tex->rt_renderpass = vk_renderpass_ref(pass_info);

		VkImageView             attachments[]   = { tex->view };
		VkFramebufferCreateInfo framebuffer_info = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
		framebuffer_info.renderPass      = vk_renderpass_cache[tex->rt_renderpass].renderpass;
		framebuffer_info.attachmentCount = _countof(attachments);
		framebuffer_info.pAttachments    = attachments;
		framebuffer_info.width           = tex->width;
		framebuffer_info.height          = tex->height;
		framebuffer_info.layers          = 1;

		if (vkCreateFramebuffer(skr_device.device, &framebuffer_info, nullptr, &tex->rt_framebuffer) != VK_SUCCESS) {
			printf("failed to create framebuffer!\n");
		}

		VkCommandBufferAllocateInfo alloc_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
		alloc_info.commandPool        = vk_cmd_pool;
		alloc_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandBufferCount = 1;

		if (vkAllocateCommandBuffers(skr_device.device, &alloc_info, &tex->rt_commandbuffer) != VK_SUCCESS) {
			printf("failed to allocate command buffers!\n");
		}
	}
}

skr_tex_t skr_tex_from_native(void *native_tex, skr_tex_type_ type, skr_tex_fmt_ format, int32_t width, int32_t height) {
	skr_tex_t result = {};
	result.type    = type;
	result.texture = *(VkImage *)native_tex;
	result.format  = format;
	result.width   = width;
	result.height  = height;
	result.rt_renderpass = -1;

	skr_tex_create_views(&result);

	return result;
}
skr_tex_t            skr_tex_create(skr_tex_type_ type, skr_use_ use, skr_tex_fmt_ format, skr_mip_ mip_maps) {
	skr_tex_t result = {};
	result.rt_renderpass = -1;
	return result;
}
void                 skr_tex_settings(skr_tex_t *tex, skr_tex_address_ address, skr_tex_sample_ sample, int32_t anisotropy) {}
void                 skr_tex_set_data(skr_tex_t *tex, void **data_frames, int32_t data_frame_count, int32_t width, int32_t height) {}
void                 skr_tex_set_active(const skr_tex_t *tex, int32_t slot) {}
void skr_tex_destroy(skr_tex_t *tex) {
	if (tex->rt_framebuffer) vkDestroyFramebuffer(skr_device.device, tex->rt_framebuffer, nullptr);
	if (tex->rt_renderpass ) vk_renderpass_release(tex->rt_renderpass);
	
	if (tex->view)        vkDestroyImageView  (skr_device.device, tex->view,        nullptr);
	if (tex->texture)     vkDestroyImage      (skr_device.device, tex->texture,     nullptr);
	*tex = {};
}

///////////////////////////////////////////

int64_t skr_tex_fmt_to_native(skr_tex_fmt_ format) {
	switch (format) {
	case skr_tex_fmt_rgba32:        return VK_FORMAT_R8G8B8A8_SRGB;
	case skr_tex_fmt_rgba32_linear: return VK_FORMAT_R8G8B8A8_UNORM;
	case skr_tex_fmt_bgra32:        return VK_FORMAT_B8G8R8A8_SRGB;
	case skr_tex_fmt_bgra32_linear: return VK_FORMAT_B8G8R8A8_UNORM;
	case skr_tex_fmt_rgba64:        return VK_FORMAT_R16G16B16A16_UNORM;
	case skr_tex_fmt_rgba128:       return VK_FORMAT_R32G32B32A32_SFLOAT;
	case skr_tex_fmt_depth16:       return VK_FORMAT_D16_UNORM;
	case skr_tex_fmt_depth32:       return VK_FORMAT_D32_SFLOAT;
	case skr_tex_fmt_depthstencil:  return VK_FORMAT_D24_UNORM_S8_UINT;
	case skr_tex_fmt_r8:            return VK_FORMAT_R8_UNORM;
	case skr_tex_fmt_r16:           return VK_FORMAT_R16_UNORM;
	case skr_tex_fmt_r32:           return VK_FORMAT_R32_SFLOAT;
	default: return VK_FORMAT_UNDEFINED;
	}
}

///////////////////////////////////////////

skr_tex_fmt_ skr_native_to_tex_fmt(VkFormat format) {
	switch (format) {
	case VK_FORMAT_R8G8B8A8_SRGB:       return skr_tex_fmt_rgba32;
	case VK_FORMAT_R8G8B8A8_UNORM:      return skr_tex_fmt_rgba32_linear;
	case VK_FORMAT_B8G8R8A8_SRGB:       return skr_tex_fmt_bgra32;
	case VK_FORMAT_B8G8R8A8_UNORM:      return skr_tex_fmt_bgra32_linear;
	case VK_FORMAT_R16G16B16A16_UNORM:  return skr_tex_fmt_rgba64;
	case VK_FORMAT_R32G32B32A32_SFLOAT: return skr_tex_fmt_rgba128;
	case VK_FORMAT_D16_UNORM:           return skr_tex_fmt_depth16;
	case VK_FORMAT_D32_SFLOAT:          return skr_tex_fmt_depth32;
	case VK_FORMAT_D24_UNORM_S8_UINT:   return skr_tex_fmt_depthstencil;
	case VK_FORMAT_R8_UNORM:            return skr_tex_fmt_r8;
	case VK_FORMAT_R16_UNORM:           return skr_tex_fmt_r16;
	case VK_FORMAT_R32_SFLOAT:          return skr_tex_fmt_r32;
	default: return skr_tex_fmt_none;
	}
}

#endif