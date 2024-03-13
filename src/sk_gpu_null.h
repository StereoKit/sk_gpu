#pragma once
#include "sk_gpu_dev.h"
///////////////////////////////////////////

typedef struct skg_buffer_t {
	skg_use_           use;
	skg_buffer_type_   type;
	uint32_t           stride;
} skg_buffer_t;

typedef struct skg_computebuffer_t {
	skg_read_          read_write;
	skg_buffer_t       buffer;
} skg_computebuffer_t;

typedef struct skg_mesh_t {
} skg_mesh_t;

typedef struct skg_shader_stage_t {
	skg_stage_         type;
} skg_shader_stage_t;

typedef struct skg_shader_t {
	skg_shader_meta_t* meta;
} skg_shader_t;

typedef struct skg_pipeline_t {
	skg_transparency_  transparency;
	skg_cull_          cull;
	bool               wireframe;
	bool               depth_write;
	bool               scissor;
	skg_depth_test_    depth_test;
	skg_shader_meta_t* meta;
} skg_pipeline_t;

typedef struct skg_tex_t {
	int32_t            width;
	int32_t            height;
	int32_t            array_count;
	int32_t            array_start;
	int32_t            multisample;
	skg_use_           use;
	skg_tex_type_      type;
	skg_tex_fmt_       format;
	skg_mip_           mips;
} skg_tex_t;

typedef struct skg_swapchain_t {
	int32_t            width;
	int32_t            height;
} skg_swapchain_t;

typedef struct skg_platform_data_t {
} skg_platform_data_t;
