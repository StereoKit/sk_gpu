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
//#define SKR_FORCE_DIRECT3D11
//#define SKR_FORCE_OPENGL

#if   defined( SKR_FORCE_DIRECT3D11 )
#define SKR_DIRECT3D11
#elif defined( SKR_FORCE_OPENGL )
#define SKR_OPENGL
#elif defined( _WIN32 )
#define SKR_DIRECT3D11
#else
#define SKR_OPENGL
#endif

#include <stdint.h>
#include <stddef.h>

///////////////////////////////////////////

typedef enum skr_buffer_type_ {
	skr_buffer_type_vertex,
	skr_buffer_type_index,
	skr_buffer_type_constant,
} skr_buffer_type_;

typedef enum skr_tex_type_ {
	skr_tex_type_image,
	skr_tex_type_cubemap,
	skr_tex_type_rendertarget,
	skr_tex_type_depth,
} skr_tex_type_;

typedef enum skr_use_ {
	skr_use_static,
	skr_use_dynamic,
} skr_use_;

typedef enum skr_mip_ {
	skr_mip_generate,
	skr_mip_none,
} skr_mip_;

typedef enum skr_tex_address_ {
	skr_tex_address_repeat,
	skr_tex_address_clamp,
	skr_tex_address_mirror,
} skr_tex_address_;

typedef enum skr_tex_sample_ {
	skr_tex_sample_linear,
	skr_tex_sample_point,
	skr_tex_sample_anisotropic
} skr_tex_sample_;

typedef enum skr_tex_fmt_ {
	skr_tex_fmt_none = 0,
	skr_tex_fmt_rgba32,
	skr_tex_fmt_rgba32_linear,
	skr_tex_fmt_bgra32,
	skr_tex_fmt_bgra32_linear,
	skr_tex_fmt_rgba64,
	skr_tex_fmt_rgba128,
	skr_tex_fmt_r8,
	skr_tex_fmt_r16,
	skr_tex_fmt_r32,
	skr_tex_fmt_depthstencil,
	skr_tex_fmt_depth32,
	skr_tex_fmt_depth16,
} skr_tex_fmt_;

typedef enum skr_fmt_ {
	skr_fmt_none,
	skr_fmt_f32_1,    skr_fmt_f32_2,    skr_fmt_f32_3,    skr_fmt_f32_4,
	skr_fmt_f16_1,    skr_fmt_f16_2,                      skr_fmt_f16_4,
	skr_fmt_i32_1,    skr_fmt_i32_2,    skr_fmt_i32_3,    skr_fmt_i32_4,
	skr_fmt_i16_1,    skr_fmt_i16_2,                      skr_fmt_i16_4,
	skr_fmt_i8_1,     skr_fmt_i8_2,                       skr_fmt_i8_4,
	skr_fmt_ui32_1,   skr_fmt_ui32_2,   skr_fmt_ui32_3,   skr_fmt_ui32_4,
	skr_fmt_ui16_1,   skr_fmt_ui16_2,                     skr_fmt_ui16_4,
	skr_fmt_ui8_1,    skr_fmt_ui8_2,                      skr_fmt_ui8_4,
	skr_fmt_ui16_n_1, skr_fmt_ui16_n_2,                   skr_fmt_ui16_n_4,
	skr_fmt_ui8_n_1,  skr_fmt_ui8_n_2,                    skr_fmt_ui8_n_4,
} skr_fmt_;

typedef enum skr_el_semantic_ {
	skr_el_semantic_none,
	skr_el_semantic_position,
	skr_el_semantic_normal,
	skr_el_semantic_texcoord,
	skr_el_semantic_color,
	skr_el_semantic_target_index,
} skr_el_semantic_;

typedef enum skr_stage_ {
	skr_stage_vertex  = 1 << 0,
	skr_stage_pixel   = 1 << 1,
	skr_stage_compute = 1 << 2,
} skr_stage_;

typedef enum skr_shader_var_ {
	skr_shader_var_none,
	skr_shader_var_int,
	skr_shader_var_uint,
	skr_shader_var_uint8,
	skr_shader_var_float,
	skr_shader_var_double,
} skr_shader_var_;

