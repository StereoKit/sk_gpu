
///////////////////////////////////////////

#pragma comment(lib,"dxcompiler.lib")
#pragma comment(lib,"spirv-cross-c.lib")
#pragma comment(lib,"spirv-cross-core.lib")
#pragma comment(lib,"spirv-cross-cpp.lib")
#pragma comment(lib,"spirv-cross-c-shared.lib")
#pragma comment(lib,"spirv-cross-glsl.lib")
#pragma comment(lib,"spirv-cross-hlsl.lib")
#pragma comment(lib,"spirv-cross-msl.lib")
#pragma comment(lib,"spirv-cross-reflect.lib")
#pragma comment(lib,"spirv-cross-util.lib")

#include "sksc.h"

#define SKR_DIRECT3D11
#define SKR_IMPL
#include "../sk_gpu.h"

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

array_t<const wchar_t *> sksc_dxc_build_flags   (sksc_settings_t settings, sksc_shader_type_ type, sksc_shader_lang_ lang);
void                     sksc_dxc_shader_meta   (IDxcResult *compile_result, sksc_stage_data_t *out_stage);
bool                     sksc_dxc_compile_shader(DxcBuffer *source_buff, IDxcIncludeHandler *include_handler, sksc_settings_t *settings, sksc_shader_type_ type, sksc_shader_lang_ lang, sksc_stage_data_t *out_stage);

void sksc_spvc_compile_stage(const sksc_stage_data_t *src_stage, sksc_stage_data_t *out_stage);

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

void sksc_compile(char *filename, char *hlsl_text, sksc_settings_t *settings) {
	printf(" ________________\n| Compiling...\n| %s\n\n", filename);

	IDxcBlobEncoding *source;
	if (FAILED(sksc_utils->CreateBlob(hlsl_text, (uint32_t)strlen(hlsl_text), CP_UTF8, &source)))
		printf("| CreateBlob failed!\n|________________\n\n");

	DxcBuffer source_buff;
	source_buff.Ptr      = source->GetBufferPointer();
	source_buff.Size     = source->GetBufferSize();
	source_buff.Encoding = 0;

	IDxcIncludeHandler* include_handler = nullptr;
	if (FAILED(sksc_utils->CreateDefaultIncludeHandler(&include_handler)))
		printf("| CreateDefaultIncludeHandler failed!\n|________________\n\n");

	skr_shader_file_t shader_file = {};
	shader_file.stage_count = 6;
	shader_file.stages      = (skr_shader_file_stage_t*)malloc(6 * sizeof(skr_shader_file_stage_t));

	sksc_stage_data_t vs_stage_hlsl = {};
	if (!sksc_dxc_compile_shader(&source_buff, include_handler, settings, sksc_shader_type_vertex, sksc_shader_lang_hlsl, &vs_stage_hlsl)) {
		include_handler->Release();
		source         ->Release();
		return;
	}
	sksc_stage_data_t ps_stage_hlsl = {};
	if (!sksc_dxc_compile_shader(&source_buff, include_handler, settings, sksc_shader_type_pixel, sksc_shader_lang_hlsl, &ps_stage_hlsl)) {
		include_handler->Release();
		source         ->Release();
		return;
	}
	sksc_stage_data_t vs_stage_spirv = {};
	if (!sksc_dxc_compile_shader(&source_buff, include_handler, settings, sksc_shader_type_vertex, sksc_shader_lang_spirv, &vs_stage_spirv)) {
		include_handler->Release();
		source         ->Release();
		return;
	}
	sksc_stage_data_t ps_stage_spirv = {};
	if (!sksc_dxc_compile_shader(&source_buff, include_handler, settings, sksc_shader_type_pixel, sksc_shader_lang_spirv, &ps_stage_spirv)) {
		include_handler->Release();
		source         ->Release();
		return;
	}
	
	// cleanup
	include_handler->Release();
	source         ->Release();

	sksc_stage_data_t vs_stage_glsl = {};
	sksc_spvc_compile_stage(&vs_stage_spirv, &vs_stage_glsl);
	sksc_stage_data_t ps_stage_glsl = {};
	sksc_spvc_compile_stage(&ps_stage_spirv, &ps_stage_glsl);

	printf("|\n| Success!\n|________________\n\n");
}

