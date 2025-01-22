/*Licensed under MIT. See bottom of file for details.
	Author - Nick Klingensmith - @koujaku - https://twitter.com/koujaku
	Repository - https://github.com/StereoKit/sk_gpu

sk_gpu.h

	Docs are not yet here as the project is still somewhat in flight, but check
	out some of the example projects in the repository for reference! They're 
	pretty clean as is :)

	Note: this is currently an amalgamated file, so it's best to modify the 
	individual files in the repository if making modifications.
*/

#pragma once

// You can force sk_gpu to use a specific API, but if you don't, it'll pick
// an API appropriate for the platform it's being compiled for!
//
//#define SKG_FORCE_DIRECT3D11
//#define SKG_FORCE_OPENGL

// You can disable use of D3DCompile to make building this easier sometimes,
// since D3DCompile is primarily used to catch .sks shader files built from
// Linux to run on Windows, and this may not be critical in all cases.
//#define SKG_NO_D3DCOMPILER

// For OpenGL, sk_gpu caches GL state to reduce the number of state switches
// executed using GL's API. When debugging with a tool like RenderDoc, this can
// obscure the draw calls a bit. You can define this to turn off this
// optimization for clearer debug information.
//#define SKG_GL_EXPLICIT_STATE

#if   defined( SKG_FORCE_NULL )
#define SKG_NULL
#elif defined( SKG_FORCE_DIRECT3D11 )
#define SKG_DIRECT3D11
#elif defined( SKG_FORCE_OPENGL )
#define SKG_OPENGL
#elif defined( _WIN32 )
#define SKG_DIRECT3D11
#else
#define SKG_OPENGL
#endif

// If we're using OpenGL, set up some additional defines so we know what
// flavor of GL is being used, and how to load it.
#ifdef SKG_OPENGL
	#if   defined(__EMSCRIPTEN__)
		#define _SKG_GL_WEB
		#define _SKG_GL_LOAD_EMSCRIPTEN
	#elif defined(__ANDROID__)
		#define _SKG_GL_ES
		#define _SKG_GL_LOAD_EGL
		#define _SKG_GL_MAKE_FUNCTIONS
	#elif defined(__linux__)
		#if defined(SKG_LINUX_EGL)
			#define _SKG_GL_ES
			#define _SKG_GL_LOAD_EGL
			#define _SKG_GL_MAKE_FUNCTIONS
		#else
			#define _SKG_GL_DESKTOP
			#define _SKG_GL_LOAD_GLX
			#define _SKG_GL_MAKE_FUNCTIONS
		#endif
	#elif defined(_WIN32)
		#define _SKG_GL_DESKTOP
		#define _SKG_GL_LOAD_WGL
		#define _SKG_GL_MAKE_FUNCTIONS
	#endif
#endif

// Add definitions for how/if we want the functions exported
#ifdef __GNUC__
	#define SKG_API
#else
	#if defined(SKG_LIB_EXPORT)
		#define SKG_API __declspec(dllexport)
	#elif defined(SKG_LIB_IMPORT)
		#define SKG_API __declspec(dllimport)
	#else
		#define SKG_API
	#endif
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

///////////////////////////////////////////

typedef enum skg_buffer_type_ {
	skg_buffer_type_vertex,
	skg_buffer_type_index,
	skg_buffer_type_constant,
	skg_buffer_type_compute,
} skg_buffer_type_;

typedef enum skg_tex_type_ {
	skg_tex_type_image,
	skg_tex_type_cubemap,
	skg_tex_type_rendertarget,
	skg_tex_type_depth,
} skg_tex_type_;

typedef enum skg_use_ {
	skg_use_static        = 1 << 0,
	skg_use_dynamic       = 1 << 2,
	skg_use_compute_read  = 1 << 3,
	skg_use_compute_write = 1 << 4,
	skg_use_compute_readwrite = skg_use_compute_read | skg_use_compute_write
} skg_use_;

typedef enum skg_read_ {
	skg_read_only,
	skg_read_write,
} skg_read_;

typedef enum skg_mip_ {
	skg_mip_generate,
	skg_mip_none,
} skg_mip_;

typedef enum skg_tex_address_ {
	skg_tex_address_repeat,
	skg_tex_address_clamp,
	skg_tex_address_mirror,
} skg_tex_address_;

typedef enum skg_tex_sample_ {
	skg_tex_sample_linear,
	skg_tex_sample_point,
	skg_tex_sample_anisotropic
} skg_tex_sample_;

