// A lot of reference from here:
// https://www.braynzarsoft.net/viewtutorial/q16390-04-direct3d-12-drawing
// https://www.3dgep.com/learning-directx-12-1/#Direct3D

#ifdef SKR_DIRECT3D12
#include "sk_gpu_dev.h"
///////////////////////////////////////////
// Direct3D12 Implementation             //
///////////////////////////////////////////

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")
#include <d3d12.h>
#include "d3dx12.h"
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

using namespace DirectX;

///////////////////////////////////////////

struct skr_fence_t {
	ID3D12Fence *fence;
	uint64_t     value;
	HANDLE       event;
};

struct Vertex {
	XMFLOAT3 pos;
	XMFLOAT4 color;
};
struct ConstantBuffer {
	XMFLOAT4X4 mvp;
	XMFLOAT4   color;
};

struct skr_vertex_desc_t {
	int32_t vert_size;
};

struct skr_mesh_t {
	uint32_t vert_count;
	uint32_t ind_count;
	skr_vertex_desc_t        vert_desc;
	ID3D12Resource          *vert_buffer;
	D3D12_VERTEX_BUFFER_VIEW vert_buffer_view;
	ID3D12Resource          *ind_buffer;
	D3D12_INDEX_BUFFER_VIEW  ind_buffer_view;
};

struct skr_tex_t {
	DXGI_FORMAT           format;
	ID3D12Resource       *resource;
	ID3D12DescriptorHeap *heap;
};

struct skr_shader_stage_t {
};

///////////////////////////////////////////

#define D3D_FRAME_COUNT 2

ID3D12Device2             *d3d_device      = nullptr;
ID3D12CommandQueue        *d3d_queue       = nullptr;
IDXGISwapChain4           *d3d_swapchain   = nullptr;
ID3D12GraphicsCommandList *d3d_cmd_list    = nullptr;
ID3D12DescriptorHeap      *d3d_heap        = nullptr;
HANDLE                     d3d_fence_event = nullptr;
ID3D12Resource            *d3d_depth_buffer= nullptr;
ID3D12DescriptorHeap      *d3d_depth_heap  = nullptr;
DXGI_FORMAT                d3d_rtarget_format = DXGI_FORMAT_R8G8B8A8_UNORM;
DXGI_FORMAT                d3d_depth_format   = DXGI_FORMAT_D32_FLOAT;
ID3D12Resource            *d3d_rtargets    [D3D_FRAME_COUNT];
ID3D12CommandAllocator    *d3d_allocator   [D3D_FRAME_COUNT];
skr_fence_t                d3d_fence       [D3D_FRAME_COUNT];
uint64_t                   d3d_fence_value [D3D_FRAME_COUNT];
uint32_t                   d3d_heap_size   = 0;
uint32_t                   d3d_frame_index = 0;
int32_t                    d3d_width       = 1280;
int32_t                    d3d_height      = 720;

skr_mesh_t app_mesh = {};

ID3D12PipelineState*     pipelineStateObject = nullptr;
ID3D12RootSignature*     rootSignature = nullptr;
D3D12_VIEWPORT           viewport = {};
D3D12_RECT               scissorRect = {};
ID3D12Resource*          vertexBuffer = nullptr;
D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};
ID3D12Resource*          indexBuffer = nullptr;
D3D12_INDEX_BUFFER_VIEW  indexBufferView = {};

ID3D12DescriptorHeap* mainDescriptorHeap[D3D_FRAME_COUNT]; // this heap will store the descripor to our constant buffer
ID3D12Resource* constantBufferUploadHeap[D3D_FRAME_COUNT];
UINT8 *cbColorMultiplierGPUAddress[D3D_FRAME_COUNT];

ConstantBuffer cbColorMultiplierData;

///////////////////////////////////////////

#include <stdio.h>
void d3d_log(const char *message) { printf(message); }

void d3d_memcpy_subresource(const D3D12_MEMCPY_DEST* dest, const D3D12_SUBRESOURCE_DATA* src, size_t row_size_bytes, uint32_t rows, uint32_t slices);
void d3d_resize_depth(int width, int height);

void d3d_mesh_create(skr_mesh_t &mesh, Vertex *verts, int32_t vert_count, uint32_t *inds, int32_t ind_count);
void d3d_mesh_draw(skr_mesh_t &mesh);
void d3d_mesh_set_verts(skr_mesh_t &mesh, skr_vertex_desc_t vert_desc, void *verts, int32_t vert_count);
void d3d_mesh_set_inds (skr_mesh_t &mesh, void *inds,  int32_t ind_count);

IDXGIAdapter1 *d3d_get_adapter  (IDXGIFactory4 *factory, void *adapter_id);
int32_t        d3d_create_device(void *app_hwnd, void *adapter_id, int32_t app_width, int32_t app_height);

skr_fence_t skr_fence_create    ();
void        skr_fence_destroy   (skr_fence_t &fence);
void        skr_fence_begin     (skr_fence_t &fence);
void        skr_fence_signal_end(const skr_fence_t &fence);
void        skr_fence_wait_end  (const skr_fence_t &fence);

///////////////////////////////////////////

void d3d_mesh_draw(skr_mesh_t &mesh) {
	d3d_cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	d3d_cmd_list->IASetVertexBuffers    (0, 1, &mesh.vert_buffer_view);
	d3d_cmd_list->IASetIndexBuffer      (&mesh.ind_buffer_view);
	d3d_cmd_list->DrawIndexedInstanced  (mesh.ind_count, 1, 0, 0, 0);
}

///////////////////////////////////////////

