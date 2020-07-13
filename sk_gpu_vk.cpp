#ifdef SKR_VULKAN
#include "sk_gpu.h"

///////////////////////////////////////////

#pragma comment(lib,"vulkan-1.lib")
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include <stdio.h>

///////////////////////////////////////////

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

#define D3D_FRAME_COUNT 2

VkInstance         vk_inst         = VK_NULL_HANDLE;
VkCommandPool      vk_cmd_pool     = VK_NULL_HANDLE;
uint32_t           vk_frame_count = 0;

VkCommandBuffer    vk_cmd_buffers         [D3D_FRAME_COUNT];
VkFence            vk_frame_fences        [D3D_FRAME_COUNT];
VkSemaphore        vk_available_semaphores[D3D_FRAME_COUNT];
VkSemaphore        vk_finished_semaphores [D3D_FRAME_COUNT];

skr_device_t skr_device = {};
vk_swapchain_t skr_swapchain = {};

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
		VK_EXT_DEBUG_REPORT_EXTENSION_NAME };
	const char *layers[] = {
		"VK_LAYER_KHRONOS_validation"
	};
	VkInstanceCreateInfo create_info = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	create_info.pApplicationInfo        = &app_info;
	create_info.enabledExtensionCount   = _countof(ext);
	create_info.ppEnabledExtensionNames = ext;
	create_info.enabledLayerCount       = _countof(layers);
	create_info.ppEnabledLayerNames     = layers;

	return vkCreateInstance(&create_info, 0, out_inst) == VK_SUCCESS;
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

bool vk_create_swapchain(skr_device_t &device, int32_t app_width, int32_t app_height, vk_swapchain_t *out_swapchain) {
	*out_swapchain = {};

	out_swapchain->format = vk_get_preferred_fmt(device);
	VkPresentModeKHR mode = vk_get_presentation_mode(skr_device);
	out_swapchain->img_count = mode == VK_PRESENT_MODE_MAILBOX_KHR ? 3 : 2;

	VkSurfaceCapabilitiesKHR surface_caps;
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.phys_device, device.surface, &surface_caps);

	out_swapchain->extents = surface_caps.currentExtent;
	if (out_swapchain->extents.width == UINT32_MAX) {
		out_swapchain->extents.width = app_width;
		if (out_swapchain->extents.width < surface_caps.minImageExtent.width)
			out_swapchain->extents.width = surface_caps.minImageExtent.width;
		if (out_swapchain->extents.width > surface_caps.maxImageExtent.width)
			out_swapchain->extents.width = surface_caps.maxImageExtent.width;

		out_swapchain->extents.height = app_height;
		if (out_swapchain->extents.height< surface_caps.minImageExtent.height)
			out_swapchain->extents.height= surface_caps.minImageExtent.height;
		if (out_swapchain->extents.height> surface_caps.maxImageExtent.height)
			out_swapchain->extents.height= surface_caps.maxImageExtent.height;
	}

	VkSwapchainCreateInfoKHR swapchain_info = { VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR };
	swapchain_info.surface          = device.surface;
	swapchain_info.minImageCount    = out_swapchain->img_count;
	swapchain_info.imageFormat      = out_swapchain->format.format;
	swapchain_info.imageColorSpace  = out_swapchain->format.colorSpace;
	swapchain_info.imageExtent      = out_swapchain->extents;
	swapchain_info.imageArrayLayers = 1; // 2 for stere0;
	swapchain_info.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	swapchain_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapchain_info.preTransform     = surface_caps.currentTransform;
	swapchain_info.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchain_info.presentMode      = mode;
	swapchain_info.clipped          = VK_TRUE;

	if (vkCreateSwapchainKHR(device.device, &swapchain_info, 0, &out_swapchain->swapchain) != VK_SUCCESS)
		return false;

	vkGetSwapchainImagesKHR(device.device, out_swapchain->swapchain, &out_swapchain->img_count, NULL);
	vkGetSwapchainImagesKHR(device.device, out_swapchain->swapchain, &out_swapchain->img_count, out_swapchain->imgs);

	return true;
}

///////////////////////////////////////////

