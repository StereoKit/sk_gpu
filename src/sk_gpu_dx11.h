#pragma once
#include "sk_gpu_dev.h"
#include <d3d11.h>
#include <dxgi1_6.h>

///////////////////////////////////////////

typedef struct skr_buffer_t {
	skr_use_         use;
	skr_buffer_type_ type;
	ID3D11Buffer    *buffer;
} skr_buffer_t;

typedef struct skr_mesh_t {
	ID3D11Buffer *ind_buffer;
	ID3D11Buffer *vert_buffer;
} skr_mesh_t;

typedef struct skr_shader_stage_t {
	skr_shader_  type;
	void        *shader;
} skr_shader_stage_t;

typedef struct skr_shader_t {
	skr_shader_meta_t  *meta;
	ID3D11VertexShader *vertex;
	ID3D11PixelShader  *pixel;
} skr_shader_t;

typedef struct skr_pipeline_t {
	skr_transparency_ transparency;
	skr_cull_         cull;
	bool              wireframe;
	ID3D11VertexShader    *vertex;
	ID3D11PixelShader     *pixel;
	ID3D11BlendState      *blend;
	ID3D11RasterizerState *rasterize;
} skr_pipeline_t;

typedef struct skr_tex_t {
	int32_t width;
	int32_t height;
	int32_t array_count;
	skr_use_                  use;
	skr_tex_type_             type;
	skr_tex_fmt_              format;
	skr_mip_                  mips;
	ID3D11Texture2D          *texture;
	ID3D11SamplerState       *sampler;
	ID3D11ShaderResourceView *resource;
	ID3D11RenderTargetView   *target_view;
	ID3D11DepthStencilView   *depth_view;
	skr_tex_t                *depth_tex;
} skr_tex_t;

typedef struct skr_swapchain_t {
	int32_t   width;
	int32_t   height;
	skr_tex_t target;
	skr_tex_t depth;
	IDXGISwapChain1 *d3d_swapchain;
} skr_swapchain_t;

typedef struct skr_platform_data_t {
	void *d3d11_device;
} skr_platform_data_t;
