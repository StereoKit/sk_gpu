#pragma once
#define SKG_MANUAL_SRGB

///////////////////////////////////////////

typedef struct skg_buffer_t {
	skg_use_         use;
	skg_buffer_type_ type;
	uint32_t         stride;
	uint32_t         _target;
	uint32_t         _buffer;
} skg_buffer_t;

typedef struct skg_mesh_t {
	uint32_t _ind_buffer;
	uint32_t _vert_buffer;
	uint32_t _layout;
	uint32_t _ind_format;
} skg_mesh_t;

typedef struct skg_shader_stage_t {
	skg_stage_ type;
	uint32_t   _shader;
} skg_shader_stage_t;

typedef struct skg_shader_t {
	skg_shader_meta_t *meta;
	uint32_t           _vertex;
	uint32_t           _pixel;
	uint32_t           _program;
	uint32_t           _compute;
} skg_shader_t;

typedef struct skg_pipeline_t {
	skg_transparency_ transparency;
	skg_cull_         cull;
	bool              wireframe;
	bool              depth_write;
	bool              scissor;
	skg_depth_test_   depth_test;
	skg_shader_meta_t*meta;
	skg_shader_t      _shader;
} skg_pipeline_t;

typedef struct skg_tex_t {
	int32_t       width;
	int32_t       height;
	int32_t       array_count;
	int32_t       array_start;
	int32_t       multisample;
	skg_use_      use;
	skg_tex_type_ type;
	skg_tex_fmt_  format;
	skg_mip_      mips;
	uint32_t      _texture;
	uint32_t      _framebuffer;
	uint32_t      _target;
	uint32_t      _access;
	uint32_t      _format;
} skg_tex_t;

typedef struct skg_swapchain_t {
	int32_t  width;
	int32_t  height;

#if   defined(_SKG_GL_LOAD_WGL)
	void *_hdc;
	void *_hwnd;
#elif defined(_SKG_GL_LOAD_EGL)
	void *_egl_surface;
#elif defined(_SKG_GL_LOAD_GLX)
	void *_x_window;
#elif defined(_SKG_GL_LOAD_EMSCRIPTEN) && defined(SKG_MANUAL_SRGB)
	skg_tex_t      _surface;
	skg_tex_t      _surface_depth;
	skg_shader_t   _convert_shader;
	skg_pipeline_t _convert_pipe;
	skg_buffer_t   _quad_vbuff;
	skg_buffer_t   _quad_ibuff;
	skg_mesh_t     _quad_mesh;
#endif
} skg_swapchain_t;

typedef struct skg_platform_data_t {
#if   defined(_SKG_GL_LOAD_EGL)
	void *_egl_display;
	void *_egl_config;
	void *_egl_context;
#elif defined(_SKG_GL_LOAD_WGL)
	void *_gl_hdc;
	void *_gl_hrc;
#elif defined(_SKG_GL_LOAD_GLX)
	void *_x_display;
	void *_visual_id;
	void *_glx_fb_config;
	void *_glx_drawable;
	void *_glx_context;
#endif
} skg_platform_data_t;
