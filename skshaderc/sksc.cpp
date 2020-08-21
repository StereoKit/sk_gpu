
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
	int64_t     find_index (bool (*c)(const T &item, void *user_data), void *user_data) const { for (size_t i=0; i<count; i++) if (c(data[i], user_data)) return i; return -1;}
};

///////////////////////////////////////////

array_t<const wchar_t *> sksc_dxc_build_flags   (sksc_settings_t settings, skr_shader_ type, skr_shader_lang_ lang);
void                     sksc_dxc_shader_meta   (IDxcResult *compile_result, skr_shader_ stage, skr_shader_meta_t *out_meta);
bool                     sksc_dxc_compile_shader(DxcBuffer *source_buff, IDxcIncludeHandler *include_handler, sksc_settings_t *settings, skr_shader_ type, skr_shader_lang_ lang, skr_shader_file_stage_t *out_stage, skr_shader_meta_t *out_meta);

void sksc_spvc_compile_stage(const skr_shader_file_stage_t *src_stage, skr_shader_file_stage_t *out_stage);

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

bool sksc_compile(char *filename, char *hlsl_text, sksc_settings_t *settings, skr_shader_file_t *out_file) {
	printf(" ________________\n| Compiling...\n| %s\n\n", filename);

	IDxcBlobEncoding *source;
	if (FAILED(sksc_utils->CreateBlob(hlsl_text, (uint32_t)strlen(hlsl_text), CP_UTF8, &source))) {
		printf("| CreateBlob failed!\n|________________\n\n");
		return false;
	}

	DxcBuffer source_buff;
	source_buff.Ptr      = source->GetBufferPointer();
	source_buff.Size     = source->GetBufferSize();
	source_buff.Encoding = 0;

	IDxcIncludeHandler* include_handler = nullptr;
	if (FAILED(sksc_utils->CreateDefaultIncludeHandler(&include_handler))) {
		printf("| CreateDefaultIncludeHandler failed!\n|________________\n\n");
		return false;
	}

	*out_file = {};
	 out_file->stage_count = 6;
	 out_file->stages      = (skr_shader_file_stage_t*)malloc(sizeof(skr_shader_file_stage_t) * 6);
	 out_file->meta        = (skr_shader_meta_t      *)malloc(sizeof(skr_shader_meta_t));
	*out_file->stages      = {};
	*out_file->meta        = {};

	if (!sksc_dxc_compile_shader(&source_buff, include_handler, settings, skr_shader_vertex, skr_shader_lang_hlsl,  &out_file->stages[0], out_file->meta) ||
		!sksc_dxc_compile_shader(&source_buff, include_handler, settings, skr_shader_pixel,  skr_shader_lang_hlsl,  &out_file->stages[1], out_file->meta) ||
		!sksc_dxc_compile_shader(&source_buff, include_handler, settings, skr_shader_vertex, skr_shader_lang_spirv, &out_file->stages[2], nullptr)          ||
		!sksc_dxc_compile_shader(&source_buff, include_handler, settings, skr_shader_pixel,  skr_shader_lang_spirv, &out_file->stages[3], nullptr)) {
		include_handler->Release();
		source         ->Release();
		return false;
	}
	
	// cleanup
	include_handler->Release();
	source         ->Release();

	sksc_spvc_compile_stage(&out_file->stages[2], &out_file->stages[4]);
	sksc_spvc_compile_stage(&out_file->stages[3], &out_file->stages[5]);

	// Write out our reflection information
	printf("|--Buffer Info--\n");
	for (size_t i = 0; i < out_file->meta->buffer_count; i++) {
		skr_shader_meta_buffer_t *buff = &out_file->meta->buffers[i];
		printf("|  %s : %u bytes\n", buff->name, buff->size);
		for (size_t v = 0; v < buff->var_count; v++) {
			skr_shader_meta_var_t *var = &buff->vars[v];
			printf("|    %-15s: +%-4u [%u]\n", var->name, var->offset, var->size);
		}
	}
	skr_shader_ stages[2] = { skr_shader_vertex, skr_shader_pixel };
	for (size_t s = 0; s < sizeof(stages)/sizeof(skr_shader_); s++) {
		const char *stage_name = stages[s] == skr_shader_pixel ? "Pixel" : "Vertex";
		printf("|--%s Shader--\n", stage_name);
		for (size_t i = 0; i < out_file->meta->buffer_count; i++) {
			skr_shader_meta_buffer_t *buff = &out_file->meta->buffers[i];
			if (buff->used_by_bits & stages[s]) {
				printf("|  b%u : %s\n", buff->slot, buff->name);
			}
		}
		for (size_t i = 0; i < out_file->meta->texture_count; i++) {
			skr_shader_meta_texture_t *tex = &out_file->meta->textures[i];
			if (tex->used_by_bits & stages[s]) {
				printf("|  s%u : %s\n", tex->slot, tex->name );
			}
		}
	}

	printf("|\n| Success!\n|________________\n\n");
	return true;
}

