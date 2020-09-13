#pragma once
#include "sk_gpu_dev.h"
#include <d3d11.h>
#include <dxgi1_6.h>

///////////////////////////////////////////

typedef struct skr_buffer_t {
	skr_use_         use;
	skr_buffer_type_ type;
	ID3D11Buffer    *_buffer;
} skr_buffer_t;

typedef struct skr_mesh_t {
	ID3D11Buffer *_ind_buffer;
	ID3D11Buffer *_vert_buffer;
} skr_mesh_t;

typedef struct skr_shader_stage_t {
	skr_stage_  type;
	void       *_shader;
} skr_shader_stage_t;

typedef struct skr_shader_t {
	skr_shader_meta_t  *meta;
	ID3D11VertexShader *_vertex;
	ID3D11PixelShader  *_pixel;
} skr_shader_t;

typedef struct skr_pipeline_t {
	skr_transparency_      transparency;
	skr_cull_              cull;
	bool                   wireframe;
	ID3D11VertexShader    *_vertex;
	ID3D11PixelShader     *_pixel;
	ID3D11BlendState      *_blend;
	ID3D11RasterizerState *_rasterize;
} skr_pipeline_t;

typedef struct skr_tex_t {
	int32_t                   width;
	int32_t                   height;
	int32_t                   array_count;
	skr_use_                  use;
	skr_tex_type_             type;
	skr_tex_fmt_              format;
	skr_mip_                  mips;
	ID3D11Texture2D          *_texture;
	ID3D11SamplerState       *_sampler;
	ID3D11ShaderResourceView *_resource;
	ID3D11RenderTargetView   *_target_view;
	ID3D11DepthStencilView   *_depth_view;
} skr_tex_t;

typedef struct skr_swapchain_t {
	int32_t          width;
	int32_t          height;
	skr_tex_t        _target;
	skr_tex_t        _depth;
	IDXGISwapChain1 *_swapchain;
} skr_swapchain_t;

typedef struct skr_platform_data_t {
	void *_d3d11_device;
} skr_platform_data_t;