int32_t skr_init(const char *app_name, void *app_hwnd, void *adapter_id) {
	VkResult result = VK_ERROR_INITIALIZATION_FAILED;

	if (!vk_create_instance (app_name, &vk_inst)) return -1;
	if (!vk_create_device   (vk_inst, app_hwnd, &skr_device)) return -2;
	if (!vk_create_swapchain(skr_device, 1280,720, &skr_swapchain)) return -2;
	//skr_swapchain = skr_swapchain_create(skr_tex_fmt_rgba32_linear, skr_tex_fmt_depthstencil, 1280, 720);

	// Initialize the renderer
	VkCommandPoolCreateInfo cmd_pool_info = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	cmd_pool_info.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	cmd_pool_info.queueFamilyIndex = skr_device.queue_gfx_index;
	vkCreateCommandPool(skr_device.device, &cmd_pool_info, 0, &vk_cmd_pool);

	VkCommandBufferAllocateInfo cmd_buffer_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
	cmd_buffer_info.commandPool        = vk_cmd_pool;
	cmd_buffer_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmd_buffer_info.commandBufferCount = D3D_FRAME_COUNT;
	vkAllocateCommandBuffers(skr_device.device, &cmd_buffer_info, vk_cmd_buffers);

	VkSemaphoreCreateInfo semaphore_info = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	vkCreateSemaphore(skr_device.device, &semaphore_info, 0, &vk_available_semaphores[0]);
	vkCreateSemaphore(skr_device.device, &semaphore_info, 0, &vk_available_semaphores[1]);
	vkCreateSemaphore(skr_device.device, &semaphore_info, 0, &vk_finished_semaphores [0]);
	vkCreateSemaphore(skr_device.device, &semaphore_info, 0, &vk_finished_semaphores [1]);

	VkFenceCreateInfo fence_info = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	vkCreateFence(skr_device.device, &fence_info, 0, &vk_frame_fences[0]);
	vkCreateFence(skr_device.device, &fence_info, 0, &vk_frame_fences[1]);

	return result == VK_SUCCESS ? 1 : -4;
}

///////////////////////////////////////////

void skr_shutdown() {
	vkDeviceWaitIdle(skr_device.device);
	vkDestroyFence(skr_device.device, vk_frame_fences[0], 0);
	vkDestroyFence(skr_device.device, vk_frame_fences[1], 0);
	vkDestroySemaphore(skr_device.device, vk_finished_semaphores[0], 0);
	vkDestroySemaphore(skr_device.device, vk_finished_semaphores[1], 0);
	vkDestroySemaphore(skr_device.device, vk_available_semaphores[0], 0);
	vkDestroySemaphore(skr_device.device, vk_available_semaphores[1], 0);
	vkDestroyCommandPool(skr_device.device, vk_cmd_pool, 0);
	vkDestroySurfaceKHR(vk_inst, skr_device.surface, 0);
	vkDestroyDevice(skr_device.device, 0);
	vkDestroyInstance(vk_inst, 0);
}

///////////////////////////////////////////

void skr_draw_begin() {
	/*uint32_t index = (vk_frame_count++) % D3D_FRAME_COUNT;
	vkWaitForFences(skr_device.device, 1, &vk_frame_fences[index], VK_TRUE, UINT64_MAX);
	vkResetFences  (skr_device.device, 1, &vk_frame_fences[index]);*/
}

///////////////////////////////////////////