///////////////////////////////////////////

void sksc_save(char *filename, const skr_shader_file_t *file) {
	FILE *fp;
	if (fopen_s(&fp, filename, "wb") != 0 || fp == nullptr) {
		return;
	}

	fwrite("SKSHADER", 8, 1, fp);
	uint16_t version = 1;
	fwrite(&version, sizeof(version), 1, fp);

	fwrite(&file->stage_count,         sizeof(file->stage_count        ), 1, fp);
	fwrite( file->meta->name,          sizeof(file->meta->name         ), 1, fp);
	fwrite(&file->meta->buffer_count,  sizeof(file->meta->buffer_count ), 1, fp);
	fwrite(&file->meta->texture_count, sizeof(file->meta->texture_count), 1, fp);

	for (size_t i = 0; i < file->meta->buffer_count; i++) {
		skr_shader_meta_buffer_t *buff = &file->meta->buffers[i];
		fwrite( buff->name,         sizeof(buff->name     ), 1, fp);
		fwrite(&buff->slot,         sizeof(buff->slot     ), 1, fp);
		fwrite(&buff->used_by_bits, sizeof(uint16_t       ), 1, fp);
		fwrite(&buff->size,         sizeof(buff->size     ), 1, fp);
		fwrite(&buff->var_count,    sizeof(buff->var_count), 1, fp);
		//fwrite(&buff->defaults,     buff->size,              1, fp);

		for (uint32_t t = 0; t < buff->var_count; t++) {
			skr_shader_meta_var_t *var = &buff->vars[t];
			fwrite(var->name,    sizeof(var->name),   1, fp);
			fwrite(var->extra,   sizeof(var->extra),  1, fp);
			fwrite(&var->offset, sizeof(var->offset), 1, fp);
			fwrite(&var->size,   sizeof(var->size),   1, fp);
		}
	}

	for (uint32_t i = 0; i < file->meta->texture_count; i++) {
		skr_shader_meta_texture_t *tex = &file->meta->textures[i];
		fwrite( tex->name,          sizeof(tex->name        ), 1, fp); 
		fwrite( tex->extra,         sizeof(tex->extra       ), 1, fp); 
		fwrite(&tex->slot,          sizeof(tex->slot        ), 1, fp); 
		fwrite(&tex->used_by_bits,  sizeof(tex->used_by_bits), 1, fp); 
	}

	for (uint32_t i = 0; i < file->stage_count; i++) {
		skr_shader_file_stage_t *stage = &file->stages[i];
		fwrite(&stage->language,  sizeof(stage->language ), 1, fp);
		fwrite(&stage->stage,     sizeof(stage->stage    ), 1, fp);
		fwrite(&stage->code_size, sizeof(stage->code_size), 1, fp);
		fwrite( stage->code,      stage->code_size,         1, fp);
	}

	fclose(fp);
}

///////////////////////////////////////////

