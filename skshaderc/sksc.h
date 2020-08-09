// https://simoncoenen.com/blog/programming/graphics/DxcRevised.html
#pragma once

#include <stdint.h>

///////////////////////////////////////////

typedef struct sksc_settings_t {
	bool debug;
	bool row_major;
	int  optimize;
	wchar_t folder[512];
	const wchar_t *vs_entrypoint;
	const wchar_t *ps_entrypoint;
	const wchar_t *shader_model;
	wchar_t shader_model_str[16];
} sksc_settings_t;

typedef enum sksc_shader_type_ {
	sksc_shader_type_pixel,
	sksc_shader_type_vertex,
} sksc_shader_type_;

typedef struct sksc_param_t {
	uint8_t type;
	uint8_t slot;
	wchar_t name[32];
} sksc_param_t;

typedef struct sksc_stage_data_t {
	sksc_shader_type_ type;
	sksc_param_t     *params;
	int32_t           param_count;
	void             *binary;
	size_t            binary_size;
} sksc_stage_data_t;

typedef struct sksc_shader_t {
	sksc_stage_data_t *stages;
	int32_t            stage_count;
} sksc_shader_t;

///////////////////////////////////////////

void sksc_init    ();
void sksc_shutdown();
void sksc_compile (char *filename, char *hlsl_text, sksc_settings_t *settings);

///////////////////////////////////////////

#ifdef SKSC_IMPL

///////////////////////////////////////////

#pragma comment(lib,"dxcompiler.lib")

#include <windows.h>
#include <dxcapi.h>
#include <d3d12shader.h>
#include <spirv_cross/spirv_cross_c.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

///////////////////////////////////////////

template <typename T> struct array_t {
	T     *data;
	size_t count;
	size_t capacity;

	size_t      add        (const T &item)           { if (count+1 > capacity) { resize(capacity * 2 < 4 ? 4 : capacity * 2); } data[count] = item; count += 1; return count - 1; }
	void        insert     (size_t at, const T &item){ if (count+1 > capacity) resize(capacity<1?1:capacity*2); memmove(&data[at+1], &data[at], (count-at)*sizeof(T)); memcpy(&data[at], &item, sizeof(T)); count += 1;}
	void        trim       ()                        { resize(count); }
	void        remove     (size_t at)               { memmove(&data[at], &data[at+1], (count - (at + 1))*sizeof(T)); count -= 1; }
	void        pop        ()                        { remove(count - 1); }
	void        clear      ()                        { count = 0; }
	T          &last       () const                  { return data[count - 1]; }
	inline void set        (size_t id, const T &val) { data[id] = val; }
	inline T   &get        (size_t id) const         { return data[id]; }
	inline T   &operator[] (size_t id) const         { return data[id]; }
	void        reverse    ()                        { for(size_t i=0; i<count/2; i+=1) {T tmp = get(i);set(i, get(count-i-1));set(count-i-1, tmp);}};
	array_t<T>  copy       () const                  { array_t<T> result = {malloc(sizeof(T) * capacity),count,capacity}; memcpy(result.data, data, sizeof(T) * count); return result; }
	void        each       (void (*e)(T &))          { for (size_t i=0; i<count; i++) e(data[i]); }
	void        free       ()                        { ::free(data); *this = {}; }
	void        resize     (size_t to_capacity)      { if (count > to_capacity) count = to_capacity; void *old = data; void *new_mem = malloc(sizeof(T) * to_capacity); memcpy(new_mem, old, sizeof(T) * count); data = (T*)new_mem; ::free(old); capacity = to_capacity; }
	int64_t     index_of   (const T &item) const     { for (size_t i = 0; i < count; i++) if (memcmp(data[i], item, sizeof(T)) == 0) return i; return -1; }
	template <typename T, typename D>
	int64_t     index_of   (const D T::*key, const D &item) const { const size_t offset = (size_t)&((T*)0->*key); for (size_t i = 0; i < count; i++) if (memcmp(((uint8_t *)&data[i]) + offset, &item, sizeof(D)) == 0) return i; return -1; }
};

///////////////////////////////////////////