void d3d_mesh_create(skr_mesh_t &mesh, Vertex *verts, int32_t vert_count, uint32_t *inds, int32_t ind_count) {
	size_t vert_size   = sizeof(Vertex ) * vert_count;
	size_t ind_size    = sizeof(int32_t) * ind_count;
	size_t buffer_size = ind_size + vert_size;
	CD3DX12_HEAP_PROPERTIES upload_heap_props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC   upload_buff_desc  = CD3DX12_RESOURCE_DESC::Buffer(buffer_size);

	// Create upload buffer on CPU
	ID3D12Resource *upload_buffer;
	d3d_device->CreateCommittedResource(&upload_heap_props, D3D12_HEAP_FLAG_NONE, &upload_buff_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&upload_buffer));

	// Create vertex & index buffer on the GPU
	// HEAP_TYPE_DEFAULT is on GPU, we also initialize with COPY_DEST state
	// so we don't have to transition into this before copying into them
	CD3DX12_HEAP_PROPERTIES heap_props = CD3DX12_HEAP_PROPERTIES (D3D12_HEAP_TYPE_DEFAULT);
	CD3DX12_RESOURCE_DESC   vbuff_desc = CD3DX12_RESOURCE_DESC::Buffer(vert_size);
	CD3DX12_RESOURCE_DESC   ibuff_desc = CD3DX12_RESOURCE_DESC::Buffer(ind_size);
	d3d_device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &vbuff_desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS (&mesh.vert_buffer));
	d3d_device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &ibuff_desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS (&mesh.ind_buffer));

	// Create buffer views
	mesh.vert_buffer_view.BufferLocation = mesh.vert_buffer->GetGPUVirtualAddress();
	mesh.vert_buffer_view.SizeInBytes    = vert_size;
	mesh.vert_buffer_view.StrideInBytes  = sizeof(Vertex);

	mesh.ind_buffer_view.BufferLocation = mesh.ind_buffer->GetGPUVirtualAddress();
	mesh.ind_buffer_view.SizeInBytes    = ind_size;
	mesh.ind_buffer_view.Format         = DXGI_FORMAT_R32_UINT;

	// Copy data on CPU into the upload buffer
	void* p;
	upload_buffer->Map(0, nullptr, &p);
	memcpy(p,                       verts, vert_size);
	memcpy((uint8_t*)p + vert_size, inds,  ind_size );
	upload_buffer->Unmap(0, nullptr);

	// Copy data from upload buffer on CPU into the index/vertex buffer on 
	// the GPU
	d3d_cmd_list->CopyBufferRegion(mesh.vert_buffer, 0, upload_buffer, 0,         vert_size);
	d3d_cmd_list->CopyBufferRegion(mesh.ind_buffer,  0, upload_buffer, vert_size, ind_size );

	// Barriers, batch them together
	CD3DX12_RESOURCE_BARRIER barriers[2] = {
		CD3DX12_RESOURCE_BARRIER::Transition(mesh.vert_buffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER),
		CD3DX12_RESOURCE_BARRIER::Transition(mesh.ind_buffer,  D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDEX_BUFFER)
	};
	d3d_cmd_list->ResourceBarrier (2, barriers);
	//upload_buffer->Release();

	mesh.vert_count = vert_count;
	mesh.ind_count  = ind_count;
}

///////////////////////////////////////////