typedef enum skg_tex_fmt_ {
	skg_tex_fmt_none = 0,
	skg_tex_fmt_rgba32,
	skg_tex_fmt_rgba32_linear,
	skg_tex_fmt_bgra32,
	skg_tex_fmt_bgra32_linear,
	skg_tex_fmt_rg11b10,
	skg_tex_fmt_rgb10a2,
	skg_tex_fmt_rgba64u,
	skg_tex_fmt_rgba64s,
	skg_tex_fmt_rgba64f,
	skg_tex_fmt_rgba128,
	skg_tex_fmt_r8,
	skg_tex_fmt_r16u,
	skg_tex_fmt_r16s,
	skg_tex_fmt_r16f,
	skg_tex_fmt_r32,
	skg_tex_fmt_depthstencil,
	skg_tex_fmt_depth32,
	skg_tex_fmt_depth16,
	skg_tex_fmt_r8g8,

	skg_tex_fmt_bc1_rgb_srgb,
	skg_tex_fmt_bc1_rgb,
	skg_tex_fmt_bc3_rgba_srgb,
	skg_tex_fmt_bc3_rgba,
	skg_tex_fmt_bc4_r,
	skg_tex_fmt_bc5_rg,
	skg_tex_fmt_bc7_rgba_srgb,
	skg_tex_fmt_bc7_rgba,

	skg_tex_fmt_etc1_rgb,
	skg_tex_fmt_etc2_rgba_srgb,
	skg_tex_fmt_etc2_rgba,
	skg_tex_fmt_etc2_r11,
	skg_tex_fmt_etc2_rg11,
	skg_tex_fmt_pvrtc1_rgb_srgb, 
	skg_tex_fmt_pvrtc1_rgb,
	skg_tex_fmt_pvrtc1_rgba_srgb,
	skg_tex_fmt_pvrtc1_rgba,
	skg_tex_fmt_pvrtc2_rgba_srgb,
	skg_tex_fmt_pvrtc2_rgba,
	skg_tex_fmt_astc4x4_rgba_srgb,
	skg_tex_fmt_astc4x4_rgba,
	skg_tex_fmt_atc_rgb,
	skg_tex_fmt_atc_rgba,

	skg_tex_fmt_max,
	skg_tex_fmt_compressed_start = skg_tex_fmt_bc1_rgb_srgb,
} skg_tex_fmt_;

typedef enum skg_ind_fmt_ {
	skg_ind_fmt_u32,
	skg_ind_fmt_u16,
	skg_ind_fmt_u8,
} skg_ind_fmt_;

typedef enum skg_el_semantic_ {
	skg_el_semantic_none,
	skg_el_semantic_position,
	skg_el_semantic_normal,
	skg_el_semantic_texcoord,
	skg_el_semantic_color,
	skg_el_semantic_target_index,
} skg_el_semantic_;

typedef enum skg_stage_ {
	skg_stage_vertex  = 1 << 0,
	skg_stage_pixel   = 1 << 1,
	skg_stage_compute = 1 << 2,
} skg_stage_;

typedef enum skg_register_ {
	skg_register_default,
	skg_register_vertex,
	skg_register_index,
	skg_register_constant,
	skg_register_resource,
	skg_register_readwrite,
} skg_register_;

typedef enum skg_shader_var_ {
	skg_shader_var_none,
	skg_shader_var_int,
	skg_shader_var_uint,
	skg_shader_var_uint8,
	skg_shader_var_float,
	skg_shader_var_double,
} skg_shader_var_;

typedef enum skg_transparency_ {
	skg_transparency_none = 1,
	skg_transparency_alpha_to_coverage,
	skg_transparency_blend,
	skg_transparency_add,
} skg_transparency_;

typedef enum skg_cull_ {
	skg_cull_back = 0,
	skg_cull_front,
	skg_cull_none,
} skg_cull_;

typedef enum skg_depth_test_ {
	skg_depth_test_less = 0,
	skg_depth_test_less_or_eq,
	skg_depth_test_greater,
	skg_depth_test_greater_or_eq,
	skg_depth_test_equal,
	skg_depth_test_not_equal,
	skg_depth_test_always,
	skg_depth_test_never,
} skg_depth_test_;

typedef enum skg_log_ {
	skg_log_info,
	skg_log_warning,
	skg_log_critical,
} skg_log_;

typedef enum skg_cap_ {
	skg_cap_tex_layer_select = 1,
	skg_cap_wireframe,
	skg_cap_tiled_multisample,
	skg_cap_fmt_pvrtc1,
	skg_cap_fmt_pvrtc2,
	skg_cap_fmt_astc,
	skg_cap_fmt_atc,
	skg_cap_max,
} skg_cap_;

typedef struct {
	uint8_t r, g, b, a;
} skg_color32_t;

