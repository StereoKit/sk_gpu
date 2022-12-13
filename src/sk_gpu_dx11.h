#pragma once
#include "sk_gpu_dev.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <d3d11.h>
#include <dxgi1_6.h>

///////////////////////////////////////////

typedef struct skg_buffer_t {
	skg_use_         use;
	skg_buffer_type_ type;
	uint32_t         stride;
	ID3D11Buffer    *_buffer;
	ID3D11ShaderResourceView  *_resource;
	ID3D11UnorderedAccessView *_unordered;
} skg_buffer_t;

typedef struct skg_computebuffer_t {
	skg_read_                  read_write;
	skg_buffer_t               buffer;
	ID3D11ShaderResourceView  *_resource;
	ID3D11UnorderedAccessView *_unordered;
} skg_computebuffer_t;

typedef struct skg_mesh_t {
	ID3D11Buffer *_ind_buffer;
	ID3D11Buffer *_vert_buffer;
} skg_mesh_t;

typedef struct skg_shader_stage_t {
	skg_stage_         type;
	void              *_shader;
	ID3D11InputLayout *_layout;
} skg_shader_stage_t;

typedef struct skg_shader_t {
	skg_shader_meta_t   *meta;
	ID3D11VertexShader  *_vertex;
	ID3D11PixelShader   *_pixel;
	ID3D11ComputeShader *_compute;
	ID3D11InputLayout   *_layout;
} skg_shader_t;

typedef struct skg_pipeline_t {
	skg_transparency_        transparency;
	skg_cull_                cull;
	bool                     wireframe;
	bool                     depth_write;
	bool                     scissor;
	skg_depth_test_          depth_test;
	skg_shader_meta_t       *meta;
	ID3D11VertexShader      *_vertex;
	ID3D11PixelShader       *_pixel;
	ID3D11InputLayout       *_layout;
	ID3D11BlendState        *_blend;
	ID3D11RasterizerState   *_rasterize;
	ID3D11DepthStencilState *_depth;
} skg_pipeline_t;

typedef struct skg_tex_t {
	int32_t                    width;
	int32_t                    height;
	int32_t                    array_count;
	int32_t                    array_start;
	int32_t                    multisample;
	skg_use_                   use;
	skg_tex_type_              type;
	skg_tex_fmt_               format;
	skg_mip_                   mips;
	ID3D11Texture2D           *_texture;
	ID3D11SamplerState        *_sampler;
	ID3D11ShaderResourceView  *_resource;
	ID3D11UnorderedAccessView *_unordered;
	ID3D11RenderTargetView    *_target_view;
	ID3D11DepthStencilView    *_depth_view;
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