int32_t skr_init(const char *app_name, void *app_hwnd, void *adapter_id, int32_t app_width, int32_t app_height) {
	if (!d3d_create_device(app_hwnd, adapter_id, app_width, app_height)) {
		return -1;
	}
	
	// Create the backbuffer objects
	D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
	heap_desc.NumDescriptors = D3D_FRAME_COUNT;
	heap_desc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	heap_desc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	if (FAILED(d3d_device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&d3d_heap))))
		return -2;

	d3d_heap_size = d3d_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	D3D12_CPU_DESCRIPTOR_HANDLE heap_handle = d3d_heap->GetCPUDescriptorHandleForHeapStart();
	for (int i = 0; i < D3D_FRAME_COUNT; i++) {
		if (FAILED(d3d_swapchain->GetBuffer(i, IID_PPV_ARGS(&d3d_rtargets[i]))))
			return -3;

		d3d_device->CreateRenderTargetView(d3d_rtargets[i], nullptr, heap_handle);
		d3d_rtargets[i]->SetName(L"Swapchain/RenderTarget");
		heap_handle.ptr += INT64(1) * UINT64(d3d_heap_size); // See CD3DX12_CPU_DESCRIPTOR_HANDLE.Offset https://github.com/Microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12HelloWorld/src/HelloTriangle/d3dx12.h

		if (FAILED(d3d_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&d3d_allocator[i]))))
			return -4;
		d3d_allocator[i]->SetName(L"Swapchain/Allocator");

		d3d_fence[i] = skr_fence_create();
	}
	d3d_resize_depth(app_width, app_height);

	if (FAILED(d3d_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, d3d_allocator[0], nullptr, IID_PPV_ARGS(&d3d_cmd_list))))
		return -5;
	d3d_cmd_list->SetName(L"Command List");

	// Application initialization

	/////////////////////////
	// Command Buffers     //
	/////////////////////////

	// create a descriptor range (descriptor table) and fill it out
	// this is a range of descriptors inside a descriptor heap

	D3D12_DESCRIPTOR_RANGE  descriptorTableRanges[1]; // only one range right now
	descriptorTableRanges[0].RangeType                         = D3D12_DESCRIPTOR_RANGE_TYPE_CBV; // this is a range of constant buffer views (descriptors)
	descriptorTableRanges[0].NumDescriptors                    = 1; // we only have one constant buffer, so the range is only 1
	descriptorTableRanges[0].BaseShaderRegister                = 0; // start index of the shader registers in the range
	descriptorTableRanges[0].RegisterSpace                     = 0; // space 0. can usually be zero
	descriptorTableRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // this appends the range to the end of the root signature descriptor tables

	D3D12_ROOT_DESCRIPTOR_TABLE descriptorTable;
	descriptorTable.NumDescriptorRanges = _countof(descriptorTableRanges); // we only have one range
	descriptorTable.pDescriptorRanges   = &descriptorTableRanges[0]; // the pointer to the beginning of our ranges array

	D3D12_ROOT_PARAMETER  rootParameters[1]; // only one parameter right now
	rootParameters[0].ParameterType    = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // this is a descriptor table
	rootParameters[0].DescriptorTable  = descriptorTable; // this is our descriptor table for this root parameter
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // our pixel shader will be the only shader accessing this parameter for now

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.NumParameters = _countof(rootParameters);
	rootSignatureDesc.pParameters   = rootParameters;
	rootSignatureDesc.Flags         = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	ID3DBlob* signature;
	if (FAILED(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, nullptr))) {
		return 0;
	}
	if (FAILED(d3d_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&rootSignature)))) {
		return 0;
	}
	rootSignature->SetName(L"Root Signature");

	for (int i = 0; i < D3D_FRAME_COUNT; i++) {
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
		heapDesc.NumDescriptors = 1;
		heapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		heapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		if (FAILED(d3d_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mainDescriptorHeap[i]))))
			return 0;

		D3D12_HEAP_PROPERTIES heap_props = {};
		heap_props.Type                 = D3D12_HEAP_TYPE_UPLOAD;
		heap_props.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heap_props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heap_props.CreationNodeMask     = 1;
		heap_props.VisibleNodeMask      = 1;
		D3D12_RESOURCE_DESC res_desc = {};
		res_desc.Dimension        = D3D12_RESOURCE_DIMENSION_BUFFER;
		res_desc.Alignment        = 0;
		res_desc.Width            = 1024 * 64; // size of the resource heap. Must be a multiple of 64KB for single-textures and constant buffers
		res_desc.Height           = 1;
		res_desc.DepthOrArraySize = 1;
		res_desc.MipLevels        = 1;
		res_desc.Format           = DXGI_FORMAT_UNKNOWN;
		res_desc.Layout           = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		res_desc.Flags            = D3D12_RESOURCE_FLAG_NONE;
		res_desc.SampleDesc.Count   = 1;
		res_desc.SampleDesc.Quality = 0;
		if (FAILED(d3d_device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &res_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&constantBufferUploadHeap[i]))))
			return 0;

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
		cbvDesc.BufferLocation = constantBufferUploadHeap[i]->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = (sizeof(ConstantBuffer) + 255) & ~255;    // CB size is required to be 256-byte aligned.
		d3d_device->CreateConstantBufferView(&cbvDesc, mainDescriptorHeap[i]->GetCPUDescriptorHandleForHeapStart());
		constantBufferUploadHeap[i]->SetName(L"Constant Buffer Upload Resource Heap");

		D3D12_RANGE readRange = { 0, 0 };    // We do not intend to read from this resource on the CPU. (End is less than or equal to begin)
		constantBufferUploadHeap[i]->Map(0, &readRange, (void**)&cbColorMultiplierGPUAddress[i]);
		memcpy(cbColorMultiplierGPUAddress[i], &cbColorMultiplierData, sizeof(cbColorMultiplierData));
	}
	ZeroMemory(&cbColorMultiplierData, sizeof(cbColorMultiplierData));

	//////////////////////////
	// Shader compile       //
	//////////////////////////

	ID3DBlob* vertexShader;
	ID3DBlob* errorBuff;
	if (FAILED(D3DCompileFromFile(L"shader.hlsl", nullptr, nullptr, "vs", "vs_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0, &vertexShader, &errorBuff)))
	{
		OutputDebugStringA((char*)errorBuff->GetBufferPointer());
		return 0;
	}
	D3D12_SHADER_BYTECODE vertexShaderBytecode = {};
	vertexShaderBytecode.BytecodeLength  = vertexShader->GetBufferSize();
	vertexShaderBytecode.pShaderBytecode = vertexShader->GetBufferPointer();

	// compile pixel shader
	ID3DBlob* pixelShader;
	if (FAILED(D3DCompileFromFile(L"shader.hlsl", nullptr, nullptr, "ps", "ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0, &pixelShader, &errorBuff)))
	{
		OutputDebugStringA((char*)errorBuff->GetBufferPointer());
		return 0;
	}
	D3D12_SHADER_BYTECODE pixelShaderBytecode = {};
	pixelShaderBytecode.BytecodeLength  = pixelShader->GetBufferSize();
	pixelShaderBytecode.pShaderBytecode = pixelShader->GetBufferPointer();

	//////////////////////////
	// Pipeline state       //
	//////////////////////////

	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = {};
	inputLayoutDesc.NumElements        = _countof(inputLayout);
	inputLayoutDesc.pInputElementDescs = inputLayout;
	D3D12_RASTERIZER_DESC rasterizer_desc = {};
	rasterizer_desc.FillMode              = D3D12_FILL_MODE_SOLID;
	rasterizer_desc.CullMode              = D3D12_CULL_MODE_BACK;
	rasterizer_desc.FrontCounterClockwise = false;
	rasterizer_desc.DepthBias             = D3D12_DEFAULT_DEPTH_BIAS;
	rasterizer_desc.DepthBiasClamp        = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	rasterizer_desc.SlopeScaledDepthBias  = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	rasterizer_desc.DepthClipEnable       = true;
	rasterizer_desc.MultisampleEnable     = false;
	rasterizer_desc.AntialiasedLineEnable = false;
	rasterizer_desc.ForcedSampleCount     = 0;
	rasterizer_desc.ConservativeRaster    = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
	D3D12_BLEND_DESC blend_desc = {};
	blend_desc.AlphaToCoverageEnable  = false;
	blend_desc.IndependentBlendEnable = FALSE;
	const D3D12_RENDER_TARGET_BLEND_DESC rtblend_desc = {
		false, false,
		D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
		D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
		D3D12_LOGIC_OP_NOOP, D3D12_COLOR_WRITE_ENABLE_ALL,
	};
	for (int32_t i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; i++)
		blend_desc.RenderTarget[ i ] = rtblend_desc;
	D3D12_DEPTH_STENCIL_DESC depth_desc = {};
	depth_desc.DepthEnable      = true;
	depth_desc.DepthWriteMask   = D3D12_DEPTH_WRITE_MASK_ALL;
	depth_desc.DepthFunc        = D3D12_COMPARISON_FUNC_LESS; 
	depth_desc.StencilEnable    = false; 
	depth_desc.StencilReadMask  = D3D12_DEFAULT_STENCIL_READ_MASK;
	depth_desc.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK; 
	const D3D12_DEPTH_STENCILOP_DESC defaultStencilOp = { D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS };
	depth_desc.FrontFace        = defaultStencilOp;
	depth_desc.BackFace         = defaultStencilOp;
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {}; // a structure to define a pso
	psoDesc.InputLayout           = inputLayoutDesc; // the structure describing our input layout
	psoDesc.pRootSignature        = rootSignature; // the root signature that describes the input data this pso needs
	psoDesc.VS                    = vertexShaderBytecode; // structure describing where to find the vertex shader bytecode and how large it is
	psoDesc.PS                    = pixelShaderBytecode; // same as VS but for pixel shader
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; // type of topology we are drawing
	psoDesc.RTVFormats[0]         = d3d_rtarget_format; // format of the render target
	psoDesc.SampleDesc.Count      = 1; // must be the same sample description as the swapchain and depth/stencil buffer
	psoDesc.SampleMask            = 0xffffffff; // sample mask has to do with multi-sampling. 0xffffffff means point sampling is done
	psoDesc.RasterizerState       = rasterizer_desc; // a default rasterizer state.
	psoDesc.BlendState            = blend_desc; // a default blent state.
	psoDesc.NumRenderTargets      = 1; // we are only binding one render target
	psoDesc.DepthStencilState     = depth_desc;
	psoDesc.DSVFormat             = d3d_depth_format;
	if (FAILED(d3d_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineStateObject)))) {
		return 0;
	}
	pipelineStateObject->SetName(L"Pipeline State");

	
	Vertex vList[] = {
		{ {-0.5f,  0.5f, 0.5f}, {1.0f, 0.0f, 0.0f, 1.0f} },
		{ { 0.5f, -0.5f, 0.5f}, {0.0f, 1.0f, 0.0f, 1.0f} },
		{ {-0.5f, -0.5f, 0.5f}, {0.0f, 0.0f, 1.0f, 1.0f } },
		{ { 0.5f,  0.5f, 0.5f}, {1.0f, 0.0f, 1.0f, 1.0f } },
	};
	uint32_t iList[] = {
		0, 1, 2,
		0, 3, 1
	};
	int vBufferSize = sizeof(vList);
	int iBufferSize = sizeof(iList);

	d3d_mesh_create(app_mesh, vList, _countof(vList), iList, _countof(iList));

	//////////////////////////
	// Create vertex buffer //
	//////////////////////////

	/*D3D12_HEAP_PROPERTIES heap_props = {};
	heap_props.Type                 = D3D12_HEAP_TYPE_DEFAULT;
	heap_props.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heap_props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heap_props.CreationNodeMask     = 1;
	heap_props.VisibleNodeMask      = 1;
	// See: https://github.com/Microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12HelloWorld/src/HelloTriangle/d3dx12.h#L1751
	D3D12_RESOURCE_DESC res_desc = {};
	res_desc.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
	res_desc.Alignment          = 0;
	res_desc.Width              = vBufferSize;
	res_desc.Height             = 1;
	res_desc.DepthOrArraySize   = 1;
	res_desc.MipLevels          = 1;
	res_desc.Format             = DXGI_FORMAT_UNKNOWN;
	res_desc.SampleDesc.Count   = 1;
	res_desc.SampleDesc.Quality = 0;
	res_desc.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	res_desc.Flags              = D3D12_RESOURCE_FLAG_NONE;
	d3d_device->CreateCommittedResource( &heap_props, D3D12_HEAP_FLAG_NONE, &res_desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&vertexBuffer));
	vertexBuffer->SetName(L"Vertex Buffer Resource Heap");

	heap_props.Type = D3D12_HEAP_TYPE_UPLOAD;
	ID3D12Resource* vBufferUploadHeap;
	d3d_device->CreateCommittedResource( &heap_props, D3D12_HEAP_FLAG_NONE, &res_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vBufferUploadHeap));
	vBufferUploadHeap->SetName(L"Vertex Buffer Upload Resource Heap");

	// store vertex buffer in upload heap
	D3D12_SUBRESOURCE_DATA vertexData = {};
	vertexData.pData      = (uint8_t*)vList; // pointer to our vertex array
	vertexData.RowPitch   = vBufferSize; // size of all our triangle vertex data
	vertexData.SlicePitch = vBufferSize; // also the size of our triangle vertex data

	// UpdateSubresources, see: https://github.com/Microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12HelloWorld/src/HelloTriangle/d3dx12.h#L1893
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT pLayout = {};
	uint32_t num_rows;
	uint64_t row_size, req_size;
	d3d_device->GetCopyableFootprints(&vertexBuffer->GetDesc(), 0, 1, 0, &pLayout, &num_rows, &row_size, &req_size);
	uint8_t* data;
	vBufferUploadHeap->Map(0, nullptr, (void**)&data);
	D3D12_MEMCPY_DEST dest_data = { data + pLayout.Offset, pLayout.Footprint.RowPitch, SIZE_T(pLayout.Footprint.RowPitch) * SIZE_T(num_rows) };
	d3d_memcpy_subresource(&dest_data, &vertexData, static_cast<SIZE_T>(row_size), num_rows, pLayout.Footprint.Depth);
	vBufferUploadHeap->Unmap(0, nullptr);
	d3d_cmd_list->CopyBufferRegion(vertexBuffer, 0, vBufferUploadHeap, pLayout.Offset, pLayout.Footprint.Width);

	// transition the vertex buffer data from copy destination state to vertex buffer state
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type  = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource   = vertexBuffer;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	d3d_cmd_list->ResourceBarrier(1, &barrier);

	vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
	vertexBufferView.StrideInBytes  = sizeof(Vertex);
	vertexBufferView.SizeInBytes    = vBufferSize;
	
	/////////////////////////
	// Create index buffer //
	/////////////////////////

	// create default heap to hold index buffer
	heap_props.Type  = D3D12_HEAP_TYPE_DEFAULT;
	res_desc  .Width = iBufferSize;
	d3d_device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &res_desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&indexBuffer));
	indexBuffer->SetName(L"Index Buffer Resource Heap");

	heap_props.Type = D3D12_HEAP_TYPE_UPLOAD;
	ID3D12Resource* iBufferUploadHeap;
	d3d_device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &res_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&iBufferUploadHeap));
	iBufferUploadHeap->SetName(L"Index Buffer Upload Resource Heap");

	D3D12_SUBRESOURCE_DATA indexData = {};
	indexData.pData      = (uint8_t*)iList; 
	indexData.RowPitch   = iBufferSize;
	indexData.SlicePitch = iBufferSize;

	// UpdateSubresources, see: https://github.com/Microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12HelloWorld/src/HelloTriangle/d3dx12.h#L1893
	d3d_device->GetCopyableFootprints(&indexBuffer->GetDesc(), 0, 1, 0, &pLayout, &num_rows, &row_size, &req_size);
	iBufferUploadHeap->Map(0, nullptr, (void**)&data);
	dest_data = { data + pLayout.Offset, pLayout.Footprint.RowPitch, SIZE_T(pLayout.Footprint.RowPitch) * SIZE_T(num_rows) };
	d3d_memcpy_subresource(&dest_data, &indexData, static_cast<SIZE_T>(row_size), num_rows, pLayout.Footprint.Depth);
	iBufferUploadHeap->Unmap(0, nullptr);
	d3d_cmd_list->CopyBufferRegion(indexBuffer, 0, iBufferUploadHeap, pLayout.Offset, pLayout.Footprint.Width);

	// transition the vertex buffer data from copy destination state to vertex buffer state
	barrier.Transition.pResource  = indexBuffer;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_INDEX_BUFFER;
	d3d_cmd_list->ResourceBarrier(1, &barrier);

	indexBufferView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
	indexBufferView.Format         = DXGI_FORMAT_R32_UINT;
	indexBufferView.SizeInBytes    = iBufferSize;*/

	/////////////////////////
	// Wrap up             //
	/////////////////////////

	// Execute the vertex/index buffer resource uploads!
	d3d_cmd_list->Close();
	ID3D12CommandList* ppCommandLists[] = { d3d_cmd_list };
	d3d_queue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	// increment the fence value now, otherwise the buffer might not be uploaded by the time we start drawing
	skr_fence_signal_end(d3d_fence[d3d_frame_index]);

	d3d_width  = app_width;
	d3d_height = app_height;

	// Fill out the Viewport
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width    = d3d_width;
	viewport.Height   = d3d_height;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	// Fill out a scissor rect
	scissorRect.left   = 0;
	scissorRect.top    = 0;
	scissorRect.right  = d3d_width;
	scissorRect.bottom = d3d_height;
	return 1;
}

///////////////////////////////////////////

void skr_resize_swapchain(int32_t width, int32_t height) {
	if (width  < 1) width  = 1;
	if (height < 1) height = 1;
	if (width == d3d_width && height == d3d_height)
		return;
	d3d_width  = width;
	d3d_height = height;
	if (d3d_device == nullptr) return;

	// Flush. Wait until the graphics card isn't busy
	skr_fence_signal_end(d3d_fence[d3d_frame_index]);
	skr_fence_wait_end  (d3d_fence[d3d_frame_index]);

	// Release existing references to the swapchain
	for (int i=0; i<D3D_FRAME_COUNT; i++) {
		if (d3d_rtargets[i] != nullptr) d3d_rtargets[i]->Release();
	}
	d3d_frame_index = d3d_swapchain->GetCurrentBackBufferIndex();

	// resize the swapchain to the new size
	DXGI_SWAP_CHAIN_DESC swapchain_desc = {};
	d3d_swapchain->GetDesc(&swapchain_desc);
	d3d_swapchain->ResizeBuffers(D3D_FRAME_COUNT, d3d_width, d3d_height, swapchain_desc.BufferDesc.Format, swapchain_desc.Flags);
	
	// Recreate the swapchain buffer views
	d3d_heap_size = d3d_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	D3D12_CPU_DESCRIPTOR_HANDLE heap_handle = d3d_heap->GetCPUDescriptorHandleForHeapStart();
	for (int i = 0; i < D3D_FRAME_COUNT; i++) {
		if (FAILED(d3d_swapchain->GetBuffer(i, IID_PPV_ARGS(&d3d_rtargets[i]))))
			return;

		d3d_device->CreateRenderTargetView(d3d_rtargets[i], nullptr, heap_handle);
		heap_handle.ptr += INT64(1) * UINT64(d3d_heap_size); // See CD3DX12_CPU_DESCRIPTOR_HANDLE.Offset https://github.com/Microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12HelloWorld/src/HelloTriangle/d3dx12.h
	}

	d3d_resize_depth(d3d_width, d3d_height);

	viewport.Width     = d3d_width;
	viewport.Height    = d3d_height;
	scissorRect.right  = d3d_width;
	scissorRect.bottom = d3d_height;
}

///////////////////////////////////////////

void skr_shutdown() {

	// wait for the gpu to finish all frames
	for (int32_t i=0; i<D3D_FRAME_COUNT; i++) {
		skr_fence_signal_end(d3d_fence[i]);
		skr_fence_wait_end  (d3d_fence[i]);
	}

	if (pipelineStateObject != nullptr) pipelineStateObject->Release();
	if (rootSignature       != nullptr) rootSignature->Release();
	if (vertexBuffer        != nullptr) vertexBuffer->Release();
	if (indexBuffer         != nullptr) indexBuffer->Release();

	// get swapchain out of full screen before exiting
	BOOL is_fullscreen = false;
	d3d_swapchain->GetFullscreenState(&is_fullscreen, nullptr);
	if (is_fullscreen)
		d3d_swapchain->SetFullscreenState(false, nullptr);

	if (d3d_device    != nullptr) d3d_device->Release();
	if (d3d_swapchain != nullptr) d3d_swapchain->Release();
	if (d3d_queue     != nullptr) d3d_queue->Release();
	if (d3d_heap      != nullptr) d3d_heap->Release();
	if (d3d_cmd_list  != nullptr) d3d_cmd_list->Release();
	if (d3d_depth_buffer != nullptr) d3d_depth_buffer->Release();
	if (d3d_depth_heap   != nullptr) d3d_depth_heap  ->Release();

	for (int i=0; i<D3D_FRAME_COUNT; i++) {
		skr_fence_destroy(d3d_fence[i]);

		if (d3d_rtargets [i] != nullptr) d3d_rtargets [i]->Release();
		if (d3d_allocator[i] != nullptr) d3d_allocator[i]->Release();

		if (mainDescriptorHeap      [i] != nullptr) mainDescriptorHeap      [i]->Release();
		if (constantBufferUploadHeap[i] != nullptr) constantBufferUploadHeap[i]->Release();
	};
}

///////////////////////////////////////////

void skr_draw() {
	d3d_frame_index = d3d_swapchain->GetCurrentBackBufferIndex();
	skr_fence_wait_end(d3d_fence[d3d_frame_index]);
	skr_fence_begin   (d3d_fence[d3d_frame_index]);

	if (FAILED(d3d_allocator[d3d_frame_index]->Reset()) || 
		FAILED(d3d_cmd_list->Reset(d3d_allocator[d3d_frame_index], nullptr))) {
		// Running = false;
		printf("Fail start\n");
	}

	d3d_cmd_list->SetGraphicsRootSignature(rootSignature);
	d3d_cmd_list->RSSetViewports   (1, &viewport);
	d3d_cmd_list->RSSetScissorRects(1, &scissorRect);

	// Update the command buffer
	cbColorMultiplierData.color.x = cbColorMultiplierData.color.x > 1 ? 0 : cbColorMultiplierData.color.x + 0.01f;
	cbColorMultiplierData.color.y = cbColorMultiplierData.color.y > 1 ? 0 : cbColorMultiplierData.color.y + 0.01f;
	cbColorMultiplierData.color.z = cbColorMultiplierData.color.z > 1 ? 0 : cbColorMultiplierData.color.z + 0.01f;
	memcpy(cbColorMultiplierGPUAddress[d3d_frame_index], &cbColorMultiplierData, sizeof(cbColorMultiplierData));
	// set constant buffer descriptor heap
	ID3D12DescriptorHeap* descriptorHeaps[] = { mainDescriptorHeap[d3d_frame_index] };
	d3d_cmd_list->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
	d3d_cmd_list->SetGraphicsRootDescriptorTable(0, mainDescriptorHeap[d3d_frame_index]->GetGPUDescriptorHandleForHeapStart());


	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type  = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource   = d3d_rtargets[d3d_frame_index];
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	d3d_cmd_list->ResourceBarrier(1, &barrier);

	// See: https://github.com/Microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12HelloWorld/src/HelloTriangle/d3dx12.h#L1616
	D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = d3d_heap->GetCPUDescriptorHandleForHeapStart();
	rtv_handle.ptr = (SIZE_T)(rtv_handle.ptr + INT64(d3d_frame_index) * UINT64(d3d_heap_size));
	D3D12_CPU_DESCRIPTOR_HANDLE dsv_handle = d3d_depth_heap->GetCPUDescriptorHandleForHeapStart();
	const float clearColor[] = { 1.0f, 0.0f, 0.0f, 1.0f };
	d3d_cmd_list->OMSetRenderTargets(1, &rtv_handle, false, &dsv_handle);
	d3d_cmd_list->ClearRenderTargetView(rtv_handle, clearColor, 0, nullptr);
	d3d_cmd_list->ClearDepthStencilView(dsv_handle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	// draw triangle
	d3d_cmd_list->SetPipelineState(pipelineStateObject);

	d3d_mesh_draw(app_mesh);

	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_PRESENT;
	d3d_cmd_list->ResourceBarrier(1, &barrier);
	d3d_cmd_list->Close();

	ID3D12CommandList* command_lists[] = { d3d_cmd_list };
	d3d_queue->ExecuteCommandLists(_countof(command_lists), command_lists);
	skr_fence_signal_end(d3d_fence[d3d_frame_index]);

	if (FAILED(d3d_swapchain->Present(1, 0))) {
		printf("Fail end\n");
	}
}

///////////////////////////////////////////

void d3d_resize_depth(int width, int height) {
	if (d3d_depth_buffer != nullptr) d3d_depth_buffer->Release();
	if (d3d_depth_heap   != nullptr) d3d_depth_heap  ->Release();

	// Resize the depth buffer too
	D3D12_CLEAR_VALUE optimizedClearValue = {};
	optimizedClearValue.Format       = d3d_depth_format;
	optimizedClearValue.DepthStencil = { 1.0f, 0 };

	D3D12_HEAP_PROPERTIES depth_heap_props = {};
	depth_heap_props.Type                 = D3D12_HEAP_TYPE_DEFAULT;
	depth_heap_props.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	depth_heap_props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	depth_heap_props.CreationNodeMask     = 1;
	depth_heap_props.VisibleNodeMask      = 1;
	D3D12_RESOURCE_DESC res_desc = {};
	res_desc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	res_desc.Alignment        = 0;
	res_desc.Width            = width;
	res_desc.Height           = height;
	res_desc.DepthOrArraySize = 1;
	res_desc.MipLevels        = 1;
	res_desc.Format           = d3d_depth_format;
	res_desc.Layout           = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	res_desc.Flags            = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	res_desc.SampleDesc.Count   = 1;
	res_desc.SampleDesc.Quality = 0;
	d3d_device->CreateCommittedResource( &depth_heap_props, D3D12_HEAP_FLAG_NONE, &res_desc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &optimizedClearValue, IID_PPV_ARGS(&d3d_depth_buffer));
	d3d_depth_buffer->SetName(L"Depth/Buffer");

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	d3d_device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&d3d_depth_heap));
	d3d_depth_heap->SetName(L"Depth/Heap");

	D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
	dsv.Format             = d3d_depth_format;
	dsv.ViewDimension      = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsv.Texture2D.MipSlice = 0;
	dsv.Flags              = D3D12_DSV_FLAG_NONE;
	d3d_device->CreateDepthStencilView(d3d_depth_buffer, &dsv, d3d_depth_heap->GetCPUDescriptorHandleForHeapStart());
}

///////////////////////////////////////////

void d3d_memcpy_subresource(const D3D12_MEMCPY_DEST *dest, const D3D12_SUBRESOURCE_DATA *src, size_t row_size_bytes, uint32_t rows, uint32_t slices) {
	for (uint32_t s=0; s<slices; s++) {
		uint8_t* dest_slice = (uint8_t*)(dest->pData) + dest->SlicePitch * s;
		uint8_t* src_slice  = (uint8_t*)(src ->pData) + src ->SlicePitch * s;
		for (uint32_t r=0; r<rows; r++) {
			memcpy(dest_slice + dest->RowPitch * r,
				   src_slice  + src ->RowPitch * r,
				   row_size_bytes);
		}
	}
}

///////////////////////////////////////////

IDXGIAdapter1 *d3d_get_adapter(IDXGIFactory4 *factory, void *adapter_id) {
	// Find the graphics card matching the adapter_id, or pick the card with 
	// the highest amount of memory.
	IDXGIAdapter1 *curr_adapter  = nullptr;
	IDXGIAdapter1 *final_adapter = nullptr;
	size_t         max_memory    = 0;
	for (UINT i = 0; factory->EnumAdapters1(i, &curr_adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
		DXGI_ADAPTER_DESC1 adapter_desc;
		curr_adapter->GetDesc1(&adapter_desc);
		if ((adapter_desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) > 0) continue;
		if (FAILED(D3D12CreateDevice(curr_adapter, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr))) continue;

		bool matches_luid = adapter_id != nullptr ? memcmp(&adapter_desc.AdapterLuid, adapter_id, sizeof(LUID)) == 0 : false;
		if (matches_luid || max_memory < adapter_desc.DedicatedVideoMemory) {
			max_memory = adapter_desc.DedicatedVideoMemory;
			if (final_adapter != nullptr)
				final_adapter->Release();
			final_adapter = curr_adapter;
			curr_adapter  = nullptr;
			if (matches_luid) break;
		}
		if (curr_adapter != nullptr)
			curr_adapter->Release();
	}

	return final_adapter;
}

///////////////////////////////////////////

int32_t d3d_create_device(void *app_hwnd, void *adapter_id, int32_t app_width, int32_t app_height) {
	// Create 'The Factory'
	IDXGIFactory4 *factory;
	UINT           factory_flags = 0;
#if defined(_DEBUG)
	factory_flags = DXGI_CREATE_FACTORY_DEBUG;
#endif
	if (FAILED(CreateDXGIFactory2(factory_flags, IID_PPV_ARGS(&factory))))
		return -1;

	// Enable a debug layer for extra dev information!
#if defined(_DEBUG)
	// This causes an insanely fast leak
	ID3D12Debug *debugInterface;
	D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface));
	debugInterface->EnableDebugLayer();
#endif

	// Graphics Device
	IDXGIAdapter1 *adapter = d3d_get_adapter(factory, adapter_id);
	if (FAILED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&d3d_device))))
		return -2;
	adapter->Release();

	// Command Queue
	D3D12_COMMAND_QUEUE_DESC queue_desc = {};
	queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queue_desc.Type  = D3D12_COMMAND_LIST_TYPE_DIRECT;
	if (FAILED(d3d_device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&d3d_queue))))
		return -3;

	// Create the swapchain surface!
	DXGI_SWAP_CHAIN_DESC swapchain_desc = {};
	swapchain_desc.BufferCount  = D3D_FRAME_COUNT;
	swapchain_desc.BufferUsage  = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapchain_desc.SwapEffect   = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapchain_desc.OutputWindow = (HWND)app_hwnd; 
	swapchain_desc.Windowed     = true;
	swapchain_desc.BufferDesc.Width  = app_width;
	swapchain_desc.BufferDesc.Height = app_height;
	swapchain_desc.BufferDesc.Format = d3d_rtarget_format;
	swapchain_desc.SampleDesc.Count = 1;

	IDXGISwapChain* temp_swapchain;
	if (FAILED(factory->CreateSwapChain(d3d_queue, &swapchain_desc, &temp_swapchain)))
		return -4;
	d3d_swapchain   = (IDXGISwapChain4*)temp_swapchain;
	d3d_frame_index = d3d_swapchain->GetCurrentBackBufferIndex();

	// Done with the factory
	factory->Release();

	return 1;
}

