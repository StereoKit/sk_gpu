#pragma once
#include "sk_gpu_dev.h"
#include <d3d11.h>
#include <dxgi1_6.h>

///////////////////////////////////////////

typedef struct skg_buffer_t {
	skg_use_         use;
	skg_buffer_type_ type;
	uint32_t         stride;
	ID3D11Buffer    *_buffer;
} skg_buffer_t;

typedef struct skg_mesh_t {
	ID3D11Buffer *_ind_buffer;
	ID3D11Buffer *_vert_buffer;
} skg_mesh_t;

typedef struct skg_shader_stage_t {
	skg_stage_  type;
	void       *_shader;
} skg_shader_stage_t;

typedef struct skg_shader_t {
	skg_shader_meta_t   *meta;
	ID3D11VertexShader  *_vertex;
	ID3D11PixelShader   *_pixel;
	ID3D11ComputeShader *_compute;
} skg_shader_t;

typedef struct skg_pipeline_t {
	skg_transparency_        transparency;
	skg_cull_                cull;
	bool                     wireframe;
	bool                     depth_write;
	skg_depth_test_          depth_test;
	ID3D11VertexShader      *_vertex;
	ID3D11PixelShader       *_pixel;
	ID3D11BlendState        *_blend;
	ID3D11RasterizerState   *_rasterize;
	ID3D11DepthStencilState *_depth;
} skg_pipeline_t;

typedef struct skg_tex_t {
	int32_t                   width;
	int32_t                   height;
	int32_t                   array_count;
	skg_use_                  use;
	skg_tex_type_             type;
	skg_tex_fmt_              format;
	skg_mip_                  mips;
	ID3D11Texture2D          *_texture;
	ID3D11SamplerState       *_sampler;
	ID3D11ShaderResourceView *_resource;
	ID3D11RenderTargetView   *_target_view;
	ID3D11DepthStencilView   *_depth_view;
} skg_tex_t;

typedef struct skg_swapchain_t {
	int32_t          width;
	int32_t          height;
	skg_tex_t        _target;
	skg_tex_t        _depth;
	IDXGISwapChain1 *_swapchain;
} skg_swapchain_t;

typedef struct skg_platform_data_t {
	void *_d3d11_device;
} skg_platform_data_t;