array_t<const wchar_t *> sksc_dxc_build_flags   (sksc_settings_t settings, sksc_shader_type_ type);
void                     sksc_dxc_shader_meta   (IDxcResult *compile_result, sksc_stage_data_t *out_stage);
bool                     sksc_dxc_compile_shader(DxcBuffer *source_buff, IDxcIncludeHandler *include_handler, sksc_settings_t *settings, sksc_shader_type_ type, sksc_stage_data_t *out_stage);

///////////////////////////////////////////

IDxcCompiler3 *sksc_compiler;
IDxcUtils     *sksc_utils;

///////////////////////////////////////////

void sksc_init() {
	DxcCreateInstance(CLSID_DxcCompiler, __uuidof(IDxcCompiler3), (void **)(&sksc_compiler));	
	DxcCreateInstance(CLSID_DxcUtils,    __uuidof(IDxcUtils),     (void **)(&sksc_utils));
}

///////////////////////////////////////////

void sksc_shutdown() {
	sksc_utils   ->Release();
	sksc_compiler->Release();
}

///////////////////////////////////////////

bool sksc_dxc_compile_shader(DxcBuffer *source_buff, IDxcIncludeHandler* include_handler, sksc_settings_t *settings, sksc_shader_type_ type, sksc_stage_data_t *out_stage) {
	IDxcResult   *compile_result;
	IDxcBlobUtf8 *errors;
	bool result = false;

	array_t<const wchar_t *> flags = sksc_dxc_build_flags(*settings, type);
	if (FAILED(sksc_compiler->Compile(source_buff, flags.data, flags.count, include_handler, __uuidof(IDxcResult), (void **)(&compile_result)))) {
		printf("|Compile failed!\n|________________\n");
		return false;
	}

	const char *type_name = type == sksc_shader_type_pixel ? "Pixel" : "Vertex";
	compile_result->GetOutput(DXC_OUT_ERRORS, __uuidof(IDxcBlobUtf8), (void **)(&errors), nullptr);
	if (errors && errors->GetStringLength() > 0) {
		printf((char*)errors->GetBufferPointer());
		printf("|%s shader error!\n|________________\n\n", type_name);
	} else {
		out_stage->type = type;

		// Get the shader binary
		IDxcBlob *shader_bin;
		compile_result->GetOutput(DXC_OUT_OBJECT, __uuidof(IDxcBlob), (void **)(&shader_bin), nullptr);
		out_stage->binary      = malloc(shader_bin->GetBufferSize());
		out_stage->binary_size = shader_bin->GetBufferSize();
		memcpy(out_stage->binary, shader_bin->GetBufferPointer(), shader_bin->GetBufferSize());
		shader_bin->Release();

		printf("|--%s shader--\n", type_name);
		sksc_dxc_shader_meta(compile_result, out_stage);
		result = true;
	}
	errors        ->Release();
	compile_result->Release();
	flags.free();

	return result;
}

///////////////////////////////////////////

void sksc_dxc_shader_meta(IDxcResult *compile_result, sksc_stage_data_t *out_stage) {
	// Get information about the shader!
	IDxcBlob *reflection;
	compile_result->GetOutput(DXC_OUT_REFLECTION,  __uuidof(IDxcBlob), (void **)(&reflection), nullptr);

	ID3D12ShaderReflection *shader_reflection;
	DxcBuffer               reflection_buffer;
	reflection_buffer.Ptr      = reflection->GetBufferPointer();
	reflection_buffer.Size     = reflection->GetBufferSize();
	reflection_buffer.Encoding = 0;
	sksc_utils->CreateReflection(&reflection_buffer, __uuidof(ID3D12ShaderReflection), (void **)(&shader_reflection));

	D3D12_SHADER_DESC desc;
	shader_reflection->GetDesc(&desc);
	out_stage->param_count = desc.BoundResources;
	out_stage->params      = (sksc_param_t*)malloc(sizeof(sksc_param_t) * desc.BoundResources);
	for (uint32_t i = 0; i < desc.BoundResources; i++) {
		D3D12_SHADER_INPUT_BIND_DESC bind_desc;
		shader_reflection->GetResourceBindingDesc(i, &bind_desc);

		out_stage->params[i].type = 0;
		out_stage->params[i].slot = bind_desc.BindPoint;
		switch (bind_desc.Type) {
		case D3D_SIT_CBUFFER: out_stage->params[i].type = 'b'; break;
		case D3D_SIT_TEXTURE: out_stage->params[i].type = 't'; break;
		case D3D_SIT_SAMPLER: out_stage->params[i].type = 's'; break;
		}
		swprintf_s(out_stage->params[i].name, _countof(out_stage->params[i].name), L"%s", bind_desc.Name);

		printf("|Param %u%u : %s\n", 
			out_stage->params[i].type, 
			out_stage->params[i].slot, 
			out_stage->params[i].name);
	}

	shader_reflection->Release();
	reflection       ->Release();
}