///////////////////////////////////////////
// Fence                                 //
///////////////////////////////////////////

skr_fence_t skr_fence_create() {
	skr_fence_t result = {};
	result.event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (result.event == nullptr) {
		d3d_log("Failed to create fence event");
		return result;
	}
	if (FAILED(d3d_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&result.fence)))) {
		d3d_log("Failed to create fence");
		return result;
	}
	result.fence->SetName(L"Swapchain/Fence");
	result.value = 0;
	return result;
}

///////////////////////////////////////////

void skr_fence_destroy(skr_fence_t &fence) {
	if (fence.event != nullptr) CloseHandle(fence.event);
	if (fence.fence != nullptr) fence.fence->Release();
	fence = {};
}

///////////////////////////////////////////

void skr_fence_begin(skr_fence_t &fence) {
	fence.value += 1;
}

///////////////////////////////////////////

void skr_fence_signal_end(const skr_fence_t &fence) {
	d3d_queue->Signal(fence.fence, fence.value);
}

///////////////////////////////////////////

void skr_fence_wait_end(const skr_fence_t &fence) {
	if (fence.fence->GetCompletedValue() < fence.value) {
		fence.fence->SetEventOnCompletion(fence.value, fence.event);
		WaitForSingleObject(fence.event, INFINITE);
	}
}

