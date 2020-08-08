// https://simoncoenen.com/blog/programming/graphics/DxcRevised.html

#pragma comment(lib,"dxcompiler.lib")
#include <windows.h>
#include <dxcapi.h>
#include <d3d12shader.h>
#include <spirv_cross/spirv_cross_c.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

///////////////////////////////////////////

typedef struct settings_t {
	bool debug;
	bool row_major;
	int  optimize;
	wchar_t folder[512];
	const wchar_t *vs_entrypoint;
	const wchar_t *ps_entrypoint;
	const wchar_t *shader_model;
	wchar_t shader_model_str[16];
} settings_t;

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

enum shader_type_ {
	shader_type_pixel,
	shader_type_vertex,
};

///////////////////////////////////////////

IDxcCompiler3 *compiler;
IDxcUtils     *utils;

///////////////////////////////////////////

void       get_folder    (char *filename, char *out_dest,  size_t dest_size);
bool       read_file     (char *filename, char **out_text, size_t *out_size);
void       iterate_files (char *input_name, settings_t *settings);
void       compile_file  (char *filename, char *hlsl_text, settings_t *settings);
settings_t check_settings(int32_t argc, char **argv);

array_t<const wchar_t *> dxc_build_flags    (settings_t settings, shader_type_ type);
void                     dxc_shader_meta    (IDxcResult *compile_result);
bool                     dxc_compiler_shader(DxcBuffer *source_buff, IDxcIncludeHandler *include_handler, settings_t *settings, shader_type_ type);

///////////////////////////////////////////

int main(int argc, char **argv) {
	settings_t settings = check_settings(argc, argv);

	DxcCreateInstance(CLSID_DxcCompiler, __uuidof(IDxcCompiler3), (void **)(&compiler));	
	DxcCreateInstance(CLSID_DxcUtils,    __uuidof(IDxcUtils),     (void **)(&utils));

	iterate_files(argv[argc - 1], &settings);

	utils   ->Release();
	compiler->Release();
}

///////////////////////////////////////////

settings_t check_settings(int32_t argc, char **argv) {
	settings_t result = {};
	result.debug         = true;
	result.optimize      = 3;
	result.ps_entrypoint = L"ps";
	result.vs_entrypoint = L"vs";
	result.shader_model  = L"6_0";

	// Get the inlcude folder
	char folder[512];
	get_folder(argv[argc-1], folder, sizeof(folder));
	mbstowcs_s(nullptr, result.folder, _countof(result.folder), folder, sizeof(folder));

	for (int32_t i=1; i<argc-1; i++) {

	}

	return result;
}

///////////////////////////////////////////

void iterate_files(char *input_name, settings_t *settings) {
	HANDLE           handle;
	WIN32_FIND_DATAA file_info;

	char folder[512] = {};
	get_folder(input_name, folder, sizeof(folder));

	if((handle = FindFirstFileA(input_name, &file_info)) != INVALID_HANDLE_VALUE) {
		do {
			char filename[1024];
			sprintf_s(filename, "%s%s", folder, file_info.cFileName);

			char  *file_text;
			size_t file_size;
			if (read_file(filename, &file_text, &file_size)) {
				compile_file(filename, file_text, settings);
				free(file_text);
			}
		} while(FindNextFileA(handle, &file_info));
		FindClose(handle);
	}
}

///////////////////////////////////////////

bool dxc_compiler_shader(DxcBuffer *source_buff, IDxcIncludeHandler* include_handler, settings_t *settings, shader_type_ type) {
	IDxcResult   *compile_result;
	IDxcBlobUtf8 *errors;
	bool result = false;

	array_t<const wchar_t *> flags = dxc_build_flags(*settings, type);
	if (FAILED(compiler->Compile(source_buff, flags.data, flags.count, include_handler, __uuidof(IDxcResult), (void **)(&compile_result)))) {
		printf("|Compile failed!\n|________________\n");
		return false;
	}

	const char *type_name = type == shader_type_pixel ? "Pixel" : "Vertex";
	compile_result->GetOutput(DXC_OUT_ERRORS, __uuidof(IDxcBlobUtf8), (void **)(&errors), nullptr);
	if (errors && errors->GetStringLength() > 0) {
		printf((char*)errors->GetBufferPointer());
		printf("|%s shader error!\n|________________\n\n", type_name);
	} else {
		printf("|--%s shader--\n", type_name);
		dxc_shader_meta(compile_result);
		result = true;
	}
	errors        ->Release();
	compile_result->Release();
	flags.free();

	return result;
}