///////////////////////////////////////////

bool sksc_dxc_compile_shader(DxcBuffer *source_buff, IDxcIncludeHandler* include_handler, sksc_settings_t *settings, sksc_shader_type_ type, sksc_shader_lang_ lang, sksc_stage_data_t *out_stage) {
	IDxcResult   *compile_result;
	IDxcBlobUtf8 *errors;
	bool result = false;

	array_t<const wchar_t *> flags = sksc_dxc_build_flags(*settings, type, lang);
	if (FAILED(sksc_compiler->Compile(source_buff, flags.data, flags.count, include_handler, __uuidof(IDxcResult), (void **)(&compile_result)))) {
		printf("| Compile failed!\n|________________\n");
		return false;
	}

	const char *lang_name = lang == sksc_shader_lang_hlsl  ? "HLSL"  : "SPIRV";
	const char *type_name = type == sksc_shader_type_pixel ? "Pixel" : "Vertex";
	compile_result->GetOutput(DXC_OUT_ERRORS, __uuidof(IDxcBlobUtf8), (void **)(&errors), nullptr);
	if (errors && errors->GetStringLength() > 0) {
		printf((char*)errors->GetBufferPointer());
		printf("| %s shader error!\n|________________\n\n", type_name);
	} else {
		out_stage->type = type;
		out_stage->lang = lang;

		// Get the shader binary
		IDxcBlob *shader_bin;
		compile_result->GetOutput(DXC_OUT_OBJECT, __uuidof(IDxcBlob), (void **)(&shader_bin), nullptr);

		void  *src  = shader_bin->GetBufferPointer();
		size_t size = shader_bin->GetBufferSize();
		out_stage->binary      = malloc(size);
		out_stage->binary_size = size;
		memcpy(out_stage->binary, src, size);
		shader_bin->Release();

		if (lang == sksc_shader_lang_hlsl) {
			printf("|--%s %s shader--\n", lang_name, type_name);
			sksc_dxc_shader_meta(compile_result, out_stage);
		}
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
	out_stage->buffer_count = desc.ConstantBuffers;
	out_stage->buffers      = (sksc_buffer_t*)malloc(sizeof(sksc_buffer_t) * out_stage->buffer_count);
	for (uint32_t i = 0; i < desc.ConstantBuffers; i++) {
		ID3D12ShaderReflectionConstantBuffer *cb = shader_reflection->GetConstantBufferByIndex(i);
		D3D12_SHADER_BUFFER_DESC              shader_buff;
		cb->GetDesc(&shader_buff);
		
		sprintf_s(out_stage->buffers[i].name, _countof(out_stage->buffers[i].name), "%s", shader_buff.Name);
		out_stage->buffers[i].size = shader_buff.Size;

		printf("| --%s : %u bytes--\n", out_stage->buffers[i].name, shader_buff.Size);

		for (size_t v = 0; v < shader_buff.Variables; v++) {
			ID3D12ShaderReflectionVariable *var  = cb->GetVariableByIndex(v);
			ID3D12ShaderReflectionType     *type = var->GetType();
			D3D12_SHADER_TYPE_DESC          type_desc;
			D3D12_SHADER_VARIABLE_DESC      var_desc;
			type->GetDesc(&type_desc);
			var ->GetDesc(&var_desc );

			printf("|   %-8s %-15s: +%-4u [%u]\n", type_desc.Name, var_desc.Name, var_desc.StartOffset, var_desc.Size);
		}
	}
	for (uint32_t i = 0; i < desc.BoundResources; i++) {
		D3D12_SHADER_INPUT_BIND_DESC bind_desc;
		shader_reflection->GetResourceBindingDesc(i, &bind_desc);
		
		if (bind_desc.Type == D3D_SIT_CBUFFER) {
			for (size_t b = 0; b < out_stage->buffer_count; b++) {
				if (strcmp(bind_desc.Name, out_stage->buffers[b].name) == 0) {
					out_stage->buffers[b].slot = bind_desc.BindPoint;

					printf("| Param b%u : %s\n",  
						out_stage->buffers[b].slot, 
						out_stage->buffers[b].name);
					break;
				}
			}
		}
		/*switch (bind_desc.Type) {
		case D3D_SIT_CBUFFER: out_stage->buffers[i].type = 'b'; break;
		case D3D_SIT_TEXTURE: out_stage->buffers[i].type = 't'; break;
		case D3D_SIT_SAMPLER: out_stage->buffers[i].type = 's'; break;
		}
		swprintf_s(out_stage->buffers[i].name, _countof(out_stage->buffers[i].name), L"%s", bind_desc.Name);

		printf("| Param %c%u : %s\n", 
			out_stage->buffers[i].type, 
			out_stage->buffers[i].slot, 
			out_stage->buffers[i].name);*/
	}

	shader_reflection->Release();
	reflection       ->Release();
}

///////////////////////////////////////////

array_t<const wchar_t *> sksc_dxc_build_flags(sksc_settings_t settings, sksc_shader_type_ type, sksc_shader_lang_ lang) {
	// https://simoncoenen.com/blog/programming/graphics/DxcCompiling.html

	array_t<const wchar_t *> result = {};
	if (lang == sksc_shader_lang_spirv) {
		result.add(L"-spirv");
		result.add(L"-fspv-reflect");
	}
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

///////////////////////////////////////////

void sksc_spvc_compile_stage(const sksc_stage_data_t *src_stage, sksc_stage_data_t *out_stage) {
	spvc_context context = nullptr;
	spvc_context_create            (&context);
	spvc_context_set_error_callback( context, [](void *userdata, const char *error) {
		printf("spvc err: %s\n", error);
	}, nullptr);

	spvc_compiler  compiler_glsl = nullptr;
	spvc_parsed_ir ir            = nullptr;
	spvc_context_parse_spirv    (context, (const SpvId*)src_stage->binary, src_stage->binary_size/sizeof(SpvId), &ir);
	spvc_context_create_compiler(context, SPVC_BACKEND_GLSL, ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &compiler_glsl);

	spvc_resources resources = nullptr;
	spvc_compiler_create_shader_resources(compiler_glsl, &resources);

	const char *lang_name = "GLSL";
	const char *type_name = src_stage->type == sksc_shader_type_pixel ? "Pixel" : "Vertex";
	printf("|--%s %s shader--\n", lang_name, type_name);

	const spvc_reflected_resource *list = nullptr;
	size_t                         count;
	spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_UNIFORM_BUFFER, &list, &count);
	for (size_t i = 0; i < count; i++) {
		printf("| Param b%u : %s\n", spvc_compiler_get_decoration(compiler_glsl, list[i].id, SpvDecorationBinding), spvc_compiler_get_name(compiler_glsl, list[i].id));
	}
	spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_SEPARATE_SAMPLERS, &list, &count);
	for (size_t i = 0; i < count; i++) {
		printf("| Param s%u : %s\n", spvc_compiler_get_decoration(compiler_glsl, list[i].id, SpvDecorationBinding), list[i].name);
	}
	spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_SEPARATE_IMAGE, &list, &count);
	for (size_t i = 0; i < count; i++) {
		printf("| Param t%u : %s\n", spvc_compiler_get_decoration(compiler_glsl, list[i].id, SpvDecorationBinding), list[i].name);
	}

	// Modify options.
	spvc_compiler_options options = nullptr;
	spvc_compiler_create_compiler_options(compiler_glsl, &options);
	spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_GLSL_VERSION, 300);
	spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_GLSL_ES, SPVC_TRUE);
	spvc_compiler_install_compiler_options(compiler_glsl, options);

	// combiner samplers/textures for OpenGL/ES
	spvc_compiler_build_combined_image_samplers(compiler_glsl);

	const char *result = nullptr;
	spvc_compiler_compile(compiler_glsl, &result);

	out_stage->type        = src_stage->type;
	out_stage->lang        = sksc_shader_lang_glsl;
	out_stage->binary_size = strlen(result) + 1;
	out_stage->binary      = malloc(out_stage->binary_size);
	strcpy_s((char*)out_stage->binary, out_stage->binary_size, result);

	// Frees all memory we allocated so far.
	spvc_context_destroy(context);
}