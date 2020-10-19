#pragma once

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

///////////////////////////////////////////

typedef struct skg_buffer_t {
	skg_use_         use;
	skg_buffer_type_ type;
	VkBuffer         buffer;
	VkDeviceMemory   memory;
};

typedef struct skg_mesh_t {
} skg_mesh_t;

typedef struct skg_shader_stage_t {
	skg_stage_     type;
	VkShaderModule module;
} skg_shader_stage_t;

typedef struct skg_shader_t {
	skg_shader_meta_t *meta;
	VkShaderModule     _vertex;
	VkShaderModule     _pixel;
} skg_shader_t;

typedef struct skg_pipeline_t {
	int64_t          pipeline;
	VkPipelineLayout pipeline_layout;
} skg_pipeline_t;

typedef struct skg_tex_t {
	int32_t         width;
	int32_t         height;
	skg_use_        use;
	skg_tex_type_   type;
	skg_tex_fmt_    format;
	skg_mip_        mips;

	VkImage         texture;
	VkDeviceMemory  texture_mem;
	VkImageView     view;

	VkFramebuffer   rt_framebuffer;
	int64_t         rt_renderpass;
	VkCommandBuffer rt_commandbuffer;
} skg_tex_t;

typedef struct skg_swapchain_t {
	int32_t width;
	int32_t height;
	//skg_tex_t target;
	//skg_tex_t depth;

	VkSurfaceFormatKHR format;
	VkSwapchainKHR     swapchain;
	//VkFence           *fence;
	uint32_t           img_active;
	uint32_t           img_count;
	VkImage           *imgs;
	skg_tex_t         *textures;
	uint32_t           img_curr;
	VkExtent2D         extents;
	VkFence           *img_fence;

	VkSemaphore sem_available[2];
	VkSemaphore sem_finished[2];
	VkFence     fence_flight[2];
	int32_t     sync_index;
} skg_swapchain_t;

typedef struct skg_platform_data_t {

} skg_platform_data_t;