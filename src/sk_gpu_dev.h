#pragma once

//#define SKR_VULKAN
//#define SKR_DIRECT3D12
//#define SKR_DIRECT3D11
//#define SKR_OPENGL

#include <stdint.h>

///////////////////////////////////////////

enum skr_buffer_type_ {
	skr_buffer_type_vertex,
	skr_buffer_type_index,
	skr_buffer_type_constant,
};

enum skr_tex_type_ {
	skr_tex_type_image,
	skr_tex_type_cubemap,
	skr_tex_type_rendertarget,
	skr_tex_type_depth,
};

enum skr_use_ {
	skr_use_static,
	skr_use_dynamic,
};

enum skr_mip_ {
	skr_mip_generate,
	skr_mip_none,
};

enum skr_tex_address_ {
	skr_tex_address_repeat,
	skr_tex_address_clamp,
	skr_tex_address_mirror,
};

enum skr_tex_sample_ {
	skr_tex_sample_linear,
	skr_tex_sample_point,
	skr_tex_sample_anisotropic
};

enum skr_tex_fmt_ {
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
};

enum skr_fmt_ {
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
};

enum skr_el_semantic_ {
	skr_el_semantic_none,
	skr_el_semantic_position,
	skr_el_semantic_normal,
	skr_el_semantic_texcoord,
	skr_el_semantic_color,
	skr_el_semantic_target_index,
};

enum skr_shader_ {
	skr_shader_vertex = 1 << 0,
	skr_shader_pixel  = 1 << 1,
};

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

typedef struct skr_vert_t {
	float   pos [3];
	float   norm[3];
	float   uv  [2];
	uint8_t col [4];
} skr_vert_t;

typedef struct skr_shader_bind_t {
	uint16_t slot;
	uint16_t stage_bits;
} skr_shader_bind_t;

typedef struct skr_shader_meta_var_t {
	char     name [32];
	char     extra[64];
	size_t   offset;
	size_t   size;
} skr_shader_meta_var_t;

typedef struct skr_shader_meta_buffer_t {
	char              name[32];
	skr_shader_bind_t bind;
	size_t            size;
	void             *defaults;
	uint32_t               var_count;
	skr_shader_meta_var_t *vars;
} skr_shader_meta_buffer_t;

typedef struct skr_shader_meta_texture_t {
	char              name [32];
	char              extra[64];
	skr_shader_bind_t bind;
	size_t            size;
} skr_shader_meta_texture_t;

typedef struct skr_shader_meta_t {
	char                       name[256];
	uint32_t                   buffer_count;
	skr_shader_meta_buffer_t  *buffers;
	uint32_t                   texture_count;
	skr_shader_meta_texture_t *textures;
	int32_t                    references;
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

int32_t             skr_init                (const char *app_name, void *hwnd, void *adapter_id);
void                skr_shutdown            ();
void                skr_log_callback        (void (*callback)(const char *text));
void                skr_file_read_callback  (bool (*callback)(const char *filename, void **out_data, size_t *out_size));

void                skr_draw_begin          ();
skr_platform_data_t skr_get_platform_data   ();
void                skr_set_render_target   (float clear_color[4], bool clear, skr_tex_t *render_target);
skr_tex_t          *skr_get_render_target   ();
void                skr_draw                (int32_t index_start, int32_t index_count, int32_t instance_count);
int64_t             skr_tex_fmt_to_native   (skr_tex_fmt_ format);
skr_tex_fmt_        skr_tex_fmt_from_native (int64_t format);

skr_buffer_t        skr_buffer_create       (const void *data, uint32_t size_bytes, skr_buffer_type_ type, skr_use_ use);
bool                skr_buffer_is_valid     (const skr_buffer_t *buffer);
void                skr_buffer_update       (      skr_buffer_t *buffer, const void *data, uint32_t size_bytes);
void                skr_buffer_set          (const skr_buffer_t *buffer, skr_shader_bind_t slot, uint32_t stride, uint32_t offset);
void                skr_buffer_destroy      (      skr_buffer_t *buffer);

skr_mesh_t          skr_mesh_create         (const skr_buffer_t *vert_buffer, const skr_buffer_t *ind_buffer);
void                skr_mesh_set            (const skr_mesh_t *mesh);
void                skr_mesh_destroy        (      skr_mesh_t *mesh);

skr_shader_stage_t  skr_shader_stage_create (const void *shader_data, size_t shader_size, skr_shader_ type);
void                skr_shader_stage_destroy(skr_shader_stage_t *stage);

skr_shader_t        skr_shader_create_file    (const char *sks_filename);
skr_shader_t        skr_shader_create_mem     (void *sks_data, size_t sks_data_size);
skr_shader_t        skr_shader_create_manual  (skr_shader_meta_t *meta, skr_shader_stage_t v_shader, skr_shader_stage_t p_shader);
skr_shader_bind_t   skr_shader_get_tex_bind   (const skr_shader_t *shader, const char *name);
skr_shader_bind_t   skr_shader_get_buffer_bind(const skr_shader_t *shader, const char *name);
void                skr_shader_destroy        (      skr_shader_t *shader);

skr_pipeline_t      skr_pipeline_create          (skr_shader_t *shader);
void                skr_pipeline_set             (const skr_pipeline_t *pipeline);
void                skr_pipeline_set_texture     ();
void                skr_pipeline_set_transparency(      skr_pipeline_t *pipeline, skr_transparency_ transparency);
void                skr_pipeline_set_cull        (      skr_pipeline_t *pipeline, skr_cull_ cull);
void                skr_pipeline_set_wireframe   (      skr_pipeline_t *pipeline, bool wireframe);
skr_transparency_   skr_pipeline_get_transparency(const skr_pipeline_t *pipeline);
skr_cull_           skr_pipeline_get_cull        (const skr_pipeline_t *pipeline);
bool                skr_pipeline_get_wireframe   (const skr_pipeline_t *pipeline);
void                skr_pipeline_destroy         (      skr_pipeline_t *pipeline);

skr_swapchain_t     skr_swapchain_create    (skr_tex_fmt_ format, skr_tex_fmt_ depth_format, int32_t width, int32_t height);
void                skr_swapchain_resize    (      skr_swapchain_t *swapchain, int32_t width, int32_t height);
void                skr_swapchain_present   (      skr_swapchain_t *swapchain);
skr_tex_t          *skr_swapchain_get_next  (      skr_swapchain_t *swapchain);
void                skr_swapchain_destroy   (      skr_swapchain_t *swapchain);

skr_tex_t           skr_tex_from_native     (void *native_tex, skr_tex_type_ type, skr_tex_fmt_ format, int32_t width, int32_t height, int32_t array_count);
skr_tex_t           skr_tex_create          (skr_tex_type_ type, skr_use_ use, skr_tex_fmt_ format, skr_mip_ mip_maps);
bool                skr_tex_is_valid        (const skr_tex_t *tex);
void                skr_tex_set_depth       (      skr_tex_t *tex, skr_tex_t *depth);
void                skr_tex_settings        (      skr_tex_t *tex, skr_tex_address_ address, skr_tex_sample_ sample, int32_t anisotropy);
void                skr_tex_set_data        (      skr_tex_t *tex, void **data_frames, int32_t data_frame_count, int32_t width, int32_t height);
void                skr_tex_get_data        (      skr_tex_t *tex);
void                skr_tex_set_active      (const skr_tex_t *tex, skr_shader_bind_t bind);
void                skr_tex_destroy         (      skr_tex_t *tex);
#include "sk_gpu_common.h"
///////////////////////////////////////////
// Implementations!                      //
///////////////////////////////////////////