///////////////////////////////////////////

void sksc_compile(char *filename, char *hlsl_text, sksc_settings_t *settings) {
	printf(" ________________\n|Compiling...\n|%s\n\n", filename);

	IDxcBlobEncoding *source;
	if (FAILED(sksc_utils->CreateBlob(hlsl_text, (uint32_t)strlen(hlsl_text), CP_UTF8, &source)))
		printf("|CreateBlob failed!\n|________________\n\n");

	DxcBuffer source_buff;
	source_buff.Ptr      = source->GetBufferPointer();
	source_buff.Size     = source->GetBufferSize();
	source_buff.Encoding = 0;

	IDxcIncludeHandler* include_handler = nullptr;
	if (FAILED(sksc_utils->CreateDefaultIncludeHandler(&include_handler)))
		printf("|CreateDefaultIncludeHandler failed!\n|________________\n\n");

	sksc_stage_data_t vs_stage = {};
	if (!sksc_dxc_compile_shader(&source_buff, include_handler, settings, sksc_shader_type_vertex, &vs_stage)) {
		include_handler->Release();
		source         ->Release();
		return;
	}
	sksc_stage_data_t ps_stage = {};
	if (!sksc_dxc_compile_shader(&source_buff, include_handler, settings, sksc_shader_type_pixel, &ps_stage)) {
		include_handler->Release();
		source         ->Release();
		return;
	}

	// cleanup
	include_handler->Release();
	source         ->Release();

	printf("|\n|Success!\n|________________\n\n");
}

///////////////////////////////////////////

array_t<const wchar_t *> sksc_dxc_build_flags(sksc_settings_t settings, sksc_shader_type_ type) {
	// https://simoncoenen.com/blog/programming/graphics/DxcCompiling.html

	array_t<const wchar_t *> result = {};
	result.add(settings.row_major 
		? DXC_ARG_PACK_MATRIX_ROW_MAJOR 
		: DXC_ARG_PACK_MATRIX_COLUMN_MAJOR);

	// Debug vs. Release
	if (settings.debug) {
		result.add(DXC_ARG_DEBUG);
		result.add(L"-Qembed_debug");
		result.add(L"-Od");
	} else {
		result.add(L"-Qstrip_debug");
		result.add(L"-Qstrip_reflect");
		switch (settings.optimize) {
		case 0: result.add(L"O0"); break;
		case 1: result.add(L"O1"); break;
		case 2: result.add(L"O2"); break;
		case 3: result.add(L"O3"); break;
		}
	}

	// Entrypoint
	result.add(L"-E"); 
	switch (type) {
	case sksc_shader_type_pixel:  result.add(settings.ps_entrypoint); break;
	case sksc_shader_type_vertex: result.add(settings.vs_entrypoint); break;
	}

	// Target
	result.add(L"-T");
	switch (type) {
	case sksc_shader_type_pixel:  wsprintf(settings.shader_model_str, L"ps_%s", settings.shader_model); result.add(settings.shader_model_str); break;
	case sksc_shader_type_vertex: wsprintf(settings.shader_model_str, L"vs_%s", settings.shader_model); result.add(settings.shader_model_str); break;
	}

	// Include folder
	result.add(L"-I");
	result.add(settings.folder);

	return result;
}

#endif