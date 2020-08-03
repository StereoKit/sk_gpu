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
	skr_shader_vertex,
	skr_shader_pixel,
};

typedef struct skr_vert_t {
	float   pos [3];
	float   norm[3];
	float   uv  [2];
	uint8_t col [4];
} skr_vert_t;

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
#include "sk_gpu_common.h"

///////////////////////////////////////////

int32_t             skr_init                (const char *app_name, void *hwnd, void *adapter_id);
void                skr_shutdown            ();
void                skr_draw_begin          ();
skr_platform_data_t skr_get_platform_data   ();
void                skr_set_render_target   (float clear_color[4], bool clear, skr_tex_t *render_target);
skr_tex_t          *skr_get_render_target   ();
void                skr_draw                (int32_t index_start, int32_t index_count, int32_t instance_count);
int64_t             skr_tex_fmt_to_native   (skr_tex_fmt_ format);
skr_tex_fmt_        skr_tex_fmt_from_native (int64_t format);
void                skr_log_callback        (void (*callback)(const char *text));

skr_buffer_t        skr_buffer_create       (const void *data, uint32_t size_bytes, skr_buffer_type_ type, skr_use_ use);
void                skr_buffer_update       (      skr_buffer_t *buffer, const void *data, uint32_t size_bytes);
void                skr_buffer_set          (const skr_buffer_t *buffer, uint32_t slot, uint32_t stride, uint32_t offset);
void                skr_buffer_destroy      (      skr_buffer_t *buffer);

skr_mesh_t          skr_mesh_create         (const skr_buffer_t *vert_buffer, const skr_buffer_t *ind_buffer);
void                skr_mesh_set            (const skr_mesh_t *mesh);
void                skr_mesh_destroy        (      skr_mesh_t *mesh);

skr_shader_stage_t  skr_shader_stage_create (const uint8_t *shader_data, size_t shader_size, skr_shader_ type);
void                skr_shader_stage_destroy(skr_shader_stage_t *stage);

skr_shader_t        skr_shader_create       (const skr_shader_stage_t *vertex, const skr_shader_stage_t *pixel);
void                skr_shader_set          (const skr_shader_t *shader);
void                skr_shader_destroy      (      skr_shader_t *shader);

skr_swapchain_t     skr_swapchain_create    (skr_tex_fmt_ format, skr_tex_fmt_ depth_format, int32_t width, int32_t height);
void                skr_swapchain_resize    (      skr_swapchain_t *swapchain, int32_t width, int32_t height);
void                skr_swapchain_present   (      skr_swapchain_t *swapchain);
skr_tex_t          *skr_swapchain_get_next  (      skr_swapchain_t *swapchain);
void                skr_swapchain_destroy   (      skr_swapchain_t *swapchain);

skr_tex_t           skr_tex_from_native     (void *native_tex, skr_tex_type_ type, skr_tex_fmt_ format, int32_t width, int32_t height);
skr_tex_t           skr_tex_create          (skr_tex_type_ type, skr_use_ use, skr_tex_fmt_ format, skr_mip_ mip_maps);
void                skr_tex_set_depth       (      skr_tex_t *tex, skr_tex_t *depth);
void                skr_tex_settings        (      skr_tex_t *tex, skr_tex_address_ address, skr_tex_sample_ sample, int32_t anisotropy);
void                skr_tex_set_data        (      skr_tex_t *tex, void **data_frames, int32_t data_frame_count, int32_t width, int32_t height);
void                skr_tex_set_active      (const skr_tex_t *tex, int32_t slot);
void                skr_tex_destroy         (      skr_tex_t *tex);