typedef struct {
	float r, g, b, a;
} skg_color128_t;

typedef enum skg_fmt_ {
	skg_fmt_none,
	skg_fmt_f64,
	skg_fmt_f32,
	skg_fmt_f16,
	skg_fmt_i32,
	skg_fmt_i16,
	skg_fmt_i8,
	skg_fmt_i32_normalized,
	skg_fmt_i16_normalized,
	skg_fmt_i8_normalized,
	skg_fmt_ui32,
	skg_fmt_ui16,
	skg_fmt_ui8,
	skg_fmt_ui32_normalized,
	skg_fmt_ui16_normalized,
	skg_fmt_ui8_normalized,
} skg_fmt_;

typedef enum skg_semantic_ {
	skg_semantic_none,
	skg_semantic_position,
	skg_semantic_texcoord,
	skg_semantic_normal,
	skg_semantic_binormal,
	skg_semantic_tangent,
	skg_semantic_color,
	skg_semantic_psize,
	skg_semantic_blendweight,
	skg_semantic_blendindices,
} skg_semantic_;

typedef struct skg_vert_component_t {
	skg_fmt_      format;
	uint8_t       count;
	skg_semantic_ semantic;
	uint8_t       semantic_slot;
} skg_vert_component_t;

typedef struct skg_vert_t {
	float         pos [3];
	float         norm[3];
	float         uv  [2];
	skg_color32_t col;
} skg_vert_t;

typedef struct skg_bind_t {
	uint16_t slot;
	uint8_t  stage_bits;
	uint8_t  register_type;
} skg_bind_t;

typedef struct skg_shader_var_t {
	char     name [32];
	uint64_t name_hash;
	char     extra[64];
	uint32_t offset;
	uint32_t size;
	uint16_t type;
	uint16_t type_count;
} skg_shader_var_t;

typedef struct skg_shader_buffer_t {
	char              name[32];
	uint64_t          name_hash;
	skg_bind_t        bind;
	uint32_t          size;
	void             *defaults;
	uint32_t          var_count;
	skg_shader_var_t *vars;
} skg_shader_buffer_t;

typedef struct skg_shader_resource_t {
	char       name [32];
	uint64_t   name_hash;
	char       value[64];
	char       tags [64];
	skg_bind_t bind;
} skg_shader_resource_t;

typedef struct skg_shader_ops_t {
	int32_t total;
	int32_t tex_read;
	int32_t dynamic_flow;
} skg_shader_ops_t;

typedef struct skg_shader_meta_t {
	char                   name[256];
	uint32_t               buffer_count;
	skg_shader_buffer_t   *buffers;
	uint32_t               resource_count;
	skg_shader_resource_t *resources;
	int32_t                references;
	int32_t                global_buffer_id;
	skg_vert_component_t  *vertex_inputs;
	int32_t                vertex_input_count;
	skg_shader_ops_t       ops_vertex;
	skg_shader_ops_t       ops_pixel;
} skg_shader_meta_t;

///////////////////////////////////////////

#if defined(SKG_DIRECT3D11)
#include "sk_gpu_dx11.h"
#elif defined(SKG_DIRECT3D12)
#include "sk_gpu_dx12.h"
#elif defined(SKG_VULKAN)
#include "sk_gpu_vk.h"
#elif defined(SKG_OPENGL)
#include "sk_gpu_gl.h"
#elif defined(SKG_NULL)
#include "sk_gpu_null.h"
#endif

///////////////////////////////////////////

SKG_API void                skg_setup_xlib               (void *dpy, void *vi, void *fbconfig, void *drawable);
SKG_API int32_t             skg_init                     (const char *app_name, void *adapter_id);
SKG_API const char*         skg_adapter_name             ();
SKG_API void                skg_shutdown                 ();
SKG_API void                skg_callback_log             (void (*callback)(skg_log_ level, const char *text));
SKG_API void                skg_callback_file_read       (bool (*callback)(const char *filename, void **out_data, size_t *out_size));
SKG_API skg_platform_data_t skg_get_platform_data        ();
SKG_API bool                skg_capability               (skg_cap_ capability);

SKG_API void                skg_event_begin              (const char *name);
SKG_API void                skg_event_end                ();

SKG_API void                skg_draw_begin               ();
SKG_API void                skg_draw                     (int32_t index_start, int32_t index_base, int32_t index_count, int32_t instance_count);
SKG_API void                skg_compute                  (uint32_t thread_count_x, uint32_t thread_count_y, uint32_t thread_count_z);
SKG_API void                skg_viewport                 (const int32_t *xywh);
SKG_API void                skg_viewport_get             (int32_t *out_xywh);
SKG_API void                skg_scissor                  (const int32_t *xywh);
SKG_API void                skg_target_clear             (bool depth, const float *clear_color_4);

