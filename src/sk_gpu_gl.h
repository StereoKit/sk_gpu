#pragma once
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

typedef struct skr_shader_stage_t {
	skr_stage_ type;
	uint32_t   shader;
} skr_shader_stage_t;

typedef struct skr_shader_t {
	skr_shader_meta_t *meta;
	uint32_t           vertex;
	uint32_t           pixel;
	uint32_t           program;
} skr_shader_t;

typedef struct skr_pipeline_t {
	skr_transparency_ transparency;
	skr_cull_         cull;
	bool              wireframe;
	skr_shader_t      shader;
} skr_pipeline_t;

typedef struct skr_tex_t {
	int32_t       width;
	int32_t       height;
	int32_t       array_count;
	skr_use_      use;
	skr_tex_type_ type;
	skr_tex_fmt_  format;
	skr_mip_      mips;
	uint32_t      texture;
	uint32_t      framebuffer;
} skr_tex_t;

typedef struct skr_platform_data_t {
#if __ANDROID__
	void *egl_display;
	void *egl_config;
	void *egl_context;
#elif _WIN32
	void *gl_hdc;
	void *gl_hrc;
#endif
} skr_platform_data_t;

typedef struct skr_swapchain_t {
	int32_t   width;
	int32_t   height;
	skr_tex_t target;
	skr_tex_t depth;
	uint32_t  gl_framebuffer;
} skr_swapchain_t;