#ifdef SKR_DIRECT3D11
#include "sk_gpu.h"

#pragma comment(lib,"D3D11.lib")
#pragma comment(lib,"Dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")
#include <d3d11.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <math.h>

#include <stdio.h>

///////////////////////////////////////////

ID3D11Device             *d3d_device      = nullptr;
ID3D11DeviceContext      *d3d_context     = nullptr;
ID3D11InfoQueue          *d3d_info        = nullptr;
ID3D11InputLayout        *d3d_vert_layout = nullptr;
ID3D11RasterizerState    *d3d_rasterstate = nullptr;
void                     *d3d_hwnd        = nullptr;

///////////////////////////////////////////

size_t       skr_el_to_size(skr_fmt_ desc);
DXGI_FORMAT  skr_el_to_d3d (skr_fmt_ desc);
const char  *skr_semantic_to_d3d(skr_el_semantic_ semantic);
skr_tex_fmt_ skr_d3d_to_tex_fmt(DXGI_FORMAT format);
uint32_t     skr_tex_fmt_size  (skr_tex_fmt_ format);
bool         skr_tex_make_view (skr_tex_t *tex, uint32_t mip_count, uint32_t array_size, bool use_in_shader);

template <typename T>
void skr_downsample_1(T *data, int32_t width, int32_t height, T **out_data, int32_t *out_width, int32_t *out_height);
template <typename T>
void skr_downsample_4(T *data, int32_t width, int32_t height, T **out_data, int32_t *out_width, int32_t *out_height);

///////////////////////////////////////////

int32_t skr_init(const char *app_name, void *hwnd, void *adapter_id) {
	d3d_hwnd = hwnd;
	UINT creation_flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
	creation_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	// Find the right adapter to use:
	IDXGIAdapter1 *final_adapter = nullptr;
	if (adapter_id != nullptr) {
		IDXGIFactory1 *dxgi_factory;
		CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void **)(&dxgi_factory));

		int curr = 0;
		IDXGIAdapter1 *curr_adapter = nullptr;
		while (dxgi_factory->EnumAdapters1(curr++, &curr_adapter) == S_OK) {
			DXGI_ADAPTER_DESC1 adapterDesc;
			curr_adapter->GetDesc1(&adapterDesc);

			if (memcmp(&adapterDesc.AdapterLuid, adapter_id, sizeof(LUID)) == 0) {
				final_adapter = curr_adapter;
				break;
			}
			curr_adapter->Release();
		}
		dxgi_factory->Release();
	}

	D3D_FEATURE_LEVEL feature_levels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
	if (FAILED(D3D11CreateDevice(final_adapter, final_adapter == nullptr ? D3D_DRIVER_TYPE_HARDWARE : D3D_DRIVER_TYPE_UNKNOWN, 0, creation_flags, feature_levels, _countof(feature_levels), D3D11_SDK_VERSION, &d3d_device, nullptr, &d3d_context))) {
		return -1;
	}
	printf("sk_gpu: Using Direct3D 11\n");

	if (final_adapter != nullptr)
		final_adapter->Release();

	// Hook into debug information
	ID3D11Debug *d3d_debug = nullptr;
	if (SUCCEEDED(d3d_device->QueryInterface(__uuidof(ID3D11Debug), (void**)&d3d_debug))) {
		d3d_info = nullptr;
		if (SUCCEEDED(d3d_debug->QueryInterface(__uuidof(ID3D11InfoQueue), (void**)&d3d_info))) {
			D3D11_MESSAGE_ID hide[] = {
				D3D11_MESSAGE_ID_SETPRIVATEDATA_CHANGINGPARAMS, 
				(D3D11_MESSAGE_ID)351,
				(D3D11_MESSAGE_ID)49, // TODO: Figure out the Flip model for backbuffers!
									  // Add more message IDs here as needed
			};

			D3D11_INFO_QUEUE_FILTER filter = {};
			filter.DenyList.NumIDs = _countof(hide);
			filter.DenyList.pIDList = hide;
			d3d_info->ClearStorageFilter();
			d3d_info->AddStorageFilterEntries(&filter);
		}
		d3d_debug->Release();
	}

	D3D11_RASTERIZER_DESC desc_rasterizer = {};
	desc_rasterizer.FillMode = D3D11_FILL_SOLID;
	desc_rasterizer.CullMode = D3D11_CULL_BACK;
	desc_rasterizer.FrontCounterClockwise = true;
	d3d_device->CreateRasterizerState(&desc_rasterizer, &d3d_rasterstate);
	d3d_context->RSSetState(d3d_rasterstate);

	return 1;
}

///////////////////////////////////////////

void skr_shutdown() {
	if (d3d_rasterstate) { d3d_rasterstate->Release(); d3d_rasterstate = nullptr; }
	if (d3d_info     ) { d3d_info     ->Release(); d3d_info      = nullptr; }
	if (d3d_context  ) { d3d_context  ->Release(); d3d_context   = nullptr; }
	if (d3d_device   ) { d3d_device   ->Release(); d3d_device    = nullptr; }
}

///////////////////////////////////////////

void skr_draw_begin() {
	d3d_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	d3d_context->IASetInputLayout      (d3d_vert_layout);
}

///////////////////////////////////////////