SKG_API skg_buffer_t        skg_buffer_create            (const void *data, uint32_t size_count, uint32_t size_stride, skg_buffer_type_ type, skg_use_ use);
SKG_API void                skg_buffer_name              (      skg_buffer_t *buffer, const char* name);
SKG_API bool                skg_buffer_is_valid          (const skg_buffer_t *buffer);
SKG_API void                skg_buffer_set_contents      (      skg_buffer_t *buffer, const void *data, uint32_t size_bytes);
SKG_API void                skg_buffer_get_contents      (const skg_buffer_t *buffer, void *ref_buffer, uint32_t buffer_size);
SKG_API void                skg_buffer_bind              (const skg_buffer_t *buffer, skg_bind_t slot_vc);
SKG_API void                skg_buffer_clear             (      skg_bind_t bind);
SKG_API void                skg_buffer_destroy           (      skg_buffer_t *buffer);

SKG_API skg_mesh_t          skg_mesh_create              (const skg_buffer_t *vert_buffer, const skg_buffer_t *ind_buffer);
SKG_API void                skg_mesh_name                (      skg_mesh_t *mesh, const char* name);
SKG_API void                skg_mesh_set_verts           (      skg_mesh_t *mesh, const skg_buffer_t *vert_buffer);
SKG_API void                skg_mesh_set_inds            (      skg_mesh_t *mesh, const skg_buffer_t *ind_buffer);
SKG_API void                skg_mesh_bind                (const skg_mesh_t *mesh);
SKG_API void                skg_mesh_destroy             (      skg_mesh_t *mesh);

SKG_API skg_shader_stage_t  skg_shader_stage_create      (const void *shader_data, size_t shader_size, skg_stage_ type);
SKG_API void                skg_shader_stage_destroy     (skg_shader_stage_t *stage);

SKG_API skg_shader_t        skg_shader_create_file       (const char *sks_filename);
SKG_API skg_shader_t        skg_shader_create_memory     (const void *sks_memory, size_t sks_memory_size);
SKG_API skg_shader_t        skg_shader_create_manual     (skg_shader_meta_t *meta, skg_shader_stage_t v_shader, skg_shader_stage_t p_shader, skg_shader_stage_t c_shader);
SKG_API void                skg_shader_name              (      skg_shader_t *shader, const char* name);
SKG_API bool                skg_shader_is_valid          (const skg_shader_t *shader);
SKG_API void                skg_shader_compute_bind      (const skg_shader_t *shader);
SKG_API skg_bind_t          skg_shader_get_bind          (const skg_shader_t *shader, const char *name);
SKG_API int32_t             skg_shader_get_var_count     (const skg_shader_t *shader);
SKG_API int32_t             skg_shader_get_var_index     (const skg_shader_t *shader, const char *name);
SKG_API int32_t             skg_shader_get_var_index_h   (const skg_shader_t *shader, uint64_t name_hash);
SKG_API const skg_shader_var_t *skg_shader_get_var_info  (const skg_shader_t *shader, int32_t var_index);
SKG_API void                skg_shader_destroy           (      skg_shader_t *shader);

SKG_API skg_pipeline_t      skg_pipeline_create          (skg_shader_t *shader);
SKG_API void                skg_pipeline_name            (      skg_pipeline_t *pipeline, const char* name);
SKG_API void                skg_pipeline_bind            (const skg_pipeline_t *pipeline);
SKG_API void                skg_pipeline_set_transparency(      skg_pipeline_t *pipeline, skg_transparency_ transparency);
SKG_API skg_transparency_   skg_pipeline_get_transparency(const skg_pipeline_t *pipeline);
SKG_API void                skg_pipeline_set_cull        (      skg_pipeline_t *pipeline, skg_cull_ cull);
SKG_API skg_cull_           skg_pipeline_get_cull        (const skg_pipeline_t *pipeline);
SKG_API void                skg_pipeline_set_wireframe   (      skg_pipeline_t *pipeline, bool wireframe);
SKG_API bool                skg_pipeline_get_wireframe   (const skg_pipeline_t *pipeline);
SKG_API void                skg_pipeline_set_depth_write (      skg_pipeline_t *pipeline, bool write);
SKG_API bool                skg_pipeline_get_depth_write (const skg_pipeline_t *pipeline);
SKG_API void                skg_pipeline_set_depth_test  (      skg_pipeline_t *pipeline, skg_depth_test_ test);
SKG_API skg_depth_test_     skg_pipeline_get_depth_test  (const skg_pipeline_t *pipeline);
SKG_API void                skg_pipeline_set_scissor     (      skg_pipeline_t *pipeline, bool enable);
SKG_API bool                skg_pipeline_get_scissor     (const skg_pipeline_t *pipeline);
SKG_API void                skg_pipeline_destroy         (      skg_pipeline_t *pipeline);