bool sksc_dxc_compile_shader(DxcBuffer *source_buff, IDxcIncludeHandler* include_handler, sksc_settings_t *settings, skr_shader_ type, skr_shader_lang_ lang, skr_shader_file_stage_t *out_stage, skr_shader_meta_t *out_meta) {
	IDxcResult   *compile_result;
	IDxcBlobUtf8 *errors;
	bool result = false;

	array_t<const wchar_t *> flags = sksc_dxc_build_flags(*settings, type, lang);
	if (FAILED(sksc_compiler->Compile(source_buff, flags.data, flags.count, include_handler, __uuidof(IDxcResult), (void **)(&compile_result)))) {
		printf("| Compile failed!\n|________________\n");
		return false;
	}

	const char *lang_name = lang == skr_shader_lang_hlsl  ? "HLSL"  : "SPIRV";
	const char *type_name = type == skr_shader_pixel      ? "Pixel" : "Vertex";
	compile_result->GetOutput(DXC_OUT_ERRORS, __uuidof(IDxcBlobUtf8), (void **)(&errors), nullptr);
	if (errors && errors->GetStringLength() > 0) {
		printf((char*)errors->GetBufferPointer());
		printf("| %s %s shader error!\n|________________\n\n", lang_name, type_name);
	} else {
		out_stage->stage    = type;
		out_stage->language = lang;

		// Get the shader binary
		IDxcBlob *shader_bin;
		compile_result->GetOutput(DXC_OUT_OBJECT, __uuidof(IDxcBlob), (void **)(&shader_bin), nullptr);

		void  *src  = shader_bin->GetBufferPointer();
		size_t size = shader_bin->GetBufferSize();
		out_stage->code      = malloc(size);
		out_stage->code_size = size;
		memcpy(out_stage->code, src, size);
		shader_bin->Release();

		if (lang == skr_shader_lang_hlsl && out_meta != nullptr) {
			sksc_dxc_shader_meta(compile_result, type, out_meta);
		}
		result = true;
	}
	errors        ->Release();
	compile_result->Release();
	flags.free();

	return result;
}

///////////////////////////////////////////

void sksc_dxc_shader_meta(IDxcResult *compile_result, skr_shader_ stage, skr_shader_meta_t *out_meta) {
	// Get information about the shader!
	IDxcBlob *reflection;
	compile_result->GetOutput(DXC_OUT_REFLECTION,  __uuidof(IDxcBlob), (void **)(&reflection), nullptr);

	array_t<skr_shader_meta_buffer_t> buffer_list = {};
	buffer_list.data     = out_meta->buffers;
	buffer_list.capacity = out_meta->buffer_count;
	buffer_list.count    = out_meta->buffer_count;
	array_t<skr_shader_meta_texture_t> texture_list = {};
	texture_list.data     = out_meta->textures;
	texture_list.capacity = out_meta->texture_count;
	texture_list.count    = out_meta->texture_count;

	ID3D12ShaderReflection *shader_reflection;
	DxcBuffer               reflection_buffer;
	reflection_buffer.Ptr      = reflection->GetBufferPointer();
	reflection_buffer.Size     = reflection->GetBufferSize();
	reflection_buffer.Encoding = 0;
	sksc_utils->CreateReflection(&reflection_buffer, __uuidof(ID3D12ShaderReflection), (void **)(&shader_reflection));
	D3D12_SHADER_DESC desc;
	shader_reflection->GetDesc(&desc);

	for (uint32_t i = 0; i < desc.BoundResources; i++) {
		D3D12_SHADER_INPUT_BIND_DESC bind_desc;
		shader_reflection->GetResourceBindingDesc(i, &bind_desc);
		
		if (bind_desc.Type == D3D_SIT_CBUFFER) {
			ID3D12ShaderReflectionConstantBuffer *cb = shader_reflection->GetConstantBufferByName(bind_desc.Name);
			D3D12_SHADER_BUFFER_DESC              shader_buff;
			cb->GetDesc(&shader_buff);

			// Find or create a buffer
			int64_t id = buffer_list.find_index([](auto &buff, void *data) { 
				return strcmp(buff.name, (char*)data) == 0; 
			}, (void*)bind_desc.Name);
			bool is_new = id == -1;
			if (is_new) id = buffer_list.add({});

			// flag it as used by this shader stage
			buffer_list[id].used_by_bits = (skr_shader_)(buffer_list[id].used_by_bits | stage);

			if (!is_new) continue;

			// Initialize the buffer with data from the shaders!
			sprintf_s(buffer_list[id].name, _countof(buffer_list[id].name), "%s", bind_desc.Name);
			buffer_list[id].slot      = bind_desc.BindPoint;
			buffer_list[id].size      = shader_buff.Size;
			buffer_list[id].var_count = shader_buff.Variables;
			buffer_list[id].vars      = (skr_shader_meta_var_t*)malloc(sizeof(skr_shader_meta_var_t) * buffer_list[i].var_count);
			*buffer_list[id].vars     = {};

			for (size_t v = 0; v < shader_buff.Variables; v++) {
				ID3D12ShaderReflectionVariable *var  = cb->GetVariableByIndex(v);
				ID3D12ShaderReflectionType     *type = var->GetType();
				D3D12_SHADER_TYPE_DESC          type_desc;
				D3D12_SHADER_VARIABLE_DESC      var_desc;
				type->GetDesc(&type_desc);
				var ->GetDesc(&var_desc );

				buffer_list[id].vars[v].size   = var_desc.Size;
				buffer_list[id].vars[v].offset = var_desc.StartOffset;
				sprintf_s(buffer_list[id].vars[v].name, _countof(buffer_list[id].vars[v].name), "%s", var_desc.Name);
			}
		} 
		if (bind_desc.Type == D3D_SIT_TEXTURE) {
			int64_t id = texture_list.find_index([](auto &tex, void *data) { 
				return strcmp(tex.name, (char*)data) == 0; 
			}, (void*)bind_desc.Name);
			if (id == -1)
				id = texture_list.add({});

			sprintf_s(texture_list[id].name, _countof(texture_list[id].name), "%s", bind_desc.Name);
			texture_list[id].used_by_bits = (skr_shader_)(texture_list[id].used_by_bits | stage);
			texture_list[id].slot         = bind_desc.BindPoint;
		}
	}

	buffer_list .trim();
	texture_list.trim();
	out_meta->buffers       = buffer_list .data;
	out_meta->buffer_count  = buffer_list .count;
	out_meta->textures      = texture_list.data;
	out_meta->texture_count = texture_list.count;

	shader_reflection->Release();
	reflection       ->Release();
}