skr_platform_data_t skr_get_platform_data() {
	skr_platform_data_t result = {};
	result.d3d11_device = d3d_device;

	return result;
}

///////////////////////////////////////////

void skr_set_render_target(float clear_color[4], const skr_tex_t *render_target, const skr_tex_t *depth_target) {
	if (render_target == nullptr) {
		d3d_context->OMSetRenderTargets(0, nullptr, nullptr);
		return;
	}
	if (render_target->type != skr_tex_type_rendertarget || (depth_target != nullptr && depth_target->type != skr_tex_type_depth))
		return;

	D3D11_VIEWPORT viewport = CD3D11_VIEWPORT(0.f, 0.f, (float)render_target->width, (float)render_target->height);
	d3d_context->RSSetViewports(1, &viewport);

	float clear[] = { 0, 0, 0, 1 };
	d3d_context->ClearRenderTargetView(render_target->target_view, clear_color);
	d3d_context->ClearDepthStencilView(depth_target ->depth_view, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	d3d_context->OMSetRenderTargets(1, &render_target->target_view, depth_target->depth_view);
}

///////////////////////////////////////////

void skr_draw(int32_t index_start, int32_t index_count, int32_t instance_count) {
	d3d_context->DrawIndexedInstanced(index_count, instance_count, index_start, 0, 0);
}

///////////////////////////////////////////

skr_buffer_t skr_buffer_create(const void *data, uint32_t size_bytes, skr_buffer_type_ type, skr_use_ use) {
	skr_buffer_t result = {};
	result.use  = use;
	result.type = type;

	D3D11_SUBRESOURCE_DATA buffer_data = { data };
	D3D11_BUFFER_DESC      buffer_desc = {};
	buffer_desc.ByteWidth = size_bytes;
	switch (type) {
	case skr_buffer_type_vertex:   buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;   break;
	case skr_buffer_type_index:    buffer_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;    break;
	case skr_buffer_type_constant: buffer_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER; break;
	default: throw "Not implemented yet!";
	}
	switch (use) {
	case skr_use_static:  buffer_desc.Usage = D3D11_USAGE_DEFAULT; break;
	case skr_use_dynamic: {
		buffer_desc.Usage          = D3D11_USAGE_DYNAMIC;
		buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	}break;
	default: throw "Not implemented yet!";
	}
	d3d_device->CreateBuffer(&buffer_desc, data==nullptr ? nullptr : &buffer_data, &result.buffer);
	return result;
}

/////////////////////////////////////////// 

void skr_buffer_update(skr_buffer_t *buffer, const void *data, uint32_t size_bytes) {
	if (buffer->use != skr_use_dynamic)
		return;

	D3D11_MAPPED_SUBRESOURCE resource;
	d3d_context->Map(buffer->buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource);
	memcpy(resource.pData, data, size_bytes);
	d3d_context->Unmap(buffer->buffer, 0);
}

/////////////////////////////////////////// 

void skr_buffer_set(const skr_buffer_t *buffer, uint32_t slot, uint32_t stride, uint32_t offset) {
	switch (buffer->type) {
	case skr_buffer_type_vertex:   d3d_context->IASetVertexBuffers  (slot, 1, &buffer->buffer, &stride, &offset); break;
	case skr_buffer_type_index:    d3d_context->IASetIndexBuffer    (buffer->buffer, DXGI_FORMAT_R32_UINT, offset); break;
	case skr_buffer_type_constant: d3d_context->VSSetConstantBuffers(slot, 1, &buffer->buffer); d3d_context->PSSetConstantBuffers(slot, 1, &buffer->buffer); break;
	}
}

/////////////////////////////////////////// 

void skr_buffer_destroy(skr_buffer_t *buffer) {
	if (buffer->buffer) buffer->buffer->Release();
	*buffer = {};
}

/////////////////////////////////////////// 

skr_mesh_t skr_mesh_create(const skr_buffer_t *vert_buffer, const skr_buffer_t *ind_buffer) {
	skr_mesh_t result = {};
	result.ind_buffer  = ind_buffer ->buffer;
	result.vert_buffer = vert_buffer->buffer;

	return result;
}

/////////////////////////////////////////// 

void skr_mesh_set(const skr_mesh_t *mesh) {
	UINT strides[] = { sizeof(skr_vert_t) };
	UINT offsets[] = { 0 };
	d3d_context->IASetVertexBuffers(0, 1, &mesh->vert_buffer, strides, offsets);
	d3d_context->IASetIndexBuffer  (mesh->ind_buffer, DXGI_FORMAT_R32_UINT, 0);
}

/////////////////////////////////////////// 

void skr_mesh_destroy(skr_mesh_t *mesh) {
	*mesh = {};
}

/////////////////////////////////////////// 

#include <stdio.h>
skr_shader_stage_t skr_shader_stage_create(const uint8_t *file_data, size_t shader_size, skr_shader_ type) {
	skr_shader_stage_t result = {};
	result.type = type;

	DWORD flags = D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR | D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS;
#ifdef _DEBUG
	flags |= D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_DEBUG;
#else
	flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

	ID3DBlob *compiled, *errors;
	if (FAILED(D3DCompile(file_data, shader_size, nullptr, nullptr, nullptr, type == skr_shader_pixel ? "ps" : "vs", type == skr_shader_pixel ? "ps_5_0" : "vs_5_0", flags, 0, &compiled, &errors)))
		printf("Error: D3DCompile failed %s", (char*)errors->GetBufferPointer());
	if (errors) errors->Release();

	switch(type) {
	case skr_shader_vertex: d3d_device->CreateVertexShader(compiled->GetBufferPointer(), compiled->GetBufferSize(), nullptr, (ID3D11VertexShader**)&result.shader); break;
	case skr_shader_pixel : d3d_device->CreatePixelShader (compiled->GetBufferPointer(), compiled->GetBufferSize(), nullptr, (ID3D11PixelShader **)&result.shader); break;
	}

	if (d3d_vert_layout == nullptr && type == skr_shader_vertex) {
		// Describe how our mesh is laid out in memory
		D3D11_INPUT_ELEMENT_DESC vert_desc[] = {
			{"SV_POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"NORMAL",      0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"TEXCOORD",    0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"COLOR" ,      0, DXGI_FORMAT_R8G8B8A8_UNORM,  0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},
			{"SV_RenderTargetArrayIndex" ,  0, DXGI_FORMAT_R8_UINT,  0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0} };
		d3d_device->CreateInputLayout(vert_desc, (UINT)_countof(vert_desc), compiled->GetBufferPointer(), compiled->GetBufferSize(), &d3d_vert_layout);
	}
	compiled->Release();

	return result;
}

/////////////////////////////////////////// 

void skr_shader_stage_destroy(skr_shader_stage_t *shader) {
	switch(shader->type) {
	case skr_shader_vertex: ((ID3D11VertexShader*)shader->shader)->Release(); break;
	case skr_shader_pixel : ((ID3D11PixelShader *)shader->shader)->Release(); break;
	}
}

/////////////////////////////////////////// 

skr_shader_t skr_shader_create(const skr_shader_stage_t *vertex, const skr_shader_stage_t *pixel) {
	skr_shader_t result = {};
	if (pixel) {
		result.pixel = (ID3D11PixelShader *)pixel->shader;
		result.pixel->AddRef();
	}
	if (vertex) {
		result.vertex = (ID3D11VertexShader *)vertex->shader;
		result.vertex->AddRef();
	}
	return result;
}

/////////////////////////////////////////// 

void skr_shader_set(const skr_shader_t *program) {
	d3d_context->VSSetShader(program->vertex, nullptr, 0);
	d3d_context->PSSetShader(program->pixel,  nullptr, 0);
}

/////////////////////////////////////////// 

void skr_shader_destroy(skr_shader_t *program) {
	if (program->pixel ) program->pixel ->Release();
	if (program->vertex) program->vertex->Release();
	*program = {};
}

/////////////////////////////////////////// 

skr_swapchain_t skr_swapchain_create(skr_tex_fmt_ format, skr_tex_fmt_ depth_format, int32_t width, int32_t height) {
	skr_swapchain_t result = {};
	result.width  = width;
	result.height = height;

	DXGI_SWAP_CHAIN_DESC1 swapchain_desc = { };
	swapchain_desc.BufferCount = 2;
	swapchain_desc.Width       = width;
	swapchain_desc.Height      = height;
	swapchain_desc.Format      = (DXGI_FORMAT)skr_tex_fmt_to_native(format);
	swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapchain_desc.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapchain_desc.SampleDesc.Count = 1;

	IDXGIDevice2  *dxgi_device;  d3d_device  ->QueryInterface(__uuidof(IDXGIDevice2),  (void **)&dxgi_device);
	IDXGIAdapter  *dxgi_adapter; dxgi_device ->GetParent     (__uuidof(IDXGIAdapter),  (void **)&dxgi_adapter);
	IDXGIFactory2 *dxgi_factory; dxgi_adapter->GetParent     (__uuidof(IDXGIFactory2), (void **)&dxgi_factory);

	dxgi_factory->CreateSwapChainForHwnd(d3d_device, (HWND)d3d_hwnd, &swapchain_desc, nullptr, nullptr, &result.d3d_swapchain);

	ID3D11Texture2D *back_buffer;
	result.d3d_swapchain->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
	result.target = skr_tex_from_native(back_buffer, skr_tex_type_rendertarget, format);
	result.depth  = skr_tex_create(skr_tex_type_depth, skr_use_static, depth_format, skr_mip_none);
	skr_tex_set_data(&result.depth, nullptr, 1, width, height);
	back_buffer->Release();

	dxgi_factory->Release();
	dxgi_adapter->Release();
	dxgi_device ->Release();

	return result;
}

/////////////////////////////////////////// 

void skr_swapchain_resize(skr_swapchain_t *swapchain, int32_t width, int32_t height) {
	if (swapchain->d3d_swapchain == nullptr || (width == swapchain->width && height == swapchain->height))
		return;

	skr_tex_fmt_ target_fmt = swapchain->target.format;
	skr_tex_fmt_ depth_fmt  = swapchain->depth .format;
	skr_tex_destroy(&swapchain->target);
	skr_tex_destroy(&swapchain->depth);

	swapchain->width  = width;
	swapchain->height = height;
	swapchain->d3d_swapchain->ResizeBuffers(0, (UINT)width, (UINT)height, DXGI_FORMAT_UNKNOWN, 0);

	ID3D11Texture2D *back_buffer;
	swapchain->d3d_swapchain->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
	swapchain->target = skr_tex_from_native(back_buffer, skr_tex_type_rendertarget, target_fmt);
	swapchain->depth  = skr_tex_create(skr_tex_type_depth, skr_use_static, depth_fmt, skr_mip_none);
	skr_tex_set_data(&swapchain->depth, nullptr, 1, width, height);
	back_buffer->Release();
}

/////////////////////////////////////////// 

void skr_swapchain_present(const skr_swapchain_t *swapchain) {
	swapchain->d3d_swapchain->Present(1, 0);
}

/////////////////////////////////////////// 

const skr_tex_t *skr_swapchain_get_target(const skr_swapchain_t *swapchain) {
	return swapchain->target.format != 0 ? &swapchain->target : nullptr;
}

/////////////////////////////////////////// 

const skr_tex_t *skr_swapchain_get_depth(const skr_swapchain_t *swapchain) {
	return swapchain->depth.format != 0 ? &swapchain->depth : nullptr;
}

/////////////////////////////////////////// 

void skr_swapchain_destroy(skr_swapchain_t *swapchain) {
	skr_tex_destroy(&swapchain->target);
	skr_tex_destroy(&swapchain->depth);
	swapchain->d3d_swapchain->Release();
	*swapchain = {};
}

/////////////////////////////////////////// 

skr_tex_t skr_tex_from_native(void *native_tex, skr_tex_type_ type, skr_tex_fmt_ override_format) {
	skr_tex_t result = {};
	result.type    = type;
	result.use     = skr_use_static;
	result.texture = (ID3D11Texture2D *)native_tex;
	result.texture->AddRef();

	// Get information about the image!
	D3D11_TEXTURE2D_DESC color_desc;
	result.texture->GetDesc(&color_desc);
	result.width  = color_desc.Width;
	result.height = color_desc.Height;
	result.format = override_format != 0 ? override_format : skr_d3d_to_tex_fmt(color_desc.Format);
	skr_tex_make_view(&result, color_desc.MipLevels, color_desc.ArraySize, color_desc.BindFlags & D3D11_BIND_SHADER_RESOURCE);

	return result;
}

/////////////////////////////////////////// 

skr_tex_t skr_tex_create(skr_tex_type_ type, skr_use_ use, skr_tex_fmt_ format, skr_mip_ mip_maps) {
	skr_tex_t result = {};
	result.type   = type;
	result.use    = use;
	result.format = format;
	result.mips   = mip_maps;

	if (use == skr_use_dynamic && mip_maps == skr_mip_generate)
		printf("Dynamic textures don't support mip-maps!");

	return result;
}

/////////////////////////////////////////// 

void skr_tex_settings(skr_tex_t *tex, skr_tex_address_ address, skr_tex_sample_ sample, int32_t anisotropy) {
	if (tex->sampler)
		tex->sampler->Release();

	D3D11_TEXTURE_ADDRESS_MODE mode;
	switch (address) {
	case skr_tex_address_clamp:  mode = D3D11_TEXTURE_ADDRESS_CLAMP;  break;
	case skr_tex_address_repeat: mode = D3D11_TEXTURE_ADDRESS_WRAP;   break;
	case skr_tex_address_mirror: mode = D3D11_TEXTURE_ADDRESS_MIRROR; break;
	default: mode = D3D11_TEXTURE_ADDRESS_WRAP;
	}

	D3D11_FILTER filter;
	switch (sample) {
	case skr_tex_sample_linear:     filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR; break; // Technically trilinear
	case skr_tex_sample_point:      filter = D3D11_FILTER_MIN_MAG_MIP_POINT;  break;
	case skr_tex_sample_anisotropic:filter = D3D11_FILTER_ANISOTROPIC;        break;
	default: filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	}

	D3D11_SAMPLER_DESC desc_sampler = {};
	desc_sampler.AddressU = mode;
	desc_sampler.AddressV = mode;
	desc_sampler.AddressW = mode;
	desc_sampler.Filter   = filter;
	desc_sampler.MaxAnisotropy  = anisotropy;
	desc_sampler.MaxLOD         = D3D11_FLOAT32_MAX;
	desc_sampler.ComparisonFunc = D3D11_COMPARISON_ALWAYS;

	// D3D will already return the same sampler when provided the same settings, so we
	// can just lean on that to prevent sampler duplicates :)
	if (FAILED(d3d_device->CreateSamplerState(&desc_sampler, &tex->sampler)))
		printf("skr_tex_settings: failed to create sampler state!");
}

///////////////////////////////////////////

void skr_make_mips(D3D11_SUBRESOURCE_DATA *tex_mem, void *curr_data, skr_tex_fmt_ format, int32_t width, int32_t height, uint32_t mip_levels) {
	void    *mip_data = curr_data;
	int32_t  mip_w    = width;
	int32_t  mip_h    = height;
	for (uint32_t m = 1; m < mip_levels; m++) {
		tex_mem[m] = {};
		switch (format) {
		case skr_tex_fmt_rgba32:
		case skr_tex_fmt_rgba32_linear: 
			skr_downsample_4((uint8_t  *)mip_data, mip_w, mip_h, (uint8_t  **)&tex_mem[m].pSysMem, &mip_w, &mip_h); 
			break;
		case skr_tex_fmt_rgba64:
			skr_downsample_4((uint16_t *)mip_data, mip_w, mip_h, (uint16_t **)&tex_mem[m].pSysMem, &mip_w, &mip_h);
			break;
		case skr_tex_fmt_rgba128:
			skr_downsample_4((float    *)mip_data, mip_w, mip_h, (float    **)&tex_mem[m].pSysMem, &mip_w, &mip_h);
			break;
		case skr_tex_fmt_depth32:
		case skr_tex_fmt_r32:
			skr_downsample_1((float    *)mip_data, mip_w, mip_h, (float    **)&tex_mem[m].pSysMem, &mip_w, &mip_h); 
			break;
		case skr_tex_fmt_depth16:
		case skr_tex_fmt_r16:
			skr_downsample_1((uint16_t *)mip_data, mip_w, mip_h, (uint16_t **)&tex_mem[m].pSysMem, &mip_w, &mip_h); 
			break;
		case skr_tex_fmt_r8:
			skr_downsample_1((uint8_t  *)mip_data, mip_w, mip_h, (uint8_t  **)&tex_mem[m].pSysMem, &mip_w, &mip_h); 
			break;
		}
		mip_data = (void*)tex_mem[m].pSysMem;
		tex_mem[m].SysMemPitch = (UINT)(skr_tex_fmt_size(format) * mip_w);
	}
}

///////////////////////////////////////////

bool skr_tex_make_view(skr_tex_t *tex, uint32_t mip_count, uint32_t array_size, bool use_in_shader) {
	DXGI_FORMAT format = (DXGI_FORMAT)skr_tex_fmt_to_native(tex->format);

	if (tex->type != skr_tex_type_depth) {
		D3D11_SHADER_RESOURCE_VIEW_DESC res_desc = {};
		res_desc.Format = format;
		if (tex->type == skr_tex_type_cubemap) {
			res_desc.TextureCube.MipLevels = mip_count;
			res_desc.ViewDimension         = D3D11_SRV_DIMENSION_TEXTURECUBE;
		} else if (array_size > 1) {
			res_desc.Texture2DArray.MipLevels = mip_count;
			res_desc.Texture2DArray.ArraySize = array_size;
			res_desc.ViewDimension            = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
		} else {
			res_desc.Texture2D.MipLevels = mip_count;
			res_desc.ViewDimension       = D3D11_SRV_DIMENSION_TEXTURE2D;
		}

		if (use_in_shader && FAILED(d3d_device->CreateShaderResourceView(tex->texture, &res_desc, &tex->resource))) {
			printf("Create Shader Resource View error!");
			return false;
		}
	} else {
		D3D11_DEPTH_STENCIL_VIEW_DESC stencil_desc = {};
		stencil_desc.Format = format;
		if (tex->type == skr_tex_type_cubemap || array_size > 1) {
			stencil_desc.Texture2DArray.ArraySize = array_size;
			stencil_desc.ViewDimension            = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
		} else {
			stencil_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		}
		if (FAILED(d3d_device->CreateDepthStencilView(tex->texture, &stencil_desc, &tex->depth_view))) {
			printf("Create Depth Stencil View error!");
			return false;
		}
	}

	if (tex->type == skr_tex_type_rendertarget) {
		D3D11_RENDER_TARGET_VIEW_DESC target_desc = {};
		target_desc.Format = format;
		if (tex->type == skr_tex_type_cubemap || array_size > 1) {
			target_desc.Texture2DArray.ArraySize = array_size;
			target_desc.ViewDimension            = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
		} else {
			target_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		}

		if (FAILED(d3d_device->CreateRenderTargetView(tex->texture, &target_desc, &tex->target_view))) {
			printf("Create Render Target View error!");
			return false;
		}
	}
	return true;
}

///////////////////////////////////////////

void skr_tex_set_data(skr_tex_t *tex, void **data_frames, int32_t data_frame_count, int32_t width, int32_t height) {
	// Some warning messages
	if (tex->use != skr_use_dynamic && tex->texture) {
		printf("Only dynamic textures can be updated!");
		return;
	}
	if (tex->use == skr_use_dynamic && (tex->mips == skr_mip_generate || data_frame_count > 1)) {
		printf("Dynamic textures don't support mip-maps or texture arrays!");
		return;
	}

	tex->width  = width;
	tex->height = height;

	uint32_t mip_levels = (tex->mips == skr_mip_generate ? log2(width) + 1 : 1);
	uint32_t px_size    = skr_tex_fmt_size(tex->format);

	if (tex->texture == nullptr) {
		D3D11_TEXTURE2D_DESC desc = {};
		desc.Width            = width;
		desc.Height           = height;
		desc.MipLevels        = mip_levels;
		desc.ArraySize        = 1;
		desc.SampleDesc.Count = 1;
		desc.Format           = (DXGI_FORMAT)skr_tex_fmt_to_native(tex->format);
		desc.BindFlags        = tex->type == skr_tex_type_depth ? D3D11_BIND_DEPTH_STENCIL : D3D11_BIND_SHADER_RESOURCE;
		desc.Usage            = tex->use  == skr_use_dynamic    ? D3D11_USAGE_DYNAMIC      : D3D11_USAGE_DEFAULT;
		desc.CPUAccessFlags   = tex->use  == skr_use_dynamic    ? D3D11_CPU_ACCESS_WRITE   : 0;
		if (tex->type == skr_tex_type_rendertarget) desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
		if (tex->type == skr_tex_type_cubemap     ) desc.MiscFlags  = D3D11_RESOURCE_MISC_TEXTURECUBE;

		D3D11_SUBRESOURCE_DATA *tex_mem = nullptr;
		if (data_frames != nullptr && data_frames[0] != nullptr) {
			tex_mem = (D3D11_SUBRESOURCE_DATA *)malloc(data_frame_count * mip_levels * sizeof(D3D11_SUBRESOURCE_DATA));
			for (int32_t i = 0; i < data_frame_count; i++) {
				tex_mem[i*mip_levels] = {};
				tex_mem[i*mip_levels].pSysMem     = data_frames[i];
				tex_mem[i*mip_levels].SysMemPitch = (UINT)(px_size * width);

				if (tex->mips == skr_mip_generate) {
					skr_make_mips(&tex_mem[i*mip_levels], data_frames[i], tex->format, width, height, mip_levels);
				}
			}
		}

		if (FAILED(d3d_device->CreateTexture2D(&desc, tex_mem, &tex->texture))) {
			printf("Create texture error!");
		}

		if (tex_mem != nullptr) {
			for (int32_t i = 0; i < data_frame_count; i++) {
				for (uint32_t m = 1; m < mip_levels; m++) {
					free((void*)tex_mem[i*mip_levels + m].pSysMem);
				} 
			}
			free(tex_mem);
		}

		skr_tex_make_view(tex, mip_levels, data_frame_count, true);
	} else {
		// For dynamic textures, just upload the new value into the texture!
		D3D11_MAPPED_SUBRESOURCE tex_mem = {};
		if (FAILED(d3d_context->Map(tex->texture, 0, D3D11_MAP_WRITE_DISCARD, 0, &tex_mem))) {
			printf("Failed mapping a texture");
			return;
		}

		uint8_t *dest_line  = (uint8_t *)tex_mem.pData;
		uint8_t *src_line   = (uint8_t *)data_frames[0];
		for (int i = 0; i < height; i++) {
			memcpy(dest_line, src_line, (size_t)width * px_size);
			dest_line += tex_mem.RowPitch;
			src_line  += (UINT)(px_size * width);
		}
		d3d_context->Unmap(tex->texture, 0);
	}

	// If the sampler has not been set up yet, we'll make a default one real quick.
	if (tex->sampler == nullptr) {
		skr_tex_settings(tex, skr_tex_address_repeat, skr_tex_sample_linear, 0);
	}
}

/////////////////////////////////////////// 

void skr_tex_set_active(const skr_tex_t *texture, int32_t slot) {
	if (texture != nullptr) {
		d3d_context->PSSetSamplers       (slot, 1, &texture->sampler);
		d3d_context->PSSetShaderResources(slot, 1, &texture->resource);
	} else {
		d3d_context->PSSetShaderResources(slot, 1, nullptr);
	}
}

/////////////////////////////////////////// 

void skr_tex_destroy(skr_tex_t *tex) {
	if (tex->target_view) tex->target_view->Release();
	if (tex->depth_view ) tex->depth_view ->Release();
	if (tex->resource) tex->resource->Release();
	if (tex->sampler ) tex->sampler ->Release();
	if (tex->texture ) tex->texture ->Release();
}

/////////////////////////////////////////// 

template <typename T>
void skr_downsample_4(T *data, int32_t width, int32_t height, T **out_data, int32_t *out_width, int32_t *out_height) {
	int w = (int32_t)log2(width);
	int h = (int32_t)log2(height);
	*out_width  = w = (1 << w) >> 1;
	*out_height = h = (1 << h) >> 1;

	*out_data = (T*)malloc(w * h * sizeof(T) * 4);
	memset(*out_data, 0, w * h * sizeof(T) * 4);
	T *result = *out_data;

	for (int32_t y = 0; y < height; y++) {
		int32_t src_row_start  = y * width;
		int32_t dest_row_start = (y / 2) * w;
		for (int32_t x = 0; x < width;  x++) {
			int src  = (x + src_row_start)*4;
			int dest = ((x / 2) + dest_row_start)*4;
			T *cD = &result[dest];
			T *cS = &data  [src];

			cD[0] += cS[0] / 4;
			cD[1] += cS[1] / 4;
			cD[2] += cS[2] / 4;
			cD[3] += cS[3] / 4;
		}
	}
}

/////////////////////////////////////////// 

template <typename T>
void skr_downsample_1(T *data, int32_t width, int32_t height, T **out_data, int32_t *out_width, int32_t *out_height) {
	int w = (int32_t)log2(width);
	int h = (int32_t)log2(height);
	*out_width  = w = (1 << w) >> 1;
	*out_height = h = (1 << h) >> 1;

	*out_data = (T*)malloc(w * h * sizeof(T));
	memset(*out_data, 0, w * h * sizeof(T));
	T *result = *out_data;

	for (int32_t y = 0; y < height; y++) {
		int32_t src_row_start  = y * width;
		int32_t dest_row_start = (y / 2) * w;
		for (int32_t x = 0; x < width;  x++) {
			int src  = (x + src_row_start);
			int dest = ((x / 2) + dest_row_start);
			result[dest] = result[dest] + (data[src] / 4);
		}
	}
}

/////////////////////////////////////////// 

size_t skr_el_to_size(skr_fmt_ desc) {
	switch (desc) {
	case skr_fmt_f32_1: return sizeof(float)*1;
	case skr_fmt_f32_2: return sizeof(float)*2;
	case skr_fmt_f32_3: return sizeof(float)*3;
	case skr_fmt_f32_4: return sizeof(float)*4;

	case skr_fmt_f16_1: return sizeof(uint16_t)*1;
	case skr_fmt_f16_2: return sizeof(uint16_t)*2;
	case skr_fmt_f16_4: return sizeof(uint16_t)*4;

	case skr_fmt_i32_1: return sizeof(int32_t)*1;
	case skr_fmt_i32_2: return sizeof(int32_t)*2;
	case skr_fmt_i32_3: return sizeof(int32_t)*3;
	case skr_fmt_i32_4: return sizeof(int32_t)*4;

	case skr_fmt_i16_1: return sizeof(int16_t)*1;
	case skr_fmt_i16_2: return sizeof(int16_t)*2;
	case skr_fmt_i16_4: return sizeof(int16_t)*4;

	case skr_fmt_i8_1: return sizeof(int8_t)*1;
	case skr_fmt_i8_2: return sizeof(int8_t)*2;
	case skr_fmt_i8_4: return sizeof(int8_t)*4;

	case skr_fmt_ui32_1: return sizeof(uint32_t)*1;
	case skr_fmt_ui32_2: return sizeof(uint32_t)*2;
	case skr_fmt_ui32_3: return sizeof(uint32_t)*3;
	case skr_fmt_ui32_4: return sizeof(uint32_t)*4;

	case skr_fmt_ui16_1: return sizeof(uint16_t)*1;
	case skr_fmt_ui16_2: return sizeof(uint16_t)*2;
	case skr_fmt_ui16_4: return sizeof(uint16_t)*4;

	case skr_fmt_ui8_1: return sizeof(uint8_t)*1;
	case skr_fmt_ui8_2: return sizeof(uint8_t)*2;
	case skr_fmt_ui8_4: return sizeof(uint8_t)*4;

	case skr_fmt_ui16_n_1: return sizeof(uint16_t)*1;
	case skr_fmt_ui16_n_2: return sizeof(uint16_t)*2;
	case skr_fmt_ui16_n_4: return sizeof(uint16_t)*4;

	case skr_fmt_ui8_n_1: return sizeof(uint8_t)*1;
	case skr_fmt_ui8_n_2: return sizeof(uint8_t)*2;
	case skr_fmt_ui8_n_4: return sizeof(uint8_t)*4;
	default: return 0;
	}
}

/////////////////////////////////////////// 

DXGI_FORMAT skr_el_to_d3d(skr_fmt_ desc) {
	switch (desc) {
	case skr_fmt_f32_1: return DXGI_FORMAT_R32_FLOAT;
	case skr_fmt_f32_2: return DXGI_FORMAT_R32G32_FLOAT;
	case skr_fmt_f32_3: return DXGI_FORMAT_R32G32B32_FLOAT;
	case skr_fmt_f32_4: return DXGI_FORMAT_R32G32B32A32_FLOAT;

	case skr_fmt_f16_1: return DXGI_FORMAT_R16_FLOAT;
	case skr_fmt_f16_2: return DXGI_FORMAT_R16G16_FLOAT;
	case skr_fmt_f16_4: return DXGI_FORMAT_R16G16B16A16_FLOAT;

	case skr_fmt_i32_1: return DXGI_FORMAT_R32_SINT;
	case skr_fmt_i32_2: return DXGI_FORMAT_R32G32_SINT;
	case skr_fmt_i32_3: return DXGI_FORMAT_R32G32B32_SINT;
	case skr_fmt_i32_4: return DXGI_FORMAT_R32G32B32A32_SINT;

	case skr_fmt_i16_1: return DXGI_FORMAT_R16_SINT;
	case skr_fmt_i16_2: return DXGI_FORMAT_R16G16_SINT;
	case skr_fmt_i16_4: return DXGI_FORMAT_R16G16B16A16_SINT;

	case skr_fmt_i8_1: return DXGI_FORMAT_R8_SINT;
	case skr_fmt_i8_2: return DXGI_FORMAT_R8G8_SINT;
	case skr_fmt_i8_4: return DXGI_FORMAT_R8G8B8A8_SINT;

	case skr_fmt_ui32_1: return DXGI_FORMAT_R32_UINT;
	case skr_fmt_ui32_2: return DXGI_FORMAT_R32G32_UINT;
	case skr_fmt_ui32_3: return DXGI_FORMAT_R32G32B32_UINT;
	case skr_fmt_ui32_4: return DXGI_FORMAT_R32G32B32A32_UINT;

	case skr_fmt_ui16_1: return DXGI_FORMAT_R16_UINT;
	case skr_fmt_ui16_2: return DXGI_FORMAT_R16G16_UINT;
	case skr_fmt_ui16_4: return DXGI_FORMAT_R16_UINT;

	case skr_fmt_ui8_1: return DXGI_FORMAT_R8_UINT;
	case skr_fmt_ui8_2: return DXGI_FORMAT_R8G8_UINT;
	case skr_fmt_ui8_4: return DXGI_FORMAT_R8G8B8A8_UINT;

	case skr_fmt_ui16_n_1: return DXGI_FORMAT_R16_UNORM;
	case skr_fmt_ui16_n_2: return DXGI_FORMAT_R16G16_UNORM;
	case skr_fmt_ui16_n_4: return DXGI_FORMAT_R16G16B16A16_UNORM;

	case skr_fmt_ui8_n_1: return DXGI_FORMAT_R8_UNORM;
	case skr_fmt_ui8_n_2: return DXGI_FORMAT_R8G8_UNORM;
	case skr_fmt_ui8_n_4: return DXGI_FORMAT_R8G8B8A8_UNORM;
	default: return DXGI_FORMAT_UNKNOWN;
	}
}

/////////////////////////////////////////// 

int64_t skr_tex_fmt_to_native(skr_tex_fmt_ format){
	switch (format) {
	case skr_tex_fmt_rgba32:        return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	case skr_tex_fmt_rgba32_linear: return DXGI_FORMAT_R8G8B8A8_UNORM;
	case skr_tex_fmt_bgra32:        return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
	case skr_tex_fmt_bgra32_linear: return DXGI_FORMAT_B8G8R8A8_UNORM;
	case skr_tex_fmt_rgba64:        return DXGI_FORMAT_R16G16B16A16_UNORM;
	case skr_tex_fmt_rgba128:       return DXGI_FORMAT_R32G32B32A32_FLOAT;
	case skr_tex_fmt_depth16:       return DXGI_FORMAT_D16_UNORM;
	case skr_tex_fmt_depth32:       return DXGI_FORMAT_D32_FLOAT;
	case skr_tex_fmt_depthstencil:  return DXGI_FORMAT_D24_UNORM_S8_UINT;
	case skr_tex_fmt_r8:            return DXGI_FORMAT_R8_UNORM;
	case skr_tex_fmt_r16:           return DXGI_FORMAT_R16_UNORM;
	case skr_tex_fmt_r32:           return DXGI_FORMAT_R32_FLOAT;
	default: return DXGI_FORMAT_UNKNOWN;
	}
}

/////////////////////////////////////////// 

skr_tex_fmt_ skr_d3d_to_tex_fmt(DXGI_FORMAT format) {
	switch (format) {
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return skr_tex_fmt_rgba32;
	case DXGI_FORMAT_R8G8B8A8_UNORM:      return skr_tex_fmt_rgba32_linear;
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return skr_tex_fmt_bgra32;
	case DXGI_FORMAT_B8G8R8A8_UNORM:      return skr_tex_fmt_bgra32_linear;
	case DXGI_FORMAT_R16G16B16A16_UNORM:  return skr_tex_fmt_rgba64;
	case DXGI_FORMAT_R32G32B32A32_FLOAT:  return skr_tex_fmt_rgba128;
	case DXGI_FORMAT_D16_UNORM:           return skr_tex_fmt_depth16;
	case DXGI_FORMAT_D32_FLOAT:           return skr_tex_fmt_depth32;
	case DXGI_FORMAT_D24_UNORM_S8_UINT:   return skr_tex_fmt_depthstencil;
	case DXGI_FORMAT_R8_UNORM:            return skr_tex_fmt_r8;
	case DXGI_FORMAT_R16_UNORM:           return skr_tex_fmt_r16;
	case DXGI_FORMAT_R32_FLOAT:           return skr_tex_fmt_r32;
	default: return skr_tex_fmt_none;
	}
}

/////////////////////////////////////////// 

uint32_t skr_tex_fmt_size(skr_tex_fmt_ format) {
	switch (format) {
	case skr_tex_fmt_rgba32:        return sizeof(uint8_t )*4;
	case skr_tex_fmt_rgba32_linear: return sizeof(uint8_t )*4;
	case skr_tex_fmt_bgra32:        return sizeof(uint8_t )*4;
	case skr_tex_fmt_bgra32_linear: return sizeof(uint8_t )*4;
	case skr_tex_fmt_rgba64:        return sizeof(uint16_t)*4;
	case skr_tex_fmt_rgba128:       return sizeof(uint32_t)*4;
	case skr_tex_fmt_depth16:       return sizeof(uint16_t);
	case skr_tex_fmt_depth32:       return sizeof(uint32_t);
	case skr_tex_fmt_depthstencil:  return sizeof(uint32_t);
	case skr_tex_fmt_r8:            return sizeof(uint8_t );
	case skr_tex_fmt_r16:           return sizeof(uint16_t);
	case skr_tex_fmt_r32:           return sizeof(uint32_t);
	default: return 0;
	}
}

/////////////////////////////////////////// 

const char *skr_semantic_to_d3d(skr_el_semantic_ semantic) {
	switch (semantic) {
	case skr_el_semantic_none: return "";
	case skr_el_semantic_position: return "SV_POSITION";
	case skr_el_semantic_normal: return "NORMAL";
	case skr_el_semantic_texcoord: return "TEXCOORD";
	case skr_el_semantic_color: return "COLOR";
	case skr_el_semantic_target_index: return "SV_RenderTargetArrayIndex";
	default: return "";
	}
}

#endif