///////////////////////////////////////////

void dxc_shader_meta(IDxcResult *compile_result) {
	// Get information about the shader!
	IDxcBlob *reflection;
	DxcBuffer reflection_buffer;
	compile_result->GetOutput(DXC_OUT_REFLECTION,  __uuidof(IDxcBlob), (void **)(&reflection), nullptr);
	reflection_buffer.Ptr      = reflection->GetBufferPointer();
	reflection_buffer.Size     = reflection->GetBufferSize();
	reflection_buffer.Encoding = 0;
	ID3D12ShaderReflection *shader_reflection;
	utils->CreateReflection(&reflection_buffer, __uuidof(ID3D12ShaderReflection), (void **)(&shader_reflection));

	D3D12_SHADER_DESC desc;
	shader_reflection->GetDesc(&desc);
	for (uint32_t i = 0; i < desc.BoundResources; i++) {
		D3D12_SHADER_INPUT_BIND_DESC bind_desc;
		shader_reflection->GetResourceBindingDesc(i, &bind_desc);

		char c = ' ';
		switch (bind_desc.Type) {
		case D3D_SIT_CBUFFER: c = 'b'; break;
		case D3D_SIT_TEXTURE: c = 't'; break;
		case D3D_SIT_SAMPLER: c = 's'; break;
		default: c = ' '; break;
		}
		printf("|Param %c%u : %s\n", c, bind_desc.BindPoint, bind_desc.Name);
	}

	shader_reflection->Release();
	reflection       ->Release();
}

///////////////////////////////////////////

void compile_file(char *filename, char *hlsl_text, settings_t *settings) {
	printf(" ________________\n|Compiling...\n|%s\n\n", filename);

	IDxcBlobEncoding *source;
	if (FAILED(utils->CreateBlob(hlsl_text, (uint32_t)strlen(hlsl_text), CP_UTF8, &source)))
		printf("|CreateBlob failed!\n|________________\n\n");

	DxcBuffer source_buff;
	source_buff.Ptr      = source->GetBufferPointer();
	source_buff.Size     = source->GetBufferSize();
	source_buff.Encoding = 0;

	IDxcIncludeHandler* include_handler = nullptr;
	if (FAILED(utils->CreateDefaultIncludeHandler(&include_handler)))
		printf("|CreateDefaultIncludeHandler failed!\n|________________\n\n");

	if (!dxc_compiler_shader(&source_buff, include_handler, settings, shader_type_vertex)) {
		include_handler->Release();
		source         ->Release();
		return;
	}
	if (!dxc_compiler_shader(&source_buff, include_handler, settings, shader_type_pixel)) {
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

array_t<const wchar_t *> dxc_build_flags(settings_t settings, shader_type_ type) {
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
	case shader_type_pixel:  result.add(settings.ps_entrypoint); break;
	case shader_type_vertex: result.add(settings.vs_entrypoint); break;
	}

	// Target
	result.add(L"-T");
	switch (type) {
	case shader_type_pixel:  wsprintf(settings.shader_model_str, L"ps_%s", settings.shader_model); result.add(settings.shader_model_str); break;
	case shader_type_vertex: wsprintf(settings.shader_model_str, L"vs_%s", settings.shader_model); result.add(settings.shader_model_str); break;
	}

	// Include folder
	result.add(L"-I");
	result.add(settings.folder);

	return result;
}

///////////////////////////////////////////

bool read_file(char *filename, char **out_text, size_t *out_size) {
	*out_text = nullptr;
	*out_size = 0;

	FILE *fp;
	if (fopen_s(&fp, filename, "rb") != 0 || fp == nullptr) {
		return false;
	}

	fseek(fp, 0L, SEEK_END);
	*out_size = ftell(fp);
	rewind(fp);

	*out_text = (char*)malloc(*out_size+1);
	if (*out_text == nullptr) { *out_size = 0; fclose(fp); return false; }
	fread (*out_text, 1, *out_size, fp);
	fclose(fp);

	(*out_text)[*out_size] = 0;
	return true;
}

void get_folder(char *filename, char *out_dest, size_t dest_size) {
	char drive[16];
	char dir  [512];
	_splitpath_s(filename,
		drive, sizeof(drive),
		dir,   sizeof(dir),
		nullptr, 0, nullptr, 0); 

	sprintf_s(out_dest, dest_size, "%s%s", drive, dir);
}