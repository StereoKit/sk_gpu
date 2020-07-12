#ifdef SKR_VULKAN
#include "sk_gpu.h"

///////////////////////////////////////////

#pragma comment(lib,"vulkan-1.lib")
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

///////////////////////////////////////////

struct skr_device_t {
	VkSurfaceKHR     surface;
	VkPhysicalDevice phys_device;
	VkDevice         device;
	VkQueue          queue;
	uint32_t         queue_index;
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

skr_device_t   skr_device    = {};
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
	if (vkCreateWin32SurfaceKHR(inst, &surface_info, NULL, &out_device->surface) != VK_SUCCESS)
		return false;

	// Get physical device list
	uint32_t         device_count;
	VkPhysicalDevice device_handles[4];
	vkEnumeratePhysicalDevices(inst, &device_count, 0);
	device_count = device_count > _countof(device_handles) ? _countof(device_handles) : device_count;
	vkEnumeratePhysicalDevices(inst, &device_count, device_handles);

	// Pick a physical device that meets our requirements
	VkQueueFamilyProperties          queue_props[4];
	VkPhysicalDeviceProperties       device_props;
	VkPhysicalDeviceFeatures         device_features;
	VkPhysicalDeviceMemoryProperties device_mem_props;
	for (uint32_t i = 0; i < device_count; i++) {
		uint32_t queue_count = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device_handles[i], &queue_count, NULL);
		queue_count = queue_count > _countof(queue_props) ? _countof(queue_props) : queue_count;
		vkGetPhysicalDeviceQueueFamilyProperties(device_handles[i], &queue_count, queue_props);

		vkGetPhysicalDeviceProperties      (device_handles[i], &device_props);
		vkGetPhysicalDeviceFeatures        (device_handles[i], &device_features);
		vkGetPhysicalDeviceMemoryProperties(device_handles[i], &device_mem_props);
		for (uint32_t j = 0; j < queue_count; ++j) {
			VkBool32 supports_present = VK_FALSE;
			vkGetPhysicalDeviceSurfaceSupportKHR(device_handles[i], j, out_device->surface, &supports_present);

			if (supports_present && (queue_props[j].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
				out_device->queue_index = j;
				out_device->phys_device = device_handles[i];
				break;
			}
		}
		if (out_device->phys_device) break;
	}

	// Create a device from the physical device
	const float queue_priority = 1.0f;
	VkDeviceQueueCreateInfo device_queue_info = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
	device_queue_info.queueFamilyIndex = out_device->queue_index;
	device_queue_info.queueCount       = 1;
	device_queue_info.pQueuePriorities = &queue_priority;

	const char *enabled_exts[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
	VkDeviceCreateInfo device_create_info = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	device_create_info.queueCreateInfoCount    = 1;
	device_create_info.pQueueCreateInfos       = &device_queue_info;
	device_create_info.enabledExtensionCount   = _countof(enabled_exts);
	device_create_info.ppEnabledExtensionNames = enabled_exts;

	if (vkCreateDevice(out_device->phys_device, &device_create_info, NULL, &out_device->device) != VK_SUCCESS)
		return false;

	vkGetDeviceQueue(out_device->device, out_device->queue_index, 0, &out_device->queue);
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
	VkPresentModeKHR result;

	uint32_t mode_count = 0;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device.phys_device, device.surface, &mode_count, NULL);
	VkPresentModeKHR modes[4];
	mode_count = mode_count > _countof(modes) ? _countof(modes) : mode_count;
	vkGetPhysicalDeviceSurfacePresentModesKHR(device.phys_device, device.surface, &mode_count, modes);

	result = VK_PRESENT_MODE_FIFO_KHR;   // always supported.
	for (uint32_t i = 0; i < mode_count; i++) {
		if (modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
			result = VK_PRESENT_MODE_MAILBOX_KHR;
			break;
		}
	}

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
	if (!vk_create_swapchain(skr_device, 1280, 720, &skr_swapchain)) return -3;

	// Initialize the renderer
	VkCommandPoolCreateInfo cmd_pool_info = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	cmd_pool_info.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	cmd_pool_info.queueFamilyIndex = skr_device.queue_index;
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
	vkDestroySwapchainKHR(skr_device.device, skr_swapchain.swapchain, 0);
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
	barrier.srcQueueFamilyIndex = skr_device.queue_index;
	barrier.dstQueueFamilyIndex = skr_device.queue_index;
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
	barrier.srcQueueFamilyIndex = skr_device.queue_index;
	barrier.dstQueueFamilyIndex = skr_device.queue_index;
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
	vkQueueSubmit(skr_device.queue, 1, &submit_info, vk_frame_fences[index]);

	VkPresentInfoKHR present_info = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores    = &vk_finished_semaphores[index];
	present_info.swapchainCount     = 1;
	present_info.pSwapchains        = &skr_swapchain.swapchain;
	present_info.pImageIndices      = &image_index;
	vkQueuePresentKHR(skr_device.queue, &present_info);
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

skr_swapchain_t      skr_swapchain_create(skr_tex_fmt_ format, skr_tex_fmt_ depth_format, int32_t width, int32_t height) {
	return {};
}
void                 skr_swapchain_resize(skr_swapchain_t *swapchain, int32_t width, int32_t height) {}
void                 skr_swapchain_present(const skr_swapchain_t *swapchain) {}
const skr_tex_t *skr_swapchain_get_target(const skr_swapchain_t *swapchain) {
	return nullptr;
}
const skr_tex_t *skr_swapchain_get_depth(const skr_swapchain_t *swapchain) {
	return nullptr;
}
void                 skr_swapchain_destroy(skr_swapchain_t *swapchain) {}

///////////////////////////////////////////
// Texture                               //
///////////////////////////////////////////

skr_tex_t            skr_tex_from_native(void *native_tex, skr_tex_type_ type, skr_tex_fmt_ override_format) {
	return {};
}
skr_tex_t            skr_tex_create(skr_tex_type_ type, skr_use_ use, skr_tex_fmt_ format, skr_mip_ mip_maps) {
	return {};
}
void                 skr_tex_settings(skr_tex_t *tex, skr_tex_address_ address, skr_tex_sample_ sample, int32_t anisotropy) {}
void                 skr_tex_set_data(skr_tex_t *tex, void **data_frames, int32_t data_frame_count, int32_t width, int32_t height) {}
void                 skr_tex_set_active(const skr_tex_t *tex, int32_t slot) {}
void                 skr_tex_destroy(skr_tex_t *tex) {}

#endif