void skr_draw_hack() {
	uint32_t index = (vk_frame_count++) % D3D_FRAME_COUNT;
	vkWaitForFences(skr_device.device, 1, &vk_frame_fences[index], VK_TRUE, UINT64_MAX);
	vkResetFences  (skr_device.device, 1, &vk_frame_fences[index]);

	uint32_t image_index;
	vkAcquireNextImageKHR(skr_device.device, skr_swapchain.swapchain, UINT64_MAX, vk_available_semaphores[index], VK_NULL_HANDLE, &image_index);

	VkCommandBufferBeginInfo begin_info = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(vk_cmd_buffers[index], &begin_info);

	VkImageSubresourceRange resource_range = {};
	resource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	resource_range.levelCount = VK_REMAINING_MIP_LEVELS;
	resource_range.layerCount = VK_REMAINING_ARRAY_LAYERS;

	// Change layout of image to be optimal for clearing
	VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	barrier.srcAccessMask       = 0;
	barrier.dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
	barrier.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.srcQueueFamilyIndex = skr_device.queue_gfx_index;
	barrier.dstQueueFamilyIndex = skr_device.queue_gfx_index;
	barrier.image               = skr_swapchain.imgs[image_index];
	barrier.subresourceRange    = resource_range,
		vkCmdPipelineBarrier(vk_cmd_buffers[index], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

	VkClearColorValue color = { 1.0f, 0, 0, 1.0f };
	vkCmdClearColorImage(vk_cmd_buffers[index], skr_swapchain.imgs[image_index], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &color, 1, &resource_range);

	// Change layout of image to be optimal for presenting
	barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	barrier.srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask       = VK_ACCESS_MEMORY_READ_BIT;
	barrier.oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout           = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	barrier.srcQueueFamilyIndex = skr_device.queue_gfx_index;
	barrier.dstQueueFamilyIndex = skr_device.queue_gfx_index;
	barrier.image               = skr_swapchain.imgs[image_index];
	barrier.subresourceRange    = resource_range;
	vkCmdPipelineBarrier(vk_cmd_buffers[index], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

	vkEndCommandBuffer(vk_cmd_buffers[index]);

	VkPipelineStageFlags flags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo submit_info = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submit_info.waitSemaphoreCount   = 1;
	submit_info.pWaitSemaphores      = &vk_available_semaphores[index];
	submit_info.pWaitDstStageMask    = &flags;
	submit_info.commandBufferCount   = 1;
	submit_info.pCommandBuffers      = &vk_cmd_buffers[index];
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores    = &vk_finished_semaphores[index];
	vkQueueSubmit(skr_device.queue_gfx, 1, &submit_info, vk_frame_fences[index]);

	VkPresentInfoKHR present_info = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores    = &vk_finished_semaphores[index];
	present_info.swapchainCount     = 1;
	present_info.pSwapchains        = &skr_swapchain.swapchain;
	present_info.pImageIndices      = &image_index;
	vkQueuePresentKHR(skr_device.queue_gfx, &present_info);
}

///////////////////////////////////////////

void skr_set_render_target(float clear_color[4], const skr_tex_t *render_target, const skr_tex_t *depth_target) {
	skr_draw_hack();
}

///////////////////////////////////////////

void skr_draw (int32_t index_start, int32_t index_count, int32_t instance_count) {
}

///////////////////////////////////////////
// Buffer                                //
///////////////////////////////////////////

skr_buffer_t skr_buffer_create(const void *data, uint32_t size_bytes, skr_buffer_type_ type, skr_use_ use) {
	skr_buffer_t result = {};
	return result;
}

///////////////////////////////////////////

void skr_buffer_update(skr_buffer_t *buffer, const void *data, uint32_t size_bytes) {
}

///////////////////////////////////////////

void skr_buffer_set(const skr_buffer_t *buffer, uint32_t slot, uint32_t stride, uint32_t offset) {
}

///////////////////////////////////////////

void skr_buffer_destroy(skr_buffer_t *buffer) {
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

skr_shader_t         skr_shader_create(const char *file_data, skr_shader_ type) {
	skr_shader_t result = {};
	return result;
}
void                 skr_shader_destroy(skr_shader_t *shader) {
}

///////////////////////////////////////////
// Shader Program                        //
///////////////////////////////////////////

skr_shader_program_t skr_shader_program_create(const skr_shader_t *vertex, const skr_shader_t *pixel) {
	return {};
}
void                 skr_shader_program_set(const skr_shader_program_t *program) {}
void                 skr_shader_program_destroy(skr_shader_program_t *program) {}

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

	if (vkCreateSwapchainKHR(skr_device.device, &swapchain_info, 0, &result.swapchain) != VK_SUCCESS)
		printf("Failed to create swapchain!");

	vkGetSwapchainImagesKHR(skr_device.device, result.swapchain, &result.img_count, nullptr);
	result.imgs     = (VkImage   *)malloc(sizeof(VkImage  ) * result.img_count);
	result.textures = (skr_tex_t *)malloc(sizeof(skr_tex_t) * result.img_count);
	//result.fence = (VkFence *)malloc(sizeof(VkFence) * result.img_count);
	vkGetSwapchainImagesKHR(skr_device.device, result.swapchain, &result.img_count, result.imgs);

	for (uint32_t i = 0; i < result.img_count; i++) {
		result.textures[i] = skr_tex_from_native(&result.imgs[i], skr_tex_type_rendertarget, skr_native_to_tex_fmt(result.format.format));
	}

	/*VkSemaphoreCreateInfo semaphore_info = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
	VkFenceCreateInfo     fence_info     = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
	fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	for (uint32_t i = 0; i < result.img_count; i++) {
		vkCreateSemaphore(skr_device.device, &semaphore_info, 0, &vk_available_semaphores[i]);
		vkCreateSemaphore(skr_device.device, &semaphore_info, 0, &vk_finished_semaphores [i]);
		vkCreateFence    (skr_device.device, &fence_info,     0, &vk_frame_fences        [i]);
	}*/

	return result;
}
void                 skr_swapchain_resize(skr_swapchain_t *swapchain, int32_t width, int32_t height) {}
void                 skr_swapchain_present(const skr_swapchain_t *swapchain) {
	/*VkImageSubresourceRange resource_range = {};
	resource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	resource_range.levelCount = VK_REMAINING_MIP_LEVELS;
	resource_range.layerCount = VK_REMAINING_ARRAY_LAYERS;

	// Change layout of image to be optimal for presenting
	VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	barrier.srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask       = VK_ACCESS_MEMORY_READ_BIT;
	barrier.oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout           = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	barrier.srcQueueFamilyIndex = skr_device.queue_gfx_index;
	barrier.dstQueueFamilyIndex = skr_device.queue_gfx_index;
	barrier.image               = swapchain->imgs[image_index];
	barrier.subresourceRange    = resource_range;
	vkCmdPipelineBarrier(vk_cmd_buffers[index], VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, NULL, 0, NULL, 1, &barrier);

	vkEndCommandBuffer(vk_cmd_buffers[index]);

	VkPipelineStageFlags flags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo submit_info = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submit_info.waitSemaphoreCount   = 1;
	submit_info.pWaitSemaphores      = &vk_available_semaphores[index];
	submit_info.pWaitDstStageMask    = &flags;
	submit_info.commandBufferCount   = 1;
	submit_info.pCommandBuffers      = &vk_cmd_buffers[index];
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores    = &vk_finished_semaphores[index];
	vkQueueSubmit(skr_device.queue_gfx, 1, &submit_info, vk_frame_fences[index]);

	VkPresentInfoKHR present_info = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores    = &vk_finished_semaphores[index];
	present_info.swapchainCount     = 1;
	present_info.pSwapchains        = &swapchain->swapchain;
	present_info.pImageIndices      = &image_index;
	vkQueuePresentKHR(skr_device.queue_gfx, &present_info);*/
}
const skr_tex_t *skr_swapchain_get_target(const skr_swapchain_t *swapchain) {
	return nullptr;
}
const skr_tex_t *skr_swapchain_get_depth(const skr_swapchain_t *swapchain) {
	return nullptr;
}

void skr_swapchain_get_next(const skr_swapchain_t *swapchain, const skr_tex_t **out_target, const skr_tex_t **out_depth) {
	/*uint32_t index = (vk_frame_count++) % D3D_FRAME_COUNT;
	vkWaitForFences(skr_device.device, 1, &vk_frame_fences[index], VK_TRUE, UINT64_MAX);
	vkResetFences  (skr_device.device, 1, &vk_frame_fences[index]);

	uint32_t image_index;
	vkAcquireNextImageKHR(skr_device.device, swapchain->swapchain, UINT64_MAX, vk_available_semaphores[index], VK_NULL_HANDLE, &image_index);
	*/
}

void skr_swapchain_destroy(skr_swapchain_t *swapchain) {
	for (uint32_t i = 0; i < swapchain->img_count; i++) {
		swapchain->textures[i].texture = nullptr;
		skr_tex_destroy(&swapchain->textures[i]);
	}
	vkDestroySwapchainKHR(skr_device.device, swapchain->swapchain, 0);
	free(swapchain->imgs);
	free(swapchain->textures);
}

///////////////////////////////////////////
// Texture                               //
///////////////////////////////////////////

skr_tex_t skr_tex_from_native(void *native_tex, skr_tex_type_ type, skr_tex_fmt_ format) {
	skr_tex_t result = {};
	result.type    = type;
	result.texture = *(VkImage *)native_tex;
	result.format  = format;

	VkImageViewCreateInfo view_info = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
	view_info.image    = result.texture;
	view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view_info.format   = (VkFormat)skr_tex_fmt_to_native(format);
	view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

	view_info.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
	view_info.subresourceRange.baseMipLevel   = 0;
	view_info.subresourceRange.levelCount     = 1;
	view_info.subresourceRange.baseArrayLayer = 0;
	view_info.subresourceRange.layerCount     = 1;

	if (vkCreateImageView(skr_device.device, &view_info, nullptr, &result.view) != VK_SUCCESS)
		printf("vkCreateImageView failed");

	return result;
}
skr_tex_t            skr_tex_create(skr_tex_type_ type, skr_use_ use, skr_tex_fmt_ format, skr_mip_ mip_maps) {
	return {};
}
void                 skr_tex_settings(skr_tex_t *tex, skr_tex_address_ address, skr_tex_sample_ sample, int32_t anisotropy) {}
void                 skr_tex_set_data(skr_tex_t *tex, void **data_frames, int32_t data_frame_count, int32_t width, int32_t height) {}
void                 skr_tex_set_active(const skr_tex_t *tex, int32_t slot) {}
void skr_tex_destroy(skr_tex_t *tex) {
	vkDestroyImageView(skr_device.device, tex->view,    nullptr);
	vkDestroyImage    (skr_device.device, tex->texture, nullptr);
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