SKG_API skg_swapchain_t     skg_swapchain_create         (void *hwnd, skg_tex_fmt_ format, skg_tex_fmt_ depth_format, int32_t requested_width, int32_t requested_height);
SKG_API void                skg_swapchain_resize         (      skg_swapchain_t *swapchain, int32_t width, int32_t height);
SKG_API void                skg_swapchain_present        (      skg_swapchain_t *swapchain);
SKG_API void                skg_swapchain_bind           (      skg_swapchain_t *swapchain);
SKG_API void                skg_swapchain_destroy        (      skg_swapchain_t *swapchain);

SKG_API skg_tex_t           skg_tex_create_from_existing (void *native_tex, skg_tex_type_ type, skg_tex_fmt_ format, int32_t width, int32_t height, int32_t array_count, int32_t multisample, int32_t framebuffer_multisample);
SKG_API skg_tex_t           skg_tex_create_from_layer    (void *native_tex, skg_tex_type_ type, skg_tex_fmt_ format, int32_t width, int32_t height, int32_t array_layer);
SKG_API skg_tex_t           skg_tex_create               (skg_tex_type_ type, skg_use_ use, skg_tex_fmt_ format, skg_mip_ mip_maps);
SKG_API void                skg_tex_name                 (      skg_tex_t *tex, const char* name);
SKG_API bool                skg_tex_is_valid             (const skg_tex_t *tex);
SKG_API void                skg_tex_copy_to              (const skg_tex_t *tex, int32_t tex_surface, skg_tex_t *destination, int32_t dest_surface);
SKG_API void                skg_tex_copy_to_swapchain    (const skg_tex_t *tex, skg_swapchain_t *destination);
SKG_API void                skg_tex_attach_depth         (      skg_tex_t *tex, skg_tex_t *depth);
SKG_API void                skg_tex_settings             (      skg_tex_t *tex, skg_tex_address_ address, skg_tex_sample_ sample, int32_t anisotropy);
SKG_API void                skg_tex_set_contents         (      skg_tex_t *tex, const void *data, int32_t width, int32_t height);
SKG_API void                skg_tex_set_contents_arr     (      skg_tex_t *tex, const void**array_data, int32_t array_count, int32_t mip_count, int32_t width, int32_t height, int32_t multisample);
SKG_API bool                skg_tex_get_contents         (      skg_tex_t *tex, void *ref_data, size_t data_size);
SKG_API bool                skg_tex_get_mip_contents     (      skg_tex_t *tex, int32_t mip_level, void *ref_data, size_t data_size);
SKG_API bool                skg_tex_get_mip_contents_arr (      skg_tex_t *tex, int32_t mip_level, int32_t arr_index, void *ref_data, size_t data_size);
SKG_API bool                skg_tex_gen_mips             (      skg_tex_t *tex);
SKG_API void*               skg_tex_get_native           (const skg_tex_t *tex);
SKG_API void                skg_tex_bind                 (const skg_tex_t *tex, skg_bind_t bind);
SKG_API void                skg_tex_clear                (skg_bind_t bind);
SKG_API void                skg_tex_target_bind          (      skg_tex_t *render_target, int32_t layer_idx, int32_t mip_level);
SKG_API skg_tex_t          *skg_tex_target_get           ();
SKG_API void                skg_tex_destroy              (      skg_tex_t *tex);
SKG_API int64_t             skg_tex_fmt_to_native        (skg_tex_fmt_ format);
SKG_API skg_tex_fmt_        skg_tex_fmt_from_native      (int64_t      format);
SKG_API uint32_t            skg_tex_fmt_memory           (skg_tex_fmt_ format, int32_t width, int32_t height);
SKG_API uint32_t            skg_tex_fmt_block_px         (skg_tex_fmt_ format);
SKG_API uint32_t            skg_tex_fmt_block_size       (skg_tex_fmt_ format);
SKG_API uint32_t            skg_tex_fmt_pitch            (skg_tex_fmt_ format, int32_t width);
SKG_API bool                skg_tex_fmt_supported        (skg_tex_fmt_ format);
SKG_API bool                skg_tex_fmt_is_compressed    (skg_tex_fmt_ format);

#include "sk_gpu_common.h"
///////////////////////////////////////////
// Implementations!                      //
///////////////////////////////////////////