///////////////////////////////////////////

array_t<const wchar_t *> sksc_dxc_build_flags(sksc_settings_t settings, skr_shader_ type, skr_shader_lang_ lang) {
	// https://simoncoenen.com/blog/programming/graphics/DxcCompiling.html

	array_t<const wchar_t *> result = {};
	if (lang == skr_shader_lang_spirv) {
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
	case skr_shader_pixel:  result.add(settings.ps_entrypoint); break;
	case skr_shader_vertex: result.add(settings.vs_entrypoint); break;
	}

	// Target
	result.add(L"-T");
	switch (type) {
	case skr_shader_pixel:  wsprintf(settings.shader_model_str, L"ps_%s", settings.shader_model); result.add(settings.shader_model_str); break;
	case skr_shader_vertex: wsprintf(settings.shader_model_str, L"vs_%s", settings.shader_model); result.add(settings.shader_model_str); break;
	}

	// Include folder
	result.add(L"-I");
	result.add(settings.folder);

	return result;
}

///////////////////////////////////////////

void sksc_spvc_compile_stage(const skr_shader_file_stage_t *src_stage, skr_shader_file_stage_t *out_stage) {
	spvc_context context = nullptr;
	spvc_context_create            (&context);
	spvc_context_set_error_callback( context, [](void *userdata, const char *error) {
		printf("spvc err: %s\n", error);
	}, nullptr);

	spvc_compiler  compiler_glsl = nullptr;
	spvc_parsed_ir ir            = nullptr;
	spvc_context_parse_spirv    (context, (const SpvId*)src_stage->code, src_stage->code_size/sizeof(SpvId), &ir);
	spvc_context_create_compiler(context, SPVC_BACKEND_GLSL, ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &compiler_glsl);

	spvc_resources resources = nullptr;
	spvc_compiler_create_shader_resources(compiler_glsl, &resources);

	/*const char *lang_name = "GLSL";
	const char *type_name = src_stage->stage == skr_shader_pixel ? "Pixel" : "Vertex";
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
	}*/

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

	out_stage->stage     = src_stage->stage;
	out_stage->language  = skr_shader_lang_glsl;
	out_stage->code_size = strlen(result) + 1;
	out_stage->code      = malloc(out_stage->code_size);
	strcpy_s((char*)out_stage->code, out_stage->code_size, result);

	// Frees all memory we allocated so far.
	spvc_context_destroy(context);
}