typedef enum skr_transparency_ {
	skr_transparency_none = 1,
	skr_transparency_blend,
	skr_transparency_clip,
} skr_transparency_;

typedef enum skr_cull_ {
	skr_cull_back = 0,
	skr_cull_front,
	skr_cull_none,
} skr_cull_;

typedef enum skr_log_ {
	skr_log_info,
	skr_log_warning,
	skr_log_critical,
} skr_log_;

typedef enum skr_cap_ {
	skr_cap_tex_layer_select = 1,
	skr_cap_wireframe,
} skr_cap_;

typedef struct skr_vert_t {
	float   pos [3];
	float   norm[3];
	float   uv  [2];
	uint8_t col [4];
} skr_vert_t;

typedef struct skr_bind_t {
	uint16_t slot;
	uint16_t stage_bits;
} skr_bind_t;

typedef struct skr_shader_var_t {
	char     name [32];
	uint64_t name_hash;
	char     extra[64];
	uint32_t offset;
	uint32_t size;
	uint16_t type;
	uint16_t type_count;
} skr_shader_var_t;

typedef struct skr_shader_buffer_t {
	char              name[32];
	uint64_t          name_hash;
	skr_bind_t        bind;
	uint32_t          size;
	void             *defaults;
	uint32_t          var_count;
	skr_shader_var_t *vars;
} skr_shader_buffer_t;

typedef struct skr_shader_texture_t {
	char       name [32];
	uint64_t   name_hash;
	char       extra[64];
	skr_bind_t bind;
} skr_shader_texture_t;

typedef struct skr_shader_meta_t {
	char                  name[256];
	uint32_t              buffer_count;
	skr_shader_buffer_t  *buffers;
	uint32_t              texture_count;
	skr_shader_texture_t *textures;
	int32_t               references;
	int32_t               global_buffer_id;
} skr_shader_meta_t;

///////////////////////////////////////////

#if defined(SKR_DIRECT3D11)
#include "sk_gpu_dx11.h"
#elif defined(SKR_DIRECT3D12)
#include "sk_gpu_dx12.h"
#elif defined(SKR_VULKAN)
#include "sk_gpu_vk.h"
#elif defined(SKR_OPENGL)
#include "sk_gpu_gl.h"
#endif

///////////////////////////////////////////

int32_t             skr_init                     (const char *app_name, void *hwnd, void *adapter_id);
void                skr_shutdown                 ();
void                skr_callback_log             (void (*callback)(skr_log_ level, const char *text));
void                skr_callback_file_read       (bool (*callback)(const char *filename, void **out_data, size_t *out_size));
skr_platform_data_t skr_get_platform_data        ();
bool                skr_capability               (skr_cap_ capability);

void                skr_draw_begin               ();
void                skr_draw                     (int32_t index_start, int32_t index_count, int32_t instance_count);

skr_buffer_t        skr_buffer_create            (const void *data, uint32_t size_bytes, skr_buffer_type_ type, skr_use_ use);
bool                skr_buffer_is_valid          (const skr_buffer_t *buffer);
void                skr_buffer_set_contents      (      skr_buffer_t *buffer, const void *data, uint32_t size_bytes);
void                skr_buffer_bind              (const skr_buffer_t *buffer, skr_bind_t slot_vc, uint32_t stride_v, uint32_t offset_vi);
void                skr_buffer_destroy           (      skr_buffer_t *buffer);

skr_mesh_t          skr_mesh_create              (const skr_buffer_t *vert_buffer, const skr_buffer_t *ind_buffer);
void                skr_mesh_set_verts           (      skr_mesh_t *mesh, const skr_buffer_t *vert_buffer);
void                skr_mesh_set_inds            (      skr_mesh_t *mesh, const skr_buffer_t *ind_buffer);
void                skr_mesh_bind                (const skr_mesh_t *mesh);
void                skr_mesh_destroy             (      skr_mesh_t *mesh);

skr_shader_stage_t  skr_shader_stage_create      (const void *shader_data, size_t shader_size, skr_stage_ type);
void                skr_shader_stage_destroy     (skr_shader_stage_t *stage);

