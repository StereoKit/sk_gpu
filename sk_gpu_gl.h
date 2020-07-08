#pragma once

///////////////////////////////////////////

typedef struct skr_buffer_t {
	skr_use_ use;
	uint32_t type;
	uint32_t buffer;
} skr_buffer_t;

typedef struct skr_mesh_t {
	uint32_t index_buffer;
	uint32_t layout;
} skr_mesh_t;

typedef struct skr_gl_attr_t {
	int32_t  location;
	int32_t  size;
	uint32_t type;
	uint8_t  normalized;
	void    *offset;
} skr_gl_attr_t;

typedef struct skr_shader_t {
	skr_shader_ type;
	uint32_t    shader;
} skr_shader_t;

typedef struct skr_shader_program_t {
	uint32_t program;
} skr_shader_program_t;

typedef struct skr_tex_t {
	int32_t       width;
	int32_t       height;
	skr_use_      use;
	skr_tex_type_ type;
	skr_tex_fmt_  format;
	skr_mip_      mips;
	uint32_t      texture;
} skr_tex_t;

typedef struct skr_platform_data_t {
	void *gl_hdc;
	void *gl_hrc;
} skr_platform_data_t;

typedef struct skr_swapchain_t {
	int32_t width;
	int32_t height;
	skr_tex_t target;
	skr_tex_t depth;
	uint32_t gl_framebuffer;
} skr_swapchain_t;