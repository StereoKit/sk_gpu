/*Licensed under MIT or Public Domain. See bottom of file for details.
	Author - Nick Klingensmith - @koujaku - https://twitter.com/koujaku
	Repository - https://github.com/maluoi/sk_gpu

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

#if   defined( SKG_FORCE_DIRECT3D11 )
#define SKG_DIRECT3D11
#elif defined( SKG_FORCE_OPENGL )
#define SKG_OPENGL
#elif defined( _WIN32 )
#define SKG_DIRECT3D11
#else
#define SKG_OPENGL
#endif

#include <stdint.h>
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
	skg_use_static,
	skg_use_dynamic,
} skg_use_;

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
	skg_tex_fmt_rgba64,
	skg_tex_fmt_rgba128,
	skg_tex_fmt_r8,
	skg_tex_fmt_r16,
	skg_tex_fmt_r32,
	skg_tex_fmt_depthstencil,
	skg_tex_fmt_depth32,
	skg_tex_fmt_depth16,
} skg_tex_fmt_;

typedef enum skg_fmt_ {
	skg_fmt_none,
	skg_fmt_f32_1,    skg_fmt_f32_2,    skg_fmt_f32_3,    skg_fmt_f32_4,
	skg_fmt_f16_1,    skg_fmt_f16_2,                      skg_fmt_f16_4,
	skg_fmt_i32_1,    skg_fmt_i32_2,    skg_fmt_i32_3,    skg_fmt_i32_4,
	skg_fmt_i16_1,    skg_fmt_i16_2,                      skg_fmt_i16_4,
	skg_fmt_i8_1,     skg_fmt_i8_2,                       skg_fmt_i8_4,
	skg_fmt_ui32_1,   skg_fmt_ui32_2,   skg_fmt_ui32_3,   skg_fmt_ui32_4,
	skg_fmt_ui16_1,   skg_fmt_ui16_2,                     skg_fmt_ui16_4,
	skg_fmt_ui8_1,    skg_fmt_ui8_2,                      skg_fmt_ui8_4,
	skg_fmt_ui16_n_1, skg_fmt_ui16_n_2,                   skg_fmt_ui16_n_4,
	skg_fmt_ui8_n_1,  skg_fmt_ui8_n_2,                    skg_fmt_ui8_n_4,
} skg_fmt_;

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
	skg_transparency_blend,
	skg_transparency_clip,
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
} skg_cap_;

typedef union {
	struct {
		uint8_t r, g, b, a;
	};
	uint32_t hex;
	uint8_t  arr[4];
} skg_color32_t;

typedef union {
	struct {
		float r, g, b, a;
	};
	float arr[4];
} skg_color128_t;

typedef struct skg_vert_t {
	float         pos [3];
	float         norm[3];
	float         uv  [2];
	skg_color32_t col;
} skg_vert_t;

typedef struct skg_bind_t {
	uint16_t slot;
	uint16_t stage_bits;
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

typedef struct skg_shader_texture_t {
	char       name [32];
	uint64_t   name_hash;
	char       extra[64];
	skg_bind_t bind;
} skg_shader_texture_t;

typedef struct skg_shader_meta_t {
	char                  name[256];
	uint32_t              buffer_count;
	skg_shader_buffer_t  *buffers;
	uint32_t              texture_count;
	skg_shader_texture_t *textures;
	int32_t               references;
	int32_t               global_buffer_id;
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
#endif

///////////////////////////////////////////

void                skg_setup_xlib               (void *dpy, void *vi, void *drawable);
int32_t             skg_init                     (const char *app_name, void *adapter_id);
void                skg_shutdown                 ();
void                skg_callback_log             (void (*callback)(skg_log_ level, const char *text));
void                skg_callback_file_read       (bool (*callback)(const char *filename, void **out_data, size_t *out_size));
skg_platform_data_t skg_get_platform_data        ();
bool                skg_capability               (skg_cap_ capability);

void                skg_draw_begin               ();
void                skg_draw                     (int32_t index_start, int32_t index_base, int32_t index_count, int32_t instance_count);
void                skg_viewport                 (const int32_t *xywh);
void                skg_viewport_get             (int32_t *out_xywh);

skg_buffer_t        skg_buffer_create            (const void *data, uint32_t size_count, uint32_t size_stride, skg_buffer_type_ type, skg_use_ use);
bool                skg_buffer_is_valid          (const skg_buffer_t *buffer);
void                skg_buffer_set_contents      (      skg_buffer_t *buffer, const void *data, uint32_t size_bytes);
void                skg_buffer_get_contents      (const skg_buffer_t *buffer, void *ref_buffer, uint32_t buffer_size);
void                skg_buffer_bind              (const skg_buffer_t *buffer, skg_bind_t slot_vc, uint32_t offset_vi);
void                skg_buffer_destroy           (      skg_buffer_t *buffer);

skg_mesh_t          skg_mesh_create              (const skg_buffer_t *vert_buffer, const skg_buffer_t *ind_buffer);
void                skg_mesh_set_verts           (      skg_mesh_t *mesh, const skg_buffer_t *vert_buffer);
void                skg_mesh_set_inds            (      skg_mesh_t *mesh, const skg_buffer_t *ind_buffer);
void                skg_mesh_bind                (const skg_mesh_t *mesh);
void                skg_mesh_destroy             (      skg_mesh_t *mesh);

skg_shader_stage_t  skg_shader_stage_create      (const void *shader_data, size_t shader_size, skg_stage_ type);
void                skg_shader_stage_destroy     (skg_shader_stage_t *stage);

skg_shader_t        skg_shader_create_file       (const char *sks_filename);
skg_shader_t        skg_shader_create_memory     (const void *sks_memory, size_t sks_memory_size);
skg_shader_t        skg_shader_create_manual     (skg_shader_meta_t *meta, skg_shader_stage_t v_shader, skg_shader_stage_t p_shader, skg_shader_stage_t c_shader);
bool                skg_shader_is_valid          (const skg_shader_t *shader);
skg_bind_t          skg_shader_get_tex_bind      (const skg_shader_t *shader, const char *name);
skg_bind_t          skg_shader_get_buffer_bind   (const skg_shader_t *shader, const char *name);
int32_t             skg_shader_get_var_count     (const skg_shader_t *shader);
int32_t             skg_shader_get_var_index     (const skg_shader_t *shader, const char *name);
int32_t             skg_shader_get_var_index_h   (const skg_shader_t *shader, uint64_t name_hash);
const skg_shader_var_t *skg_shader_get_var_info  (const skg_shader_t *shader, int32_t var_index);
void                skg_shader_destroy           (      skg_shader_t *shader);

skg_pipeline_t      skg_pipeline_create          (skg_shader_t *shader);
void                skg_pipeline_bind            (const skg_pipeline_t *pipeline);
void                skg_pipeline_set_transparency(      skg_pipeline_t *pipeline, skg_transparency_ transparency);
skg_transparency_   skg_pipeline_get_transparency(const skg_pipeline_t *pipeline);
void                skg_pipeline_set_cull        (      skg_pipeline_t *pipeline, skg_cull_ cull);
skg_cull_           skg_pipeline_get_cull        (const skg_pipeline_t *pipeline);
void                skg_pipeline_set_wireframe   (      skg_pipeline_t *pipeline, bool wireframe);
bool                skg_pipeline_get_wireframe   (const skg_pipeline_t *pipeline);
void                skg_pipeline_set_depth_write (      skg_pipeline_t *pipeline, bool write);
bool                skg_pipeline_get_depth_write (const skg_pipeline_t *pipeline);
void                skg_pipeline_set_depth_test  (      skg_pipeline_t *pipeline, skg_depth_test_ test);
skg_depth_test_     skg_pipeline_get_depth_test  (const skg_pipeline_t *pipeline);
void                skg_pipeline_destroy         (      skg_pipeline_t *pipeline);

skg_swapchain_t     skg_swapchain_create         (void *hwnd, skg_tex_fmt_ format, skg_tex_fmt_ depth_format, int32_t requested_width, int32_t requested_height);
void                skg_swapchain_resize         (      skg_swapchain_t *swapchain, int32_t width, int32_t height);
void                skg_swapchain_present        (      skg_swapchain_t *swapchain);
void                skg_swapchain_bind           (      skg_swapchain_t *swapchain, bool clear, const float *clear_color_4);
void                skg_swapchain_destroy        (      skg_swapchain_t *swapchain);

skg_tex_t           skg_tex_create_from_existing (void *native_tex, skg_tex_type_ type, skg_tex_fmt_ format, int32_t width, int32_t height, int32_t array_count);
skg_tex_t           skg_tex_create_from_layer    (void *native_tex, skg_tex_type_ type, skg_tex_fmt_ format, int32_t width, int32_t height, int32_t array_layer);
skg_tex_t           skg_tex_create               (skg_tex_type_ type, skg_use_ use, skg_tex_fmt_ format, skg_mip_ mip_maps);
bool                skg_tex_is_valid             (const skg_tex_t *tex);
void                skg_tex_attach_depth         (      skg_tex_t *tex, skg_tex_t *depth);
void                skg_tex_settings             (      skg_tex_t *tex, skg_tex_address_ address, skg_tex_sample_ sample, int32_t anisotropy);
void                skg_tex_set_contents         (      skg_tex_t *tex, const void *data, int32_t width, int32_t height);
void                skg_tex_set_contents_arr     (      skg_tex_t *tex, const void **data_frames, int32_t data_frame_count, int32_t width, int32_t height);
bool                skg_tex_get_contents         (      skg_tex_t *tex, void *ref_data, size_t data_size);
void                skg_tex_bind                 (const skg_tex_t *tex, skg_bind_t bind);
void                skg_tex_target_bind          (      skg_tex_t *render_target, bool clear, const float *clear_color_4);
skg_tex_t          *skg_tex_target_get           ();
void                skg_tex_destroy              (      skg_tex_t *tex);
int64_t             skg_tex_fmt_to_native        (skg_tex_fmt_ format);
skg_tex_fmt_        skg_tex_fmt_from_native      (int64_t      format);
uint32_t            skg_tex_fmt_size             (skg_tex_fmt_ format);

#include "sk_gpu_common.h"
///////////////////////////////////////////
// Implementations!                      //
///////////////////////////////////////////