skr_shader_t        skr_shader_create_file       (const char *sks_filename);
skr_shader_t        skr_shader_create_memory     (const void *sks_memory, size_t sks_memory_size);
skr_shader_t        skr_shader_create_manual     (skr_shader_meta_t *meta, skr_shader_stage_t v_shader, skr_shader_stage_t p_shader);
bool                skr_shader_is_valid          (const skr_shader_t *shader);
skr_bind_t          skr_shader_get_tex_bind      (const skr_shader_t *shader, const char *name);
skr_bind_t          skr_shader_get_buffer_bind   (const skr_shader_t *shader, const char *name);
int32_t             skr_shader_get_var_count     (const skr_shader_t *shader);
int32_t             skr_shader_get_var_index     (const skr_shader_t *shader, const char *name);
int32_t             skr_shader_get_var_index_h   (const skr_shader_t *shader, uint64_t name_hash);
const skr_shader_var_t *skr_shader_get_var_info  (const skr_shader_t *shader, int32_t var_index);
void                skr_shader_destroy           (      skr_shader_t *shader);

skr_pipeline_t      skr_pipeline_create          (skr_shader_t *shader);
void                skr_pipeline_bind            (const skr_pipeline_t *pipeline);
void                skr_pipeline_set_transparency(      skr_pipeline_t *pipeline, skr_transparency_ transparency);
skr_transparency_   skr_pipeline_get_transparency(const skr_pipeline_t *pipeline);
void                skr_pipeline_set_cull        (      skr_pipeline_t *pipeline, skr_cull_ cull);
skr_cull_           skr_pipeline_get_cull        (const skr_pipeline_t *pipeline);
void                skr_pipeline_set_wireframe   (      skr_pipeline_t *pipeline, bool wireframe);
bool                skr_pipeline_get_wireframe   (const skr_pipeline_t *pipeline);
void                skr_pipeline_destroy         (      skr_pipeline_t *pipeline);

skr_swapchain_t     skr_swapchain_create         (skr_tex_fmt_ format, skr_tex_fmt_ depth_format, int32_t width, int32_t height);
void                skr_swapchain_resize         (      skr_swapchain_t *swapchain, int32_t width, int32_t height);
void                skr_swapchain_present        (      skr_swapchain_t *swapchain);
skr_tex_t          *skr_swapchain_get_next       (      skr_swapchain_t *swapchain);
void                skr_swapchain_destroy        (      skr_swapchain_t *swapchain);

skr_tex_t           skr_tex_create_from_existing (void *native_tex, skr_tex_type_ type, skr_tex_fmt_ format, int32_t width, int32_t height, int32_t array_count);
skr_tex_t           skr_tex_create_from_layer    (void *native_tex, skr_tex_type_ type, skr_tex_fmt_ format, int32_t width, int32_t height, int32_t array_layer);
skr_tex_t           skr_tex_create               (skr_tex_type_ type, skr_use_ use, skr_tex_fmt_ format, skr_mip_ mip_maps);
bool                skr_tex_is_valid             (const skr_tex_t *tex);
void                skr_tex_attach_depth         (      skr_tex_t *tex, skr_tex_t *depth);
void                skr_tex_settings             (      skr_tex_t *tex, skr_tex_address_ address, skr_tex_sample_ sample, int32_t anisotropy);
void                skr_tex_set_contents         (      skr_tex_t *tex, void **data_frames, int32_t data_frame_count, int32_t width, int32_t height);
void                skr_tex_get_contents         (      skr_tex_t *tex);
void                skr_tex_bind                 (const skr_tex_t *tex, skr_bind_t bind);
void                skr_tex_target_bind          (      skr_tex_t *render_target, bool clear, const float *clear_color_4);
skr_tex_t          *skr_tex_target_get           ();
void                skr_tex_destroy              (      skr_tex_t *tex);
int64_t             skr_tex_fmt_to_native        (skr_tex_fmt_ format);
skr_tex_fmt_        skr_tex_fmt_from_native      (int64_t      format);

#include "sk_gpu_common.h"
///////////////////////////////////////////
// Implementations!                      //
///////////////////////////////////////////