///////////////////////////////////////////

/*void d3d_mesh_set_verts(skr_mesh_t &mesh, skr_vertex_desc_t vert_desc, void *verts, int32_t vert_count) {
	D3D12_HEAP_PROPERTIES heap_props = {};
	heap_props.Type             = D3D12_HEAP_TYPE_DEFAULT;
	heap_props.CreationNodeMask = 1;
	heap_props.VisibleNodeMask  = 1;
	// See: https://github.com/Microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12HelloWorld/src/HelloTriangle/d3dx12.h#L1751
	D3D12_RESOURCE_DESC res_desc = {};
	res_desc.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
	res_desc.Width              = vert_desc.vert_size * vert_count;
	res_desc.Height             = 1;
	res_desc.DepthOrArraySize   = 1;
	res_desc.MipLevels          = 1;
	res_desc.SampleDesc.Count   = 1;
	res_desc.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	d3d_device->CreateCommittedResource( &heap_props, D3D12_HEAP_FLAG_NONE, &res_desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&mesh.vert_buffer));
	mesh.vert_buffer->SetName(L"Vertex Buffer Resource Heap");

	heap_props.Type = D3D12_HEAP_TYPE_UPLOAD;
	ID3D12Resource *upload_heap;
	d3d_device->CreateCommittedResource( &heap_props, D3D12_HEAP_FLAG_NONE, &res_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&upload_heap));
	upload_heap->SetName(L"Vertex Buffer Upload Resource Heap");

	// store vertex buffer in upload heap
	D3D12_SUBRESOURCE_DATA vert_data = {};
	vert_data.pData      = verts;
	vert_data.RowPitch   = res_desc.Width;
	vert_data.SlicePitch = res_desc.Width;

	// UpdateSubresources, see: https://github.com/Microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12HelloWorld/src/HelloTriangle/d3dx12.h#L1893
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout = {};
	uint32_t num_rows;
	uint64_t row_size, req_size;
	d3d_device->GetCopyableFootprints(&mesh.vert_buffer->GetDesc(), 0, 1, 0, &layout, &num_rows, &row_size, &req_size);
	uint8_t* data;
	upload_heap->Map(0, nullptr, (void**)&data);
	D3D12_MEMCPY_DEST dest_data = { data + layout.Offset, layout.Footprint.RowPitch, layout.Footprint.RowPitch * num_rows };
	d3d_memcpy_subresource(&dest_data, &vert_data, row_size, num_rows, layout.Footprint.Depth);
	upload_heap->Unmap(0, nullptr);
	d3d_cmd_list->CopyBufferRegion(mesh.vert_buffer, 0, upload_heap, layout.Offset, layout.Footprint.Width);

	// transition the vertex buffer data from copy destination state to vertex buffer state
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Transition.pResource   = mesh.vert_buffer;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	d3d_cmd_list->ResourceBarrier(1, &barrier);

	mesh.vert_buffer_view.BufferLocation = mesh.vert_buffer->GetGPUVirtualAddress();
	mesh.vert_buffer_view.StrideInBytes  = vert_desc.vert_size;
	mesh.vert_buffer_view.SizeInBytes    = res_desc.Width;

	/////////////////////////
	// Create index buffer //
	/////////////////////////

	// UpdateSubresources, see: https://github.com/Microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12HelloWorld/src/HelloTriangle/d3dx12.h#L1893
	d3d_device->GetCopyableFootprints(&indexBuffer->GetDesc(), 0, 1, 0, &layout, &num_rows, &row_size, &req_size);
	iBufferUploadHeap->Map(0, nullptr, (void**)&data);
	dest_data = { data + layout.Offset, layout.Footprint.RowPitch, SIZE_T(layout.Footprint.RowPitch) * SIZE_T(num_rows) };
	d3d_memcpy_subresource(&dest_data, &indexData, static_cast<SIZE_T>(row_size), num_rows, layout.Footprint.Depth);
	iBufferUploadHeap->Unmap(0, nullptr);
	d3d_cmd_list->CopyBufferRegion(indexBuffer, 0, iBufferUploadHeap, layout.Offset, layout.Footprint.Width);

	// transition the vertex buffer data from copy destination state to vertex buffer state
	barrier.Transition.pResource  = indexBuffer;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_INDEX_BUFFER;
	d3d_cmd_list->ResourceBarrier(1, &barrier);

	indexBufferView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
	indexBufferView.Format         = DXGI_FORMAT_R32_UINT;
	indexBufferView.SizeInBytes    = iBufferSize;
}

///////////////////////////////////////////

void d3d_mesh_set_inds(skr_mesh_t &mesh, void *inds, int32_t ind_count) {
	D3D12_HEAP_PROPERTIES heap_props = {};
	heap_props.Type                 = D3D12_HEAP_TYPE_DEFAULT;
	heap_props.CreationNodeMask     = 1;
	heap_props.VisibleNodeMask      = 1;
	// See: https://github.com/Microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12HelloWorld/src/HelloTriangle/d3dx12.h#L1751
	D3D12_RESOURCE_DESC res_desc = {};
	res_desc.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
	res_desc.Width              = sizeof(uint32_t) * ind_count;
	res_desc.Height             = 1;
	res_desc.DepthOrArraySize   = 1;
	res_desc.MipLevels          = 1;
	res_desc.SampleDesc.Count   = 1;
	res_desc.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	d3d_device->CreateCommittedResource( &heap_props, D3D12_HEAP_FLAG_NONE, &res_desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&mesh.ind_buffer));
	mesh.ind_buffer->SetName(L"Vertex Buffer Resource Heap");

	heap_props.Type = D3D12_HEAP_TYPE_UPLOAD;
	ID3D12Resource *upload_heap;
	d3d_device->CreateCommittedResource( &heap_props, D3D12_HEAP_FLAG_NONE, &res_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&upload_heap));
	upload_heap->SetName(L"Vertex Buffer Upload Resource Heap");

	// store vertex buffer in upload heap
	D3D12_SUBRESOURCE_DATA ind_data = {};
	ind_data.pData      = inds;
	ind_data.RowPitch   = res_desc.Width;
	ind_data.SlicePitch = res_desc.Width;

	// UpdateSubresources, see: https://github.com/Microsoft/DirectX-Graphics-Samples/blob/master/Samples/Desktop/D3D12HelloWorld/src/HelloTriangle/d3dx12.h#L1893
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout = {};
	uint32_t num_rows;
	uint64_t row_size, req_size;
	d3d_device->GetCopyableFootprints(&mesh.ind_buffer->GetDesc(), 0, 1, 0, &layout, &num_rows, &row_size, &req_size);
	uint8_t* data;
	upload_heap->Map(0, nullptr, (void**)&data);
	D3D12_MEMCPY_DEST dest_data = { data + layout.Offset, layout.Footprint.RowPitch, layout.Footprint.RowPitch * num_rows };
	d3d_memcpy_subresource(&dest_data, &ind_data, row_size, num_rows, layout.Footprint.Depth);
	upload_heap->Unmap(0, nullptr);
	d3d_cmd_list->CopyBufferRegion(mesh.ind_buffer, 0, upload_heap, layout.Offset, layout.Footprint.Width);

	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Transition.pResource   = mesh.ind_buffer;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter  = D3D12_RESOURCE_STATE_INDEX_BUFFER;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	d3d_cmd_list->ResourceBarrier(1, &barrier);

	mesh.ind_buffer_view.BufferLocation = indexBuffer->GetGPUVirtualAddress();
	mesh.ind_buffer_view.Format         = DXGI_FORMAT_R32_UINT;
	mesh.ind_buffer_view.SizeInBytes    = res_desc.Width;
}*/

#endif
