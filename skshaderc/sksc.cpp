#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
///////////////////////////////////////////

/*#pragma comment(lib,"spirv-cross-c.lib")
#pragma comment(lib,"spirv-cross-core.lib")
#pragma comment(lib,"spirv-cross-cpp.lib")
#pragma comment(lib,"spirv-cross-glsl.lib")
#pragma comment(lib,"spirv-cross-hlsl.lib")
#pragma comment(lib,"spirv-cross-msl.lib")
#pragma comment(lib,"spirv-cross-reflect.lib")
#pragma comment(lib,"spirv-cross-util.lib")*/

#include "sksc.h"
#include "../sk_gpu.h"

#if defined(SKSC_D3D11)
	#pragma comment(lib,"d3dcompiler.lib")
	#include <windows.h>
	#include <d3dcompiler.h>
#endif

#if defined(SKSC_SPIRV_DXC)
	#pragma comment(lib,"dxcompiler.lib")
	#include <dxcapi.h>
	#include <d3d12shader.h>
#elif defined(SKSC_SPIRV_GLSLANG)
	#include <glslang/Include/glslang_c_interface.h>
	#include <spirv-tools/optimizer.hpp>
#endif

#include <spirv_cross_c.h>
#include <spirv_glsl.hpp>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

///////////////////////////////////////////

template <typename T> struct array_t {
	T     *data;
	size_t count;
	size_t capacity;

	size_t      add_range  (const T *items, int32_t item_count){ while (count+item_count > capacity) { resize(capacity * 2 < 4 ? 4 : capacity * 2); } memcpy(&data[count], items, sizeof(T)*item_count); count += item_count; return count - item_count; }
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
	template <typename _T, typename D>
	int64_t     index_where(const D _T::*key, const D &item) const { const size_t offset = (size_t)&((_T*)0->*key); for (size_t i = 0; i < count; i++) if (memcmp(((uint8_t *)&data[i]) + offset, &item, sizeof(D)) == 0) return i; return -1; }
	int64_t     index_where(bool (*c)(const T &item, void *user_data), void *user_data) const { for (size_t i=0; i<count; i++) if (c(data[i], user_data)) return i; return -1;}
	int64_t     index_where(bool (*c)(const T &item)) const                                   { for (size_t i=0; i<count; i++) if (c(data[i]))            return i; return -1;}
};

struct file_data_t {
	array_t<uint8_t> data;

	void write_fixed_str(const char *item, int32_t _Size) {
		size_t len = strlen(item);
		data.add_range((uint8_t*)item, (int32_t)(sizeof(char) * len));

		int32_t count = (int32_t)(_Size - len);
		if (_Size - len > 0) {
			while (data.count + count > data.capacity) { data.resize(data.capacity * 2 < 4 ? 4 : data.capacity * 2); }
		}
		memset(&data.data[data.count], 0, count);
		data.count += count;
	}
	template <typename T> 
	void write(T &item) { data.add_range((uint8_t*)&item, sizeof(T)); }
	void write(void *item, size_t size) { data.add_range((uint8_t*)item, (int32_t)size); }
};

enum compile_result_ {
	compile_result_success = 1,
	compile_result_fail = 0,
	compile_result_skip = -1,
};

struct sksc_meta_item_t {
	char name [32];
	char tag  [64];
	char value[512];
	int32_t row, col;
};

///////////////////////////////////////////

int strcmp_nocase(char const *a, char const *b) {
    for (;; a++, b++) {
        int d = tolower((unsigned char)*a) - tolower((unsigned char)*b);
        if (d != 0 || !*a)
            return d;
    }
}

///////////////////////////////////////////

array_t<sksc_meta_item_t> sksc_meta_find_defaults    (const char *hlsl_text);
void                      sksc_meta_assign_defaults  (array_t<sksc_meta_item_t> items, skg_shader_meta_t *ref_meta);
bool                      sksc_meta_check_dup_buffers(const skg_shader_meta_t *ref_meta);

bool sksc_spvc_compile_stage    (const skg_shader_file_stage_t *src_stage, const sksc_settings_t *settings, skg_shader_lang_ lang, skg_shader_file_stage_t *out_stage, const skg_shader_meta_t *meta, array_t<sksc_meta_item_t> var_meta);
bool sksc_spvc_read_meta        (const skg_shader_file_stage_t *spirv_stage, skg_shader_meta_t *meta);

void sksc_line_col              (const char *from_text, const char *at, int32_t *out_line, int32_t *out_column);
void sksc_log_at                (log_level_ level, int32_t line, int32_t column, const char *text, ...);
void sksc_log                   (log_level_ level, const char *text, ...);

///////////////////////////////////////////

array_t<sksc_log_item_t> sksc_log_list = {};

///////////////////////////////////////////

#if defined(SKSC_SPIRV_DXC)

IDxcCompiler3        *sksc_dxc_compiler;
IDxcUtils            *sksc_dxc_utils;

void                  sksc_dxc_init              ();
void                  sksc_dxc_shutdown          ();
array_t<const char *> sksc_dxc_build_flags       (sksc_settings_t settings, skg_stage_ type, skg_shader_lang_ lang);
void                  sksc_dxc_shader_meta       (IDxcResult *compile_result, skg_stage_ stage, skg_shader_meta_t *out_meta);
bool                  sksc_dxc_compile_shader    (DxcBuffer *source_buff, IDxcIncludeHandler *include_handler, sksc_settings_t *settings, skg_stage_ type, skg_shader_lang_ lang, skg_shader_file_stage_t *out_stage, skg_shader_meta_t *out_meta);
void                  sksc_dxc_errors_to_log     (const char *error_string);

///////////////////////////////////////////

void sksc_init() {
	DxcCreateInstance(CLSID_DxcCompiler, __uuidof(IDxcCompiler3), (void **)(&sksc_dxc_compiler));	
	DxcCreateInstance(CLSID_DxcUtils,    __uuidof(IDxcUtils),     (void **)(&sksc_dxc_utils));
}

///////////////////////////////////////////

void sksc_shutdown() {
	sksc_dxc_utils   ->Release();
	sksc_dxc_compiler->Release();
}

///////////////////////////////////////////

bool sksc_dxc_compile_shader(DxcBuffer *source_buff, IDxcIncludeHandler* include_handler, sksc_settings_t *settings, skg_stage_ type, skg_shader_lang_ lang, skg_shader_file_stage_t *out_stage, skg_shader_meta_t *out_meta) {
	IDxcResult   *compile_result;
	IDxcBlobUtf8 *errors;
	bool result = false;

	array_t<const char *> flags  = sksc_dxc_build_flags(*settings, type, lang);
	array_t<wchar_t    *> wflags = {};
	for (size_t i = 0; i < flags.count; i++) {
		size_t   len = strlen(flags[i]);
		wchar_t *f   = (wchar_t*)malloc(sizeof(wchar_t) * (len+1));
		mbstowcs_s(nullptr, f, len+1, flags[i], len);
		wflags.add(f);
	}
	if (FAILED(sksc_dxc_compiler->Compile(source_buff, (LPCWSTR*)wflags.data, (uint32_t)wflags.count, include_handler, __uuidof(IDxcResult), (void **)(&compile_result)))) {
		sksc_log(log_level_err, "DXShaderCompile failed!");
		return false;
	}

	const char *lang_name = lang == skg_shader_lang_hlsl ? "HLSL"  : "SPIRV";
	const char *type_name = type == skg_stage_pixel      ? "Pixel" : "Vertex";
	compile_result->GetOutput(DXC_OUT_ERRORS, __uuidof(IDxcBlobUtf8), (void **)(&errors), nullptr);
	if (errors && errors->GetStringLength() > 0) {
		sksc_dxc_errors_to_log((char *)errors->GetBufferPointer());
	} else {
		out_stage->stage    = type;
		out_stage->language = lang;

		// Get the shader binary
		IDxcBlob *shader_bin;
		compile_result->GetOutput(DXC_OUT_OBJECT, __uuidof(IDxcBlob), (void **)(&shader_bin), nullptr);

		void  *src  = shader_bin->GetBufferPointer();
		size_t size = shader_bin->GetBufferSize();
		out_stage->code      = malloc(size);
		out_stage->code_size = (uint32_t)size;
		memcpy(out_stage->code, src, size);
		shader_bin->Release();

		if (lang == skg_shader_lang_hlsl && out_meta != nullptr) {
			sksc_dxc_shader_meta(compile_result, type, out_meta);
		}
		result = true;
	}
	errors        ->Release();
	compile_result->Release();
	flags.free();
	for (size_t i = 0; i < wflags.count; i++)
		free(wflags[i]);
	wflags.free();

	return result;
}

///////////////////////////////////////////

void sksc_dxc_errors_to_log(const char *error_string) {
	const char *(*next_line)(const char *from) = [](const char *from) {
		const char *curr = from;
		while (*curr != '\n') {
			if (*curr == '\0') return (const char*)nullptr;
			curr++;
		}
		while (*curr == '\n' || *curr == '\r') { curr++; }

		if (*curr == '\0') return (const char*)nullptr;
		return curr;
	};

	bool (*is_err)(const char *line) = [](const char *line) {
		const char *curr = line;
		int32_t ct = 0;
		while (*curr != '\n' && *curr != '\0') {
			if (*curr == ':') ct++;
			curr++;
		}
		return ct >= 4;
	};

	size_t (*line_len)(const char *line) = [](const char *line) {
		const char *curr = line;
		while (*curr != '\n' && *curr != '\0') curr++;
		return (size_t)(curr-line);
	};

	void (*extract)(const char *line, char *out_word) = [](const char *line, char *out_word) {
		const char *curr = line;
		int32_t     ct   = 0;
		while (*curr != '\n' && *curr != '\0' && *curr != ':') {
			out_word[ct] = *curr;
			ct++;
			curr++;
		}
		out_word[ct] = '\0';
	};

	const char *(*index_seg)(const char *line, int out_word) = [](const char *line, int out_word) {
		const char *curr = line;
		while (*curr != '\n' && *curr != '\0') {
			if (*curr == ':') out_word -= 1;
			curr++;
			if (out_word <= 0) {
				while (*curr == ' ' || *curr == '\t') curr++;
				return curr;
			}
		}
		return (const char *)nullptr;
	};

	const char *line = error_string;
	while (line != nullptr) {
		if (is_err(line)) {
			char line_num[32];
			char col_num [32];
			char type    [32];
			extract(index_seg(line, 1), line_num);
			extract(index_seg(line, 2), col_num);
			extract(index_seg(line, 3), type);
			const char *msg = index_seg(line, 4);

			log_level_ level = log_level_err;
			if (strcmp(type, "warning") == 0) level = log_level_warn;
			int32_t line_id = atoi(line_num);
			int32_t col_id  = atoi(col_num);

			sksc_log_at(level, line_id, col_id, "%.*s", line_len(msg), msg);
		} else {
			sksc_log_at(log_level_info, -1, -1, "%.*s", line_len(line), line);
		}
		line = next_line(line);
	}
}

///////////////////////////////////////////

void sksc_dxc_shader_meta(IDxcResult *compile_result, skg_stage_ stage, skg_shader_meta_t *out_meta) {
	// Get information about the shader!
	IDxcBlob *reflection;
	compile_result->GetOutput(DXC_OUT_REFLECTION,  __uuidof(IDxcBlob), (void **)(&reflection), nullptr);

	array_t<skg_shader_buffer_t> buffer_list = {};
	buffer_list.data     = out_meta->buffers;
	buffer_list.capacity = out_meta->buffer_count;
	buffer_list.count    = out_meta->buffer_count;
	array_t<skg_shader_resource_t> texture_list = {};
	texture_list.data     = out_meta->textures;
	texture_list.capacity = out_meta->texture_count;
	texture_list.count    = out_meta->texture_count;

	ID3D12ShaderReflection *shader_reflection;
	DxcBuffer               reflection_buffer;
	reflection_buffer.Ptr      = reflection->GetBufferPointer();
	reflection_buffer.Size     = reflection->GetBufferSize();
	reflection_buffer.Encoding = 0;
	sksc_dxc_utils->CreateReflection(&reflection_buffer, __uuidof(ID3D12ShaderReflection), (void **)(&shader_reflection));
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
			int64_t id = buffer_list.index_where([](auto &buff, void *data) { 
				return strcmp(buff.name, (char*)data) == 0; 
			}, (void*)bind_desc.Name);
			bool is_new = id == -1;
			if (is_new) id = buffer_list.add({});

			// flag it as used by this shader stage
			buffer_list[id].bind.stage_bits = (skg_stage_)(buffer_list[id].bind.stage_bits | stage);

			if (!is_new) continue;

			// Initialize the buffer with data from the shaders!
			snprintf(buffer_list[id].name, _countof(buffer_list[id].name), "%s", bind_desc.Name);
			buffer_list[id].bind.slot = bind_desc.BindPoint;
			buffer_list[id].size      = shader_buff.Size;
			buffer_list[id].var_count = shader_buff.Variables;
			buffer_list[id].vars      = (skg_shader_var_t*)malloc(sizeof(skg_shader_var_t) * buffer_list[i].var_count);
			memset(buffer_list[id].vars, 0, sizeof(skg_shader_var_t) * buffer_list[i].var_count);

			for (uint32_t v = 0; v < shader_buff.Variables; v++) {
				ID3D12ShaderReflectionVariable *var  = cb->GetVariableByIndex(v);
				ID3D12ShaderReflectionType     *type = var->GetType();
				D3D12_SHADER_TYPE_DESC          type_desc;
				D3D12_SHADER_VARIABLE_DESC      var_desc;
				type->GetDesc(&type_desc);
				var ->GetDesc(&var_desc );

				skg_shader_var_ skg_type = skg_shader_var_none;
				switch (type_desc.Type) {
				case D3D_SVT_FLOAT:  skg_type = skg_shader_var_float;  break;
				case D3D_SVT_DOUBLE: skg_type = skg_shader_var_double; break;
				case D3D_SVT_INT:    skg_type = skg_shader_var_int;    break;
				case D3D_SVT_UINT:   skg_type = skg_shader_var_uint;   break;
				case D3D_SVT_UINT8:  skg_type = skg_shader_var_uint8;  break;
				}
				buffer_list[id].vars[v].type = skg_type;
				if (type_desc.Class == D3D_SVC_STRUCT) {
					buffer_list[id].vars[v].type_count = type_desc.Elements;
				} else {
					buffer_list[id].vars[v].type_count = 1;
					if (type_desc.Elements != 0)
						buffer_list[id].vars[v].type_count = type_desc.Elements;
					if (type_desc.Rows != 0)
						buffer_list[id].vars[v].type_count *= type_desc.Rows * type_desc.Columns;
				}

				buffer_list[id].vars[v].size       = var_desc.Size;
				buffer_list[id].vars[v].offset     = var_desc.StartOffset;
				snprintf(buffer_list[id].vars[v].name, _countof(buffer_list[id].vars[v].name), "%s", var_desc.Name);
			}
		} 
		if (bind_desc.Type == D3D_SIT_TEXTURE) {
			int64_t id = texture_list.index_where([](auto &tex, void *data) { 
				return strcmp(tex.name, (char*)data) == 0; 
			}, (void*)bind_desc.Name);
			if (id == -1)
				id = texture_list.add({});

			snprintf(texture_list[id].name, _countof(texture_list[id].name), "%s", bind_desc.Name);
			texture_list[id].bind.stage_bits = (skg_stage_)(texture_list[id].bind.stage_bits | stage);
			texture_list[id].bind.slot       = bind_desc.BindPoint;
		}
	}

	buffer_list .trim();
	texture_list.trim();
	out_meta->buffers       =           buffer_list .data;
	out_meta->buffer_count  = (uint32_t)buffer_list .count;
	out_meta->textures      =           texture_list.data;
	out_meta->texture_count = (uint32_t)texture_list.count;

	// Find the globals buffer, if there is one
	out_meta->global_buffer_id = -1;
	for (size_t i = 0; i < out_meta->buffer_count; i++) {
		if (strcmp(out_meta->buffers[i].name, "$Global") == 0) {
			out_meta->global_buffer_id = i;
		}
	}

	shader_reflection->Release();
	reflection       ->Release();
}

///////////////////////////////////////////

array_t<const char *> sksc_dxc_build_flags(sksc_settings_t settings, skg_stage_ type, skg_shader_lang_ lang) {
	// https://simoncoenen.com/blog/programming/graphics/DxcCompiling.html

	array_t<const char *> result = {};
	if (lang == skg_shader_lang_spirv) {
		result.add("-spirv");
		result.add("-fspv-reflect");
	}
	result.add(settings.row_major 
		? "-Zpr"   // DXC_ARG_PACK_MATRIX_ROW_MAJOR 
		: "-Zpc"); // DXC_ARG_PACK_MATRIX_COLUMN_MAJOR);

	// Debug vs. Release
	if (settings.debug) {
		result.add("-Zi"); // DXC_ARG_DEBUG
		result.add("-Qembed_debug");
		result.add("-Od");
	} else {
		result.add("-Qstrip_debug");
		result.add("-Qstrip_reflect");
		switch (settings.optimize) {
		case 0: result.add("-O0"); break;
		case 1: result.add("-O1"); break;
		case 2: result.add("-O2"); break;
		case 3: result.add("-O3"); break;
		}
	}

	// Entrypoint
	result.add("-E"); 
	switch (type) {
	case skg_stage_pixel:   result.add(settings.ps_entrypoint); break;
	case skg_stage_vertex:  result.add(settings.vs_entrypoint); break;
	case skg_stage_compute: result.add(settings.cs_entrypoint); break;
	}

	// Target
	result.add("-T");
	switch (type) {
	case skg_stage_vertex:  snprintf(settings.shader_model_str, sizeof(settings.shader_model_str), "vs_6_0", settings.shader_model); result.add(settings.shader_model_str); break;
	case skg_stage_pixel:   snprintf(settings.shader_model_str, sizeof(settings.shader_model_str), "ps_6_0", settings.shader_model); result.add(settings.shader_model_str); break;
	case skg_stage_compute: snprintf(settings.shader_model_str, sizeof(settings.shader_model_str), "cs_6_0", settings.shader_model); result.add(settings.shader_model_str); break;
	}

	// Include folders
	result.add("-I");
	result.add(settings.folder);
	for (size_t i = 0; i < settings.include_folder_ct; i++) {
		result.add("-I");
		result.add(settings.include_folders[i]);
	}

	return result;
}

#endif

///////////////////////////////////////////

#if defined(SKSC_SPIRV_GLSLANG)
void sksc_init() {
	glslang_initialize_process();
}

///////////////////////////////////////////

void sksc_shutdown() {
	glslang_finalize_process();
}

///////////////////////////////////////////

// Some nightmare code to deal with glslang's ...lacking C implementation

#include "glslang/Include/ShHandle.h"
#include "StandAlone/DirStackFileIncluder.h"

typedef struct glslang_shader_s {
	glslang::TShader* shader;
	std::string preprocessedGLSL;
} glslang_shader_t;


static EProfile c_shader_profile(glslang_profile_t profile)
{
    switch (profile) {
    case GLSLANG_BAD_PROFILE:
        return EBadProfile;
    case GLSLANG_NO_PROFILE:
        return ENoProfile;
    case GLSLANG_CORE_PROFILE:
        return ECoreProfile;
    case GLSLANG_COMPATIBILITY_PROFILE:
        return ECompatibilityProfile;
    case GLSLANG_ES_PROFILE:
        return EEsProfile;
    case GLSLANG_PROFILE_COUNT: // Should not use this
        break;
    }

    return EProfile();
}

static int c_shader_messages(glslang_messages_t messages)
{
#define CONVERT_MSG(in, out)                                                                                           \
    if ((messages & in) == in)                                                                                         \
        res |= out;

    int res = 0;

    CONVERT_MSG(GLSLANG_MSG_RELAXED_ERRORS_BIT, EShMsgRelaxedErrors);
    CONVERT_MSG(GLSLANG_MSG_SUPPRESS_WARNINGS_BIT, EShMsgSuppressWarnings);
    CONVERT_MSG(GLSLANG_MSG_AST_BIT, EShMsgAST);
    CONVERT_MSG(GLSLANG_MSG_SPV_RULES_BIT, EShMsgSpvRules);
    CONVERT_MSG(GLSLANG_MSG_VULKAN_RULES_BIT, EShMsgVulkanRules);
    CONVERT_MSG(GLSLANG_MSG_ONLY_PREPROCESSOR_BIT, EShMsgOnlyPreprocessor);
    CONVERT_MSG(GLSLANG_MSG_READ_HLSL_BIT, EShMsgReadHlsl);
    CONVERT_MSG(GLSLANG_MSG_CASCADING_ERRORS_BIT, EShMsgCascadingErrors);
    CONVERT_MSG(GLSLANG_MSG_KEEP_UNCALLED_BIT, EShMsgKeepUncalled);
    CONVERT_MSG(GLSLANG_MSG_HLSL_OFFSETS_BIT, EShMsgHlslOffsets);
    CONVERT_MSG(GLSLANG_MSG_DEBUG_INFO_BIT, EShMsgDebugInfo);
    CONVERT_MSG(GLSLANG_MSG_HLSL_ENABLE_16BIT_TYPES_BIT, EShMsgHlslEnable16BitTypes);
    CONVERT_MSG(GLSLANG_MSG_HLSL_LEGALIZATION_BIT, EShMsgHlslLegalization);
    CONVERT_MSG(GLSLANG_MSG_HLSL_DX9_COMPATIBLE_BIT, EShMsgHlslDX9Compatible);
    CONVERT_MSG(GLSLANG_MSG_BUILTIN_SYMBOL_TABLE_BIT, EShMsgBuiltinSymbolTable);
    return res;
#undef CONVERT_MSG
}

int32_t sksc_glslang_shader_preprocess(glslang_shader_t* shader, const glslang_input_t* input, glslang::TShader::Includer &includer) {
	return shader->shader->preprocess(
		reinterpret_cast<const TBuiltInResource*>(input->resource),
		input->default_version,
		c_shader_profile(input->default_profile),
		input->force_default_version_and_profile != 0,
		input->forward_compatible != 0,
		(EShMessages)c_shader_messages(input->messages),
		&shader->preprocessedGLSL,
		includer
	);
}

class SkscIncluder : public DirStackFileIncluder {
public:
    virtual IncludeResult* includeSystem(const char* headerName,
                                         const char* includerName,
                                         size_t inclusionDepth) override
    {
        return readLocalPath(headerName, includerName, (int)inclusionDepth);
    }
};

///////////////////////////////////////////

bool parse_startswith(const char* a, const char* is) {
	while (*is != '\0') {
		if (*a == '\0' || *is != *a)
			return false;
		a++;
		is++;
	}
	return true;
}

bool parse_readint(const char* a, char separator, int32_t *out_int, const char** out_at) {
	const char *end = a;
	while (*end != '\0' && *end != '\n' && *end != separator) {
		end++;
	}
	if (*end != separator) return false;

	char* success = nullptr;
	*out_int = (int32_t)strtol(a, &success, 10);
	*out_at  = end+1;

	return success <= end;
}

const char* parse_glslang_error(const char* at) {
	const char* curr  = at;
	log_level_  level = log_level_err;
	if      (parse_startswith(at, "ERROR: "  )) { level = log_level_err;  curr += 7;}
	else if (parse_startswith(at, "WARNING: ")) { level = log_level_warn; curr += 9;}

	bool has_line = false;
	int32_t line;
	int32_t col;

	const char* numbers = curr;
	// Check for 'col:line:' format line numbers
	if (parse_readint(numbers, ':', &col, &numbers) &&
		parse_readint(numbers, ':', &line, &numbers)) {
		has_line = true;
		curr = numbers + 1;
	}
	numbers = curr;
	// check for '(line)' format line numbers
	if (!has_line && *numbers == '(' && parse_readint(numbers+1, ')', &line, &numbers)) {
		has_line = true;
		curr = numbers + 1;
		if (*curr != '\0') curr++;
	}

	const char* start = curr;
	while (*curr != '\0' && *curr != '\n') {
		curr++;
	}
	if (curr - at > 1) 
		has_line 
			? sksc_log_at(level, line, col, "%.*s", curr - start, start)
			: sksc_log(level, "%.*s", curr - start, start);
	if (*curr == '\n') curr++;
	return *curr == '\0' ? nullptr : curr;
}

compile_result_ sksc_glslang_compile_shader(const char *hlsl, sksc_settings_t *settings, skg_stage_ type, skg_shader_lang_ lang, skg_shader_file_stage_t *out_stage, skg_shader_meta_t *out_meta) {
	glslang_resource_s default_resource = {};
	glslang_input_t    input            = {};
	input.language                = GLSLANG_SOURCE_HLSL;
	input.code                    = hlsl;
	input.client                  = GLSLANG_CLIENT_VULKAN;
	input.client_version          = GLSLANG_TARGET_VULKAN_1_0,
	input.target_language         = GLSLANG_TARGET_SPV;
	input.target_language_version = GLSLANG_TARGET_SPV_1_0;
	input.default_version         = 100;
	input.default_profile         = GLSLANG_NO_PROFILE;
	input.messages                = GLSLANG_MSG_DEFAULT_BIT;
	input.resource                = &default_resource;
	const char *entry = "na";
	switch(type) {
		case skg_stage_vertex:  input.stage = GLSLANG_STAGE_VERTEX;   entry = settings->vs_entrypoint; break;
		case skg_stage_pixel:   input.stage = GLSLANG_STAGE_FRAGMENT; entry = settings->ps_entrypoint; break;
		case skg_stage_compute: input.stage = GLSLANG_STAGE_COMPUTE;  entry = settings->cs_entrypoint; break;
	}

	glslang_shader_t *shader = glslang_shader_create(&input);
	shader->shader->setEntryPoint(entry);

	SkscIncluder includer;
	includer.pushExternalLocalDirectory(settings->folder);
	for (int32_t i = 0; i < settings->include_folder_ct; i++) {
		includer.pushExternalLocalDirectory(settings->include_folders[i]);
	}
	if (!sksc_glslang_shader_preprocess(shader, &input, includer)) {
		const char* curr = glslang_shader_get_info_log(shader);
		while (curr != nullptr) curr = parse_glslang_error(curr);
		curr = glslang_shader_get_info_debug_log(shader);
		while (curr != nullptr) curr = parse_glslang_error(curr);
		glslang_shader_delete (shader);
		return compile_result_fail;
	}

	if (!glslang_shader_parse(shader, &input)) {
		const char* curr = glslang_shader_get_info_log(shader);
		while (curr != nullptr) curr = parse_glslang_error(curr);
		curr = glslang_shader_get_info_debug_log(shader);
		while (curr != nullptr) curr = parse_glslang_error(curr);
		glslang_shader_delete (shader);
		return compile_result_fail;
	}

	glslang_program_t* program = glslang_program_create();
	glslang_program_add_shader(program, shader);

	if (!glslang_program_link(program, GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT | GLSLANG_MSG_DEBUG_INFO_BIT)) {
		const char* curr = glslang_shader_get_info_log(shader);
		while (curr != nullptr) curr = parse_glslang_error(curr);
		curr = glslang_shader_get_info_debug_log(shader);
		while (curr != nullptr) curr = parse_glslang_error(curr);
		glslang_shader_delete (shader);
		glslang_program_delete(program);
		return compile_result_fail;
	}

	// Check if we found an entry point
	const char *link_info = glslang_program_get_info_log(program);
	if (link_info != nullptr) {
		if (strstr(link_info, "Entry point not found") != nullptr) {
			glslang_shader_delete (shader);
			glslang_program_delete(program);
			return compile_result_skip;
		}
	}
	
	glslang_program_SPIRV_generate(program, input.stage);

	if (glslang_program_SPIRV_get_messages(program)) {
		sksc_log(log_level_info, glslang_program_SPIRV_get_messages(program));
	}

	// Get the generated SPIRV code, and wrap up glslang's responsibilities
	size_t spirv_size = glslang_program_SPIRV_get_size(program) * sizeof(unsigned int);
	void  *spirv_code = malloc(spirv_size);
	glslang_program_SPIRV_get(program, (unsigned int*)spirv_code);
	glslang_shader_delete (shader);
	glslang_program_delete(program);

	// Optimize the SPIRV we just generated
	spvtools::Optimizer optimizer(SPV_ENV_UNIVERSAL_1_0);
	//core.SetMessageConsumer(print_msg_to_stderr);
	optimizer.SetMessageConsumer([](spv_message_level_t, const char*, const spv_position_t&, const char* m) {
		printf("SPIRV optimization error: %s\n", m);
	});
	
	//optimizer.RegisterPass(spvtools::CreateWrapOpKillPass());
	optimizer.RegisterPerformancePasses();
	std::vector<uint32_t> spirv_optimized;
	if (!optimizer.Run((uint32_t*)spirv_code, spirv_size/sizeof(uint32_t), &spirv_optimized)) {
		free(spirv_code);
		return compile_result_fail;
	}

	out_stage->language  = lang;
	out_stage->stage     = type;
	out_stage->code_size = (uint32_t)(spirv_optimized.size() * sizeof(unsigned int));
	out_stage->code      = malloc(out_stage->code_size);
	memcpy(out_stage->code, spirv_optimized.data(), out_stage->code_size);
	
	free(spirv_code);

	return compile_result_success;
}

#endif

///////////////////////////////////////////

#if defined(SKSC_D3D11)

DWORD sksc_d3d11_build_flags   (const sksc_settings_t *settings);
bool  sksc_d3d11_compile_shader(const char *filename, const char *hlsl_text, sksc_settings_t *settings, skg_stage_ type, skg_shader_file_stage_t *out_stage, skg_shader_meta_t *ref_meta);

///////////////////////////////////////////

DWORD sksc_d3d11_build_flags(const sksc_settings_t *settings) {
	DWORD result = D3DCOMPILE_ENABLE_STRICTNESS;

	if (settings->row_major) result |= D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;
	else                     result |= D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR;
	if (settings->debug) {
		result |= D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_DEBUG;
	} else {
		switch (settings->optimize) {
		case 0:  result |= D3DCOMPILE_OPTIMIZATION_LEVEL0; break;
		case 1:  result |= D3DCOMPILE_OPTIMIZATION_LEVEL1; break;
		case 2:  result |= D3DCOMPILE_OPTIMIZATION_LEVEL2; break;
		default: result |= D3DCOMPILE_OPTIMIZATION_LEVEL3; break;
		}
	}
	return result;
}

///////////////////////////////////////////

class SKSCInclude : public ID3DInclude
{
public:
	const sksc_settings_t *settings;

	SKSCInclude(const sksc_settings_t *settings) {
		this->settings = settings;
	}

	HRESULT Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID* out_text, UINT* out_size) override
	{
		char path_filename[1024];
		snprintf(path_filename, sizeof(path_filename), "%s\\%s", settings->folder, pFileName);
		FILE *fp = fopen(path_filename, "rb");
		for (int32_t i = 0; fp == nullptr && i < settings->include_folder_ct; i++) {
			snprintf(path_filename, sizeof(path_filename), "%s\\%s", settings->include_folders[i], pFileName);
			fp = fopen(path_filename, "rb");
		}
		if (fp == nullptr) {
			return E_FAIL;
		}

		fseek(fp, 0L, SEEK_END);
		*out_size = ftell(fp);
		rewind(fp);

		*out_text = (char*)malloc(*out_size+1);
		if (*out_text == nullptr) { *out_size = 0; fclose(fp); return false; }
		fread((void*)*out_text, 1, *out_size, fp);
		fclose(fp);

		((char*)*out_text)[*out_size] = 0;

		return S_OK;
	}

	HRESULT Close(LPCVOID data) override
	{
		free((void*)data);
		return S_OK;
	}
};

///////////////////////////////////////////

bool sksc_d3d11_compile_shader(const char *filename, const char *hlsl_text, sksc_settings_t *settings, skg_stage_ type, skg_shader_file_stage_t *out_stage, skg_shader_meta_t *ref_meta) {
	DWORD flags = sksc_d3d11_build_flags(settings);

	const char *entrypoint = nullptr;
	char target[64];
	switch (type) {
	case skg_stage_vertex:  entrypoint = settings->vs_entrypoint; break;
	case skg_stage_pixel:   entrypoint = settings->ps_entrypoint; break;
	case skg_stage_compute: entrypoint = settings->cs_entrypoint; break;
	}
	switch (type) {
	case skg_stage_vertex:  snprintf(target, sizeof(target), "vs_%s", settings->shader_model); break;
	case skg_stage_pixel:   snprintf(target, sizeof(target), "ps_%s", settings->shader_model); break;
	case skg_stage_compute: snprintf(target, sizeof(target), "cs_%s", settings->shader_model); break;
	}

	SKSCInclude includer(settings);
	ID3DBlob *errors, *compiled = nullptr;
	if (FAILED(D3DCompile(hlsl_text, strlen(hlsl_text), filename, nullptr, &includer, entrypoint, target, flags, 0, &compiled, &errors))) {
		const char* curr  = (char*)errors->GetBufferPointer();
		sksc_log(log_level_err_pre, curr);
		if (errors) errors->Release();
		return false;
	}
	else if (errors) {
		const char* curr = (char*)errors->GetBufferPointer();
		sksc_log(log_level_err_pre, curr);
		if (errors) errors->Release();
	}

	out_stage->language  = skg_shader_lang_hlsl;
	out_stage->stage     = type;
	out_stage->code_size = (uint32_t)compiled->GetBufferSize();
	out_stage->code       = malloc(out_stage->code_size);
	memcpy(out_stage->code, compiled->GetBufferPointer(), out_stage->code_size);

	// Get some info about the shader!
	ID3D11ShaderReflection* reflector = nullptr; 
	const GUID IID_ID3D11ShaderReflection = { 0x8d536ca1, 0x0cca, 0x4956, { 0xa8, 0x37, 0x78, 0x69, 0x63, 0x75, 0x55, 0x84 } };
	if (FAILED(D3DReflect( out_stage->code, out_stage->code_size, IID_ID3D11ShaderReflection, (void**)&reflector))) {
		sksc_log(log_level_err_pre, "Shader reflection failed!");
		return false;
	}

	D3D11_SHADER_DESC shader_desc = {};
	reflector->GetDesc(&shader_desc);

	// Snag some perf related to data
	skg_shader_ops_t *ops = nullptr;
	if (type == skg_stage_vertex) ops = &ref_meta->ops_vertex;
	if (type == skg_stage_pixel)  ops = &ref_meta->ops_pixel;
	if (ops) {
		ops->total        = shader_desc.InstructionCount;
		ops->tex_read     = shader_desc.TextureLoadInstructions + shader_desc.TextureNormalInstructions;
		ops->dynamic_flow = shader_desc.DynamicFlowControlCount;
	}

	// Get information about the vertex input data
	if (type == skg_stage_vertex) {
		ref_meta->vertex_input_count = shader_desc.InputParameters;
		ref_meta->vertex_inputs = (skg_vert_component_t*)malloc(sizeof(skg_vert_component_t) * ref_meta->vertex_input_count);

		int32_t curr = 0;
		for (int32_t i = 0; i < (int32_t)shader_desc.InputParameters; i++) {
			D3D11_SIGNATURE_PARAMETER_DESC param_desc = {};
			reflector->GetInputParameterDesc(i, &param_desc);

			// Ignore SV_ inputs, unless they're SV_Position
			if (strlen(param_desc.SemanticName)>3 &&
				tolower(param_desc.SemanticName[0]) == 's' &&
				tolower(param_desc.SemanticName[1]) == 'v' &&
				tolower(param_desc.SemanticName[2]) == '_' &&
				strcmp_nocase(param_desc.SemanticName, "sv_position") != 0)
			{
				ref_meta->vertex_input_count--;
				continue;
			}
			
			ref_meta->vertex_inputs[curr].count         = 0;
			ref_meta->vertex_inputs[curr].semantic_slot = param_desc.SemanticIndex;
			if      (strcmp_nocase(param_desc.SemanticName, "binormal")     == 0) ref_meta->vertex_inputs[curr].semantic = skg_semantic_binormal;
			else if (strcmp_nocase(param_desc.SemanticName, "blendindices") == 0) ref_meta->vertex_inputs[curr].semantic = skg_semantic_blendindices;
			else if (strcmp_nocase(param_desc.SemanticName, "blendweight")  == 0) ref_meta->vertex_inputs[curr].semantic = skg_semantic_blendweight;
			else if (strcmp_nocase(param_desc.SemanticName, "color")        == 0) ref_meta->vertex_inputs[curr].semantic = skg_semantic_color;
			else if (strcmp_nocase(param_desc.SemanticName, "normal")       == 0) ref_meta->vertex_inputs[curr].semantic = skg_semantic_normal;
			else if (strcmp_nocase(param_desc.SemanticName, "sv_position")  == 0) ref_meta->vertex_inputs[curr].semantic = skg_semantic_position;
			else if (strcmp_nocase(param_desc.SemanticName, "position")     == 0) ref_meta->vertex_inputs[curr].semantic = skg_semantic_position;
			else if (strcmp_nocase(param_desc.SemanticName, "psize")        == 0) ref_meta->vertex_inputs[curr].semantic = skg_semantic_psize;
			else if (strcmp_nocase(param_desc.SemanticName, "tangent")      == 0) ref_meta->vertex_inputs[curr].semantic = skg_semantic_tangent;
			else if (strcmp_nocase(param_desc.SemanticName, "texcoord")     == 0) ref_meta->vertex_inputs[curr].semantic = skg_semantic_texcoord;
			switch(param_desc.ComponentType) {
				case D3D_REGISTER_COMPONENT_FLOAT32: ref_meta->vertex_inputs[curr].format = skg_fmt_f32;  break;
				case D3D_REGISTER_COMPONENT_SINT32:  ref_meta->vertex_inputs[curr].format = skg_fmt_i32;  break;
				case D3D_REGISTER_COMPONENT_UINT32:  ref_meta->vertex_inputs[curr].format = skg_fmt_ui32; break;
				default: ref_meta->vertex_inputs[curr].format = skg_fmt_none; break;
			}
			curr += 1;
		}
	}
	
	/*char text[256];
	for (uint32_t i = 0; i < shader_desc.ConstantBuffers; i++) {
		ID3D11ShaderReflectionConstantBuffer *buff = reflector->GetConstantBufferByIndex(i);
		D3D11_SHADER_BUFFER_DESC buff_desc = {};
		buff->GetDesc(&buff_desc);

		snprintf(text, sizeof(text), "Buffer: %s", buff_desc.Name);
		sksc_log(log_level_info, text);
		for (uint32_t v = 0; v < buff_desc.Variables; v++) {
			ID3D11ShaderReflectionVariable *var = buff->GetVariableByIndex(v);
			D3D11_SHADER_VARIABLE_DESC var_desc = {};
			var->GetDesc(&var_desc);

			snprintf(text, sizeof(text), "  %s", var_desc.Name);
			sksc_log(log_level_info, text);
		}
	}*/
	
	reflector->Release();

	compiled->Release();
	return true;
}

#endif

///////////////////////////////////////////

bool sksc_compile(const char *filename, const char *hlsl_text, sksc_settings_t *settings, skg_shader_file_t *out_file) {
	

#if defined(SKSC_SPIRV_DXC)
	IDxcBlobEncoding *source;
	if (FAILED(sksc_dxc_utils->CreateBlob(hlsl_text, (uint32_t)strlen(hlsl_text), CP_UTF8, &source))) {
		sksc_log(log_level_err, "CreateBlob failed");
		return false;
	}

	DxcBuffer source_buff;
	source_buff.Ptr      = source->GetBufferPointer();
	source_buff.Size     = source->GetBufferSize();
	source_buff.Encoding = 0;

	IDxcIncludeHandler* include_handler = nullptr;
	if (FAILED(sksc_dxc_utils->CreateDefaultIncludeHandler(&include_handler))) {
		sksc_log(log_level_err, "CreateDefaultIncludeHandler failed");
		return false;
	}
#endif

	*out_file = {};
	 out_file->meta = (skg_shader_meta_t*)malloc(sizeof(skg_shader_meta_t));
	*out_file->meta = {};
	 out_file->meta->references = 1;

	array_t<skg_shader_file_stage_t> stages   = {};
	array_t<sksc_meta_item_t>        var_meta = sksc_meta_find_defaults(hlsl_text);

	skg_stage_ compile_stages[3] = { skg_stage_vertex, skg_stage_pixel, skg_stage_compute };
	char      *entrypoints   [3] = { settings->vs_entrypoint,    settings->ps_entrypoint,    settings->cs_entrypoint };
	bool       entrypoint_req[3] = { settings->vs_entry_require, settings->ps_entry_require, settings->cs_entry_require };
	for (size_t i = 0; i < sizeof(compile_stages)/sizeof(compile_stages[0]); i++) {
		if (entrypoints[i][0] == 0)
			continue;

#if defined(SKSC_SPIRV_DXC)
		skg_shader_file_stage_t d3d12_hlsl_stage = {};
		if (!sksc_dxc_compile_shader(&source_buff, include_handler, settings, compile_stages[i], skg_shader_lang_hlsl, &d3d12_hlsl_stage, out_file->meta)) {
			sksc_log(log_level_err, "DXC failed to compile shader information");
			return false;
		}

		skg_shader_file_stage_t spirv_stage = {};
		bool spirv_result = sksc_dxc_compile_shader(&source_buff, include_handler, settings, compile_stages[i], skg_shader_lang_spirv, &spirv_stage, nullptr);
		if (settings->target_langs[skg_shader_lang_spirv]) {
			if (!spirv_result) {
				include_handler->Release();
				source         ->Release();

				sksc_log(log_level_err, "SPIRV shader compile failed");
				return false;
			}
			stages.add(spirv_stage);
		}
		include_handler->Release();
		source         ->Release();
#endif

#if defined(SKSC_SPIRV_GLSLANG)
		skg_shader_file_stage_t spirv_stage  = {};
		compile_result_         spirv_result = sksc_glslang_compile_shader(hlsl_text, settings, compile_stages[i], skg_shader_lang_spirv, &spirv_stage, nullptr);
		if (spirv_result == compile_result_fail || (spirv_result == compile_result_skip && entrypoint_req[i])) {
			sksc_log(log_level_err, "SPIRV compile failed");
			return false;
		}
		else if (spirv_result == compile_result_skip)
			continue;

		if (settings->target_langs[skg_shader_lang_spirv]) {
			stages.add(spirv_stage);
		}
		sksc_spvc_read_meta(&spirv_stage, out_file->meta);
#endif

#if defined(SKSC_D3D11)
		
		if (settings->target_langs[skg_shader_lang_hlsl]) {
			stages.add({});
			if (!sksc_d3d11_compile_shader(filename, hlsl_text, settings, compile_stages[i], &stages.last(), out_file->meta)) {
				sksc_log(log_level_err, "HLSL shader compile failed");
				return false;
			}
		}
#else
		if (settings->target_langs[skg_shader_lang_hlsl]) {
			skg_shader_file_stage_t hlsl_stage = {};
			hlsl_stage.language  = skg_shader_lang_hlsl;
			hlsl_stage.stage     = compile_stages[i];
			hlsl_stage.code_size = strlen(hlsl_text) + 1;
			hlsl_stage.code      = malloc(hlsl_stage.code_size);
			memcpy(hlsl_stage.code, hlsl_text, hlsl_stage.code_size);
			stages.add(hlsl_stage);

			sksc_log(log_level_warn, "HLSL shader compiler not available in this build! Shaders on windows may load slowly.");
		}
#endif

		if (settings->target_langs[skg_shader_lang_glsl]) {
			stages.add({});
			if (!sksc_spvc_compile_stage(&spirv_stage, settings, skg_shader_lang_glsl, &stages.last(), out_file->meta, var_meta)) {
				sksc_log(log_level_err, "GLSL shader compile failed");
				return false;
			}
		}

		if (compile_stages[i] != skg_stage_compute && settings->target_langs[skg_shader_lang_glsl_es]) {
			stages.add({});
			if (!sksc_spvc_compile_stage(&spirv_stage, settings, skg_shader_lang_glsl_es, &stages.last(), out_file->meta, var_meta)) {
				sksc_log(log_level_err, "GLES shader compile failed");
				return false;
			}
		}

		if (compile_stages[i] != skg_stage_compute && settings->target_langs[skg_shader_lang_glsl_web]) {
			stages.add({});
			if (!sksc_spvc_compile_stage(&spirv_stage, settings, skg_shader_lang_glsl_web, &stages.last(), out_file->meta, var_meta)) {
				sksc_log(log_level_err, "GLSL web shader compile failed");
				return false;
			}
		}

		//free(d3d12_hlsl_stage.code);
		if (!settings->target_langs[skg_shader_lang_spirv])
			free(spirv_stage.code);
	}
	sksc_meta_assign_defaults(var_meta, out_file->meta);
	out_file->stage_count = (uint32_t)stages.count;
	out_file->stages      = stages.data;

	if (!settings->silent_info) {
		sksc_log(log_level_info, " ________________");
		// Write out our reflection information

		// A quick summary of performance
		sksc_log(log_level_info, "|--Performance--");
		if (out_file->meta->ops_vertex.total > 0 || out_file->meta->ops_pixel.total > 0)
		sksc_log(log_level_info, "| Instructions |  all | tex | flow |");
		if (out_file->meta->ops_vertex.total > 0) {
			sksc_log(log_level_info, "|       Vertex | %4d | %3d | %4d |", 
				out_file->meta->ops_vertex.total, 
				out_file->meta->ops_vertex.tex_read, 
				out_file->meta->ops_vertex.dynamic_flow);
		} 
		if (out_file->meta->ops_pixel.total > 0) {
			sksc_log(log_level_info, "|        Pixel | %4d | %3d | %4d |", 
				out_file->meta->ops_pixel.total, 
				out_file->meta->ops_pixel.tex_read, 
				out_file->meta->ops_pixel.dynamic_flow);
		}

		// List of all the buffers
		sksc_log(log_level_info, "|--Buffer Info--");
		for (size_t i = 0; i < out_file->meta->buffer_count; i++) {
			skg_shader_buffer_t *buff = &out_file->meta->buffers[i];
			sksc_log(log_level_info, "|  %s - %u bytes", buff->name, buff->size);
			for (size_t v = 0; v < buff->var_count; v++) {
				skg_shader_var_t *var = &buff->vars[v];
				const char *type_name = "misc";
				switch (var->type) {
				case skg_shader_var_double: type_name = "dbl"; break;
				case skg_shader_var_float:  type_name = "flt"; break;
				case skg_shader_var_int:    type_name = "int"; break;
				case skg_shader_var_uint:   type_name = "uint"; break;
				case skg_shader_var_uint8:  type_name = "uint8"; break;
				}
				sksc_log(log_level_info, "|    %-15s: +%-4u [%5u] - %s%u", var->name, var->offset, var->size, type_name, var->type_count);
			}
		}

		// Show the vertex shader's input format
		if (out_file->meta->vertex_input_count > 0) {
			sksc_log(log_level_info, "|--Mesh Input--");
			for (int32_t i=0; i<out_file->meta->vertex_input_count; i++) {
				const char *format   = "NA";
				const char *semantic = "NA";
				switch (out_file->meta->vertex_inputs[i].format) {
					case skg_fmt_f32:  format = "float"; break;
					case skg_fmt_i32:  format = "int  "; break;
					case skg_fmt_ui32: format = "uint "; break;
				}
				switch (out_file->meta->vertex_inputs[i].semantic) {
					case skg_semantic_binormal:     semantic = "BiNormal";     break;
					case skg_semantic_blendindices: semantic = "BlendIndices"; break;
					case skg_semantic_blendweight:  semantic = "BlendWeight";  break;
					case skg_semantic_color:        semantic = "Color";        break;
					case skg_semantic_normal:       semantic = "Normal";       break;
					case skg_semantic_position:     semantic = "Position";     break;
					case skg_semantic_psize:        semantic = "PSize";        break;
					case skg_semantic_tangent:      semantic = "Tangent";      break;
					case skg_semantic_texcoord:     semantic = "TexCoord";     break;
				}
				sksc_log(log_level_info, "|  %s : %s%d", format, semantic, out_file->meta->vertex_inputs[i].semantic_slot);
			}
		} 

		for (size_t s = 0; s < sizeof(compile_stages)/sizeof(compile_stages[0]); s++) {
			// check if the stage is used, and skip if it's not
			bool used = false;
			for (size_t i = 0; i < stages.count; i++) {
				if (stages[i].stage & compile_stages[s]) {
					used = true;
					break;
				}
			}
			if (!used) continue;

			const char *stage_name = "";
			switch (compile_stages[s]) {
			case skg_stage_vertex:  stage_name = "Vertex";  break;
			case skg_stage_pixel:   stage_name = "Pixel";   break;
			case skg_stage_compute: stage_name = "Compute"; break;
			}
			sksc_log(log_level_info, "|--%s Shader--", stage_name);
			for (size_t i = 0; i < out_file->meta->buffer_count; i++) {
				skg_shader_buffer_t *buff = &out_file->meta->buffers[i];
				if (buff->bind.stage_bits & compile_stages[s]) {
					sksc_log(log_level_info, "|  b%u : %s", buff->bind.slot, buff->name);
				}
			}
			for (size_t i = 0; i < out_file->meta->resource_count; i++) {
				skg_shader_resource_t *tex = &out_file->meta->resources[i];
				if (tex->bind.stage_bits & compile_stages[s]) {
					sksc_log(log_level_info, "|  %c%u : %s", tex->bind.register_type == skg_register_resource ? 't' : 'u', tex->bind.slot, tex->name);
				}
			}
		}
		sksc_log(log_level_info, "|________________");
	}

	if (!sksc_meta_check_dup_buffers(out_file->meta)) {
		sksc_log(log_level_err, "Found constant buffers re-using slot ids");
		return false;
	}

	

	for (size_t i = 0; i < out_file->stage_count; i++) {
		if (out_file->stages[i].language == skg_shader_lang_glsl_es && 
			(out_file->stages[i].stage == skg_stage_pixel ||
			 out_file->stages[i].stage == skg_stage_vertex)) {
			//sksc_log(log_level_info, "OpenGL pixel shader stage:\n%s", out_file->stages[i].code);
		}
	}

	return true;
}

///////////////////////////////////////////

void sksc_build_file(const skg_shader_file_t *file, void **out_data, size_t *out_size) {
	file_data_t data = {};

	const char tag[8] = {'S','K','S','H','A','D','E','R'};
	uint16_t version = 3;
	data.write(tag);
	data.write(version);

	data.write(file->stage_count);
	data.write_fixed_str(file->meta->name, sizeof(file->meta->name));
	data.write(file->meta->buffer_count);
	data.write(file->meta->resource_count);
	data.write(file->meta->vertex_input_count);

	data.write(file->meta->ops_vertex.total);
	data.write(file->meta->ops_vertex.tex_read);
	data.write(file->meta->ops_vertex.dynamic_flow);
	data.write(file->meta->ops_pixel.total);
	data.write(file->meta->ops_pixel.tex_read);
	data.write(file->meta->ops_pixel.dynamic_flow);

	for (size_t i = 0; i < file->meta->buffer_count; i++) {
		skg_shader_buffer_t *buff = &file->meta->buffers[i];
		data.write_fixed_str(buff->name, sizeof(buff->name));
		data.write(buff->bind);
		data.write(buff->size);
		data.write(buff->var_count);
		if (buff->defaults) {
			data.write(buff->size);
			data.write(buff->defaults, buff->size);
		} else {
			uint32_t zero = 0;
			data.write(zero);
		}

		for (uint32_t t = 0; t < buff->var_count; t++) {
			skg_shader_var_t *var = &buff->vars[t];
			data.write_fixed_str(var->name,  sizeof(var->name));
			data.write_fixed_str(var->extra, sizeof(var->extra));
			data.write(var->offset);
			data.write(var->size);
			data.write(var->type);
			data.write(var->type_count);
		}
	}

	for (int32_t i = 0; i < file->meta->vertex_input_count; i++) {
		skg_vert_component_t *com = &file->meta->vertex_inputs[i];
		data.write(com->format);
		data.write(com->semantic);
		data.write(com->semantic_slot);
	}

	for (uint32_t i = 0; i < file->meta->resource_count; i++) {
		skg_shader_resource_t *res = &file->meta->resources[i];
		data.write_fixed_str(res->name,  sizeof(res->name));
		data.write_fixed_str(res->value, sizeof(res->value));
		data.write_fixed_str(res->tags,  sizeof(res->tags));
		data.write(res->bind);
	}

	for (uint32_t i = 0; i < file->stage_count; i++) {
		skg_shader_file_stage_t *stage = &file->stages[i];
		data.write(stage->language);
		data.write(stage->stage);
		data.write(stage->code_size);
		data.write(stage->code, stage->code_size);
	}

	*out_data = data.data.data;
	*out_size = data.data.count;
}

///////////////////////////////////////////

int64_t mini(int64_t a, int64_t b) {return a<b?a:b;}
int64_t maxi(int64_t a, int64_t b) {return a>b?a:b;}

array_t<sksc_meta_item_t> sksc_meta_find_defaults(const char *hlsl_text) {
	// Searches for metadata in comments that look like this:
	//--name                 = unlit/test
	//--time: color          = 1,1,1,1
	//--tex: 2D, external    = white
	//--uv_scale: range(0,2) = 0.5
	// Where --name is a unique keyword indicating the shader's name, and
	// other elements follow the pattern of:
	// |indicator|param name|tag separator|tag string|default separator|comma separated default values
	//  --        time       :             color      =                 1,1,1,1
	// Metadata can be in // as well as /**/ comments

	array_t<sksc_meta_item_t> items = {};

	// This function will get each line of comment from the file
	const char *(*next_comment)(const char *src, const char **ref_end, bool *ref_state) = [](const char *src, const char **ref_end, bool *ref_state) {
		const char *c      = *ref_end == nullptr ? src : *ref_end;
		const char *result = nullptr;

		// If we're inside a /**/ block, continue from the previous line, we
		// just need to skip any newline characters at the end.
		if (*ref_state) {
			result = (*ref_end)+1;
			while (*result == '\n' || *result == '\r') result++;
		}
		
		// Search for the start of a comment, if we don't have one already.
		while (*c != '\0' && result == nullptr) {
			if (*c == '/' && (*(c+1) == '/' || *(c+1) == '*')) {
				result = (char*)(c+2);
				*ref_state = *(c + 1) == '*';
			}
			c++;
		}

		// Find the end of this comment line.
		c = result;
		while (c != nullptr && *c != '\0' && *c != '\n' && *c != '\r') {
			if (*ref_state && *c == '*' && *(c+1) == '/') {
				*ref_state = false;
				break;
			}
			c++;
		}
		*ref_end = c;

		return result;
	};

	// This function checks if the line is relevant for our metadata
	const char *(*is_relevant)(const char *start, const char *end) = [](const char *start, const char *end) {
		const char *c = start;
		while (c != end && (*c == ' ' || *c == '\t')) c++;

		return end - c > 1 && c[0] == '-' && c[1] == '-' 
			? &c[2] 
			: (char*)nullptr;
	};

	void (*trim_str)(const char **ref_start, const char **ref_end) = [] (const char **ref_start, const char **ref_end){
		while (**ref_start   == ' ' || **ref_start   == '\t') (*ref_start)++;
		while (*(*ref_end-1) == ' ' || *(*ref_end-1) == '\t') (*ref_end)--;
	};

	const char *(*index_of)(const char *start, const char *end, char ch) = [](const char *start, const char *end, char ch) {
		while (start != end) {
			if (*start == ch)
				return start;
			start++;
		}
		return (const char*)nullptr;
	};

	bool        in_comment  = false;
	const char *comment_end = nullptr;
	const char *comment     = next_comment(hlsl_text, &comment_end, &in_comment);
	while (comment) {
		comment = is_relevant(comment, comment_end);
		if (comment) {
			const char *tag_str   = index_of(comment, comment_end, ':');
			const char *value_str = index_of(comment, comment_end, '=');

			const char *name_start = comment;
			const char *name_end   = tag_str?tag_str:(value_str?value_str:comment_end);
			trim_str(&name_start, &name_end);
			char name[32];
			int64_t ct = name_end - name_start;
			memcpy(name, name_start, mini(sizeof(name), ct));
			name[ct] = '\0';

			char tag[64]; tag[0] = '\0';
			if (tag_str) {
				const char *tag_start = tag_str + 1;
				const char *tag_end   = value_str ? value_str : comment_end;
				trim_str(&tag_start, &tag_end);
				ct = maxi(0, tag_end - tag_start);
				memcpy(tag, tag_start, mini(sizeof(tag), ct));
				tag[ct] = '\0';
			}

			char value[512]; value[0] = '\0';
			if (value_str) {
				const char *value_start = value_str + 1;
				const char *value_end   = comment_end;
				trim_str(&value_start, &value_end);
				ct = maxi(0, value_end - value_start);
				memcpy(value, value_start, mini(sizeof(value), ct));
				value[ct] = '\0';
			}

			sksc_meta_item_t item = {};
			sksc_line_col(hlsl_text, comment, &item.row, &item.col);
			strncpy(item.name,  name,  sizeof(item.name));
			strncpy(item.tag,   tag,   sizeof(item.tag));
			strncpy(item.value, value, sizeof(item.value));
			items.add(item);

			if (tag[0] == '\0' && value[0] == '\0') {
				sksc_log_at(log_level_warn, item.row, item.col, "Shader var data for '%s' has no tag or value, missing a ':' or '='?", name);
			}
		}
		comment = next_comment(hlsl_text, &comment_end, &in_comment);
	}
	return items;
}

///////////////////////////////////////////

void sksc_meta_assign_defaults(array_t<sksc_meta_item_t> items, skg_shader_meta_t *ref_meta) {
	int32_t(*count_ch)(const char *str, char ch) = [](const char *str, char ch) {
		const char *c      = str;
		int32_t     result = 0;
		while (*c != '\0') {
			if (*c == ch) result++;
			c++;
		}
		return result;
	};

	for (size_t i = 0; i < items.count; i++) {
		sksc_meta_item_t    *item  = &items[i];
		skg_shader_buffer_t *buff  = ref_meta->global_buffer_id == -1 ? nullptr : &ref_meta->buffers[ref_meta->global_buffer_id];
		int32_t              found = 0;
		for (size_t v = 0; buff && v < buff->var_count; v++) {
			if (strcmp(buff->vars[v].name, item->name) != 0) continue;
			
			found += 1;
			strncpy(buff->vars[v].extra, item->tag, sizeof(buff->vars[v].extra));

			if (item->value[0] == '\0') break;

			int32_t commas = count_ch(item->value, ',');

			if (buff->vars[v].type == skg_shader_var_none) {
				sksc_log_at(log_level_warn, item->row, item->col, "Can't set default for --%s, unimplemented type", item->name);
			} else if (commas + 1 != buff->vars[v].type_count) {
				sksc_log_at(log_level_warn, item->row, item->col, "Default value for --%s has an incorrect number of arguments", item->name);
			} else {
				if (buff->defaults == nullptr) {
					buff->defaults = malloc(buff->size);
					memset(buff->defaults, 0, buff->size);
				}
				uint8_t *write_at = ((uint8_t *)buff->defaults) + buff->vars[v].offset;

				char *start = item->value;
				char *end   = strchr(start, ',');
				char  param[64];
				for (size_t c = 0; c <= commas; c++) {
					int32_t length = (int32_t)(end == nullptr ? mini(sizeof(param)-1, strlen(item->value)) : end - start);
					memcpy(param, start, mini(sizeof(param), length));
					param[length] = '\0';

					double d = atof(param);

					switch (buff->vars[v].type) {
					case skg_shader_var_float:  {float    val = (float   )d; memcpy(write_at, &val, sizeof(val)); write_at += sizeof(val); }break;
					case skg_shader_var_double: {double   val =           d; memcpy(write_at, &val, sizeof(val)); write_at += sizeof(val); }break;
					case skg_shader_var_int:    {int32_t  val = (int32_t )d; memcpy(write_at, &val, sizeof(val)); write_at += sizeof(val); }break;
					case skg_shader_var_uint:   {uint32_t val = (uint32_t)d; memcpy(write_at, &val, sizeof(val)); write_at += sizeof(val); }break;
					case skg_shader_var_uint8:  {uint8_t  val = (uint8_t )d; memcpy(write_at, &val, sizeof(val)); write_at += sizeof(val); }break;
					}

					if (end != nullptr) {
						start = end + 1;
						end   = strchr(start, ',');
					}
				}
			}
			break;
		}

		for (size_t r = 0; r < ref_meta->resource_count; r++) {
			if (strcmp(ref_meta->resources[r].name, item->name) != 0) continue;
			found += 1;

			strncpy(ref_meta->resources[r].tags,  item->tag,   sizeof(ref_meta->resources[r].tags ));
			strncpy(ref_meta->resources[r].value, item->value, sizeof(ref_meta->resources[r].value));
			break;
		}

		if (strcmp(item->name, "name") == 0) {
			found += 1;
			strncpy(ref_meta->name, item->value, sizeof(ref_meta->name));
		}
		
		if (found != 1) {
			sksc_log_at(log_level_warn, item->row, item->col, "Can't find shader var named '%s'", item->name);
		}
	}
}

///////////////////////////////////////////

bool sksc_meta_check_dup_buffers(const skg_shader_meta_t *ref_meta) {
	for (size_t i = 0; i < ref_meta->buffer_count; i++) {
		for (size_t t = 0; t < ref_meta->buffer_count; t++) {
			if (i == t) continue;
			if (ref_meta->buffers[i].bind.slot == ref_meta->buffers[t].bind.slot) {
				return false;
			}
		}
	}
	return true;
}

///////////////////////////////////////////

bool sksc_check_tags(const char *tag_list, const char *tag) {
	const char *start = tag_list;
	const char *end   = tag_list;
	while (*end != '\0') {
		if (*end == ',' || *end == ' ') {
			size_t length = end - start;
			if (length > 0 && strncmp(start, tag, length) == 0) {
				return true;
			}
			start = end + 1;
		}
		end++;
	}
	size_t length = end - start;
	if (length > 0 && strncmp(start, tag, length) == 0) {
		return true;
	}
	return false;
}

///////////////////////////////////////////

bool sksc_spvc_compile_stage(const skg_shader_file_stage_t *src_stage, const sksc_settings_t *settings, skg_shader_lang_ lang, skg_shader_file_stage_t *out_stage, const skg_shader_meta_t *meta, array_t<sksc_meta_item_t> var_meta) {
	try {
		// Create compiler instance
		spirv_cross::CompilerGLSL glsl((uint32_t*)src_stage->code, src_stage->code_size/ sizeof(uint32_t));

		// Set up compiler options
		spirv_cross::CompilerGLSL::Options options;
		if (lang == skg_shader_lang_glsl_web) {
			options.version = 300;
			options.es      = true;
			options.vertex.support_nonzero_base_instance = false;
		} else if (lang == skg_shader_lang_glsl_es) {
			options.version = 320;
			options.es      = true;
			options.vertex.support_nonzero_base_instance = false;
		} else if (lang == skg_shader_lang_glsl) {
			options.version = settings->gl_version;
			options.es = false;
		}
		glsl.set_common_options(options);

		// Reflect shader resources
		spirv_cross::ShaderResources resources = glsl.get_shader_resources();

		// Ensure buffer ids stay the same
		for (spirv_cross::Resource &resource : resources.uniform_buffers) {
			const std::string &name = glsl.get_name(resource.id);
			for (size_t b = 0; b < meta->buffer_count; b++) {
				if (strcmp(name.c_str(), meta->buffers[b].name) == 0 || (strcmp(name.c_str(), "_Global") == 0 && strcmp(meta->buffers[b].name, "$Global") == 0)) {
					glsl.set_decoration(resource.id, spv::DecorationBinding, meta->buffers[b].bind.slot);
					break;
				}
			}
		}

		// Convert tagged textures to use OES samplers
		glsl.set_variable_type_remap_callback([&](const spirv_cross::SPIRType& type, const std::string& var_name, std::string& name_of_type) {
			for (size_t i = 0; i < var_meta.count; i++) {
				if (strcmp(var_meta[i].name, var_name.c_str()) != 0) continue;
				if (sksc_check_tags(var_meta[i].tag, "external")) {
					name_of_type = "samplerExternalOES";
				}
				break;
			}
		});
		
		// Check if we had an external texture. We need to know this now, but
		// the callback above doesn't happen until later.
		bool use_external = false;
		for (spirv_cross::Resource &image : resources.separate_images) {
			const std::string &name = glsl.get_name(image.id);

			for (size_t i = 0; i < var_meta.count; i++) {
				if (strcmp(var_meta[i].name, name.c_str()) != 0) continue;
				if (sksc_check_tags(var_meta[i].tag, "external")) {
					use_external = true;
				}
				break;
			}
		}

		glsl.add_header_line("#extension GL_EXT_gpu_shader5 : enable");
		glsl.add_header_line("#extension GL_OES_sample_variables : enable");
		if (use_external == true && lang == skg_shader_lang_glsl_es) {
			glsl.add_header_line("#extension GL_OES_EGL_image_external_essl3 : enable");
		}
		// Add custom header lines for vertex shaders
		if (src_stage->stage == skg_stage_vertex) {
			glsl.add_header_line("#ifdef GL_AMD_vertex_shader_layer");
			glsl.add_header_line("#extension GL_AMD_vertex_shader_layer : enable");
			glsl.add_header_line("#elif defined(GL_NV_viewport_array2)");
			glsl.add_header_line("#extension GL_NV_viewport_array2 : enable");
			glsl.add_header_line("#else");
			glsl.add_header_line("#define gl_Layer int _dummy_gl_layer_var");
			glsl.add_header_line("#endif");
		}

		// Build dummy sampler for combined images
		spirv_cross::VariableID dummy_sampler_id = glsl.build_dummy_sampler_for_combined_images();
		if (dummy_sampler_id) {
			glsl.set_decoration(dummy_sampler_id, spv::DecorationDescriptorSet, 0);
			glsl.set_decoration(dummy_sampler_id, spv::DecorationBinding,       0);
		}

		// Combine samplers and textures
		glsl.build_combined_image_samplers();

		// Make sure sampler names stay the same in GLSL
		for (const spirv_cross::CombinedImageSampler &remap : glsl.get_combined_image_samplers()) {
			const std::string &name    = glsl.get_name      (remap.image_id);
			uint32_t           binding = glsl.get_decoration(remap.image_id, spv::DecorationBinding);
			glsl.set_name      (remap.combined_id, name);
			glsl.set_decoration(remap.combined_id, spv::DecorationBinding, binding);
		}

		// Rename stage inputs/outputs for vertex/pixel shaders
		if (src_stage->stage == skg_stage_vertex || src_stage->stage == skg_stage_pixel) {
			size_t      off = src_stage->stage == skg_stage_vertex ? sizeof("@entryPointOutput.")-1 : sizeof("input.")-1;
			spirv_cross::SmallVector<spirv_cross::Resource> &stage_resources = src_stage->stage == skg_stage_vertex ? resources.stage_outputs : resources.stage_inputs;
			for (spirv_cross::Resource &resource : stage_resources) {
				char fs_name[64];
				snprintf(fs_name, sizeof(fs_name), "fs_%s", glsl.get_name(resource.id).c_str()+off);
				glsl.set_name(resource.id, fs_name);
			}
		}

		// Compile to GLSL
		std::string source = glsl.compile();

		// Set output stage details
		out_stage->stage     = src_stage->stage;
		out_stage->language  = lang;
		out_stage->code_size = static_cast<uint32_t>(source.size()) + 1;
		out_stage->code      = malloc(out_stage->code_size);
		strncpy(static_cast<char*>(out_stage->code), source.c_str(), out_stage->code_size);

		return true;
	} catch (const spirv_cross::CompilerError &e) {
		sksc_log(log_level_err, "[SPIRV-Cross] %s", e.what());
		return false;
	}
}

///////////////////////////////////////////

bool sksc_spvc_read_meta(const skg_shader_file_stage_t *spirv_stage, skg_shader_meta_t *ref_meta) {
	spvc_context context = nullptr;
	spvc_context_create            (&context);
	spvc_context_set_error_callback( context, [](void *userdata, const char *error) {
		sksc_log(log_level_err, "[SPIRV-Cross] %s", error);
	}, nullptr);

	spvc_compiler  compiler  = nullptr;
	spvc_parsed_ir ir        = nullptr;
	spvc_resources resources = nullptr;
	spvc_context_parse_spirv             (context, (const SpvId*)spirv_stage->code, spirv_stage->code_size/sizeof(SpvId), &ir);
	spvc_context_create_compiler         (context, SPVC_BACKEND_HLSL, ir, SPVC_CAPTURE_MODE_COPY, &compiler);
	spvc_compiler_create_shader_resources(compiler, &resources);

	array_t<skg_shader_buffer_t> buffer_list = {};
	buffer_list.data      = ref_meta->buffers;
	buffer_list.capacity  = ref_meta->buffer_count;
	buffer_list.count     = ref_meta->buffer_count;
	array_t<skg_shader_resource_t> resource_list = {};
	resource_list.data     = ref_meta->resources;
	resource_list.capacity = ref_meta->resource_count;
	resource_list.count    = ref_meta->resource_count;

	const spvc_reflected_resource *list = nullptr;
	size_t                         count;

	// Get buffers
	spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_UNIFORM_BUFFER, &list, &count);
	for (size_t i = 0; i < count; i++) {

		// Find or create a buffer
		int64_t id = buffer_list.index_where([](auto &buff, void *data) { 
			return strcmp(buff.name, (char*)data) == 0; 
		}, (void*)list[i].name);
		bool is_new = id == -1;
		if (is_new) id = buffer_list.add({});

		// Update the stage of this buffer
		skg_shader_buffer_t *buffer = &buffer_list[id];
		buffer->bind.stage_bits |= spirv_stage->stage;

		// And skip the rest if we've already seen it
		if (!is_new) continue;
		
		spvc_type type  = spvc_compiler_get_type_handle(compiler, list[i].base_type_id);
		int32_t   count = spvc_type_get_num_member_types(type);
		size_t    type_size;
		spvc_compiler_get_declared_struct_size(compiler, type, &type_size);

		buffer->size               = (uint32_t)(type_size%16==0? type_size : (type_size/16 + 1)*16);
		buffer->bind.slot          = spvc_compiler_get_decoration(compiler, list[i].id, SpvDecorationBinding);
		buffer->bind.stage_bits    = spirv_stage->stage;
		buffer->bind.register_type = skg_register_constant;
		buffer->var_count          = count;
		buffer->vars               = (skg_shader_var_t*)malloc(count * sizeof(skg_shader_var_t));
		memset(buffer->vars, 0, count * sizeof(skg_shader_var_t));
		strncpy(buffer->name, list[i].name, sizeof(buffer->name));

		for(int32_t m=0; m<count; m+=1) {
			spvc_type_id tid        = spvc_type_get_member_type(type, m);
			spvc_type    mem_type   = spvc_compiler_get_type_handle(compiler, tid);
			const char  *name       = spvc_compiler_get_member_name(compiler, list[i].base_type_id, m);
			uint32_t     member_offset;
			size_t       member_size;
			spvc_compiler_type_struct_member_offset      (compiler, type, m, &member_offset);
			spvc_compiler_get_declared_struct_member_size(compiler, type, m, &member_size);

			uint32_t dimensions = spvc_type_get_num_array_dimensions(mem_type);
			int32_t  dim_size   = 1;
			for (uint32_t d = 0; d < dimensions; d++) {
				dim_size = dim_size * spvc_type_get_array_dimension(mem_type, d);
			}
			
			strncpy(buffer->vars[m].name, name, sizeof(buffer->vars[m].name));
			buffer->vars[m].offset     = member_offset;
			buffer->vars[m].size       = (uint32_t)member_size;
			buffer->vars[m].type_count = dim_size * spvc_type_get_vector_size(mem_type) * spvc_type_get_columns(mem_type);

			if (buffer->vars[m].type_count == 0)
				buffer->vars[m].type_count = 1;

			switch(spvc_type_get_basetype(mem_type)) {
				case SPVC_BASETYPE_INT8:
				case SPVC_BASETYPE_INT16:
				case SPVC_BASETYPE_INT32:
				case SPVC_BASETYPE_INT64:  buffer->vars[m].type = skg_shader_var_int;    break;
				case SPVC_BASETYPE_UINT8:  buffer->vars[m].type = skg_shader_var_uint8;  break;
				case SPVC_BASETYPE_UINT16:
				case SPVC_BASETYPE_UINT32:
				case SPVC_BASETYPE_UINT64: buffer->vars[m].type = skg_shader_var_uint;   break;
				case SPVC_BASETYPE_FP16:
				case SPVC_BASETYPE_FP32:   buffer->vars[m].type = skg_shader_var_float;  break;
				case SPVC_BASETYPE_FP64:   buffer->vars[m].type = skg_shader_var_double; break;
				default:                   buffer->vars[m].type = skg_shader_var_none;   break;
			}
		}
		
		if (strcmp(buffer->name, "$Global") == 0) {
			ref_meta->global_buffer_id = (int32_t)id;
		}
	}
	
	// Find textures
	spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_SEPARATE_IMAGE, &list, &count);
	for (size_t i = 0; i < count; i++) {
		const char *name = spvc_compiler_get_name(compiler, list[i].id);
		int64_t     id   = resource_list.index_where([](auto &tex, void *data) { 
			return strcmp(tex.name, (char*)data) == 0; 
		}, (void*)name);
		if (id == -1)
			id = resource_list.add({});
		
		skg_shader_resource_t *tex = &resource_list[id]; 
		tex->bind.slot          = spvc_compiler_get_decoration(compiler, list[i].id, SpvDecorationBinding);
		tex->bind.stage_bits   |= spirv_stage->stage;
		tex->bind.register_type = skg_register_resource;
		strncpy(tex->name, name, sizeof(tex->name));
	}

	// Look for RWTexture2D
	spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_STORAGE_IMAGE, &list, &count);
	for (size_t i = 0; i < count; i++) {
		const char *name = spvc_compiler_get_name(compiler, list[i].id);
		int64_t     id   = resource_list.index_where([](auto &tex, void *data) { 
			return strcmp(tex.name, (char*)data) == 0; 
			}, (void*)name);
		if (id == -1)
			id = resource_list.add({});

		skg_shader_resource_t *tex = &resource_list[id];
		tex->bind.slot             = spvc_compiler_get_decoration(compiler, list[i].id, SpvDecorationBinding);
		tex->bind.stage_bits      |= spirv_stage->stage;
		tex->bind.register_type    = skg_register_readwrite;
		strncpy(tex->name, name, sizeof(tex->name));
	}

	// Look for RWStructuredBuffers and StructuredBuffers
	spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_STORAGE_BUFFER, &list, &count);
	for (size_t i = 0; i < count; i++) {
		const char *name     = spvc_compiler_get_name             (compiler, list[i].id);
		bool        readonly = spvc_compiler_has_member_decoration(compiler, list[i].base_type_id, 0, SpvDecorationNonWritable);

		int64_t id = resource_list.index_where([](auto &tex, void *data) { 
			return strcmp(tex.name, (char*)data) == 0; 
			}, (void*)name);
		if (id == -1)
			id = resource_list.add({});

		skg_shader_resource_t *tex = &resource_list[id];
		tex->bind.slot          = spvc_compiler_get_decoration(compiler, list[i].id, SpvDecorationBinding);
		tex->bind.stage_bits   |= spirv_stage->stage;
		tex->bind.register_type = readonly ? skg_register_resource : skg_register_readwrite;
		strncpy(tex->name, name, sizeof(tex->name));
	}

	ref_meta->buffers        = buffer_list.data;
	ref_meta->buffer_count   = (uint32_t)buffer_list.count;
	ref_meta->resources      = resource_list.data;
	ref_meta->resource_count = (uint32_t)resource_list.count;

	// Frees all memory we allocated so far.
	spvc_context_destroy(context);
	return true;
}

///////////////////////////////////////////

void sksc_line_col(const char *from_text, const char *at, int32_t *out_line, int32_t *out_column) {
	if (out_line  ) *out_line   = -1;
	if (out_column) *out_column = -1;

	bool found = false;
	const char *curr = from_text;
	int32_t line = 0, col = 0;
	while (*curr != '\0') {
		if (*curr == '\n') { line++; col = 0; } 
		else if (*curr != '\r') col++;
		if (curr == at) {
			found = true;
			break;
		}
		curr++;
	}

	if (found) {
		if (out_line  ) *out_line   = line+1;
		if (out_column) *out_column = col;
	}
}

///////////////////////////////////////////

void sksc_log(log_level_ level, const char *text, ...) {
	sksc_log_item_t item = {};
	item.level  = level;
	item.line   = -1;
	item.column = -1;

	va_list args;
	va_start(args, text);
	va_list copy;
	va_copy(copy, args);
	size_t length = vsnprintf(nullptr, 0, text, args);
	item.text = (char*)malloc(length + 2);
	vsnprintf((char*)item.text, length + 2, text, copy);
	va_end(copy);
	va_end(args);

	sksc_log_list.add(item);
}

///////////////////////////////////////////

void sksc_log_at(log_level_ level, int32_t line, int32_t column, const char *text, ...) {
	sksc_log_item_t item = {};
	item.level  = level;
	item.line   = line;
	item.column = column;

	va_list args;
	va_start(args, text);
	va_list copy;
	va_copy(copy, args);
	size_t length = vsnprintf(nullptr, 0, text, args);
	item.text = (char*)malloc(length + 2);
	vsnprintf((char*)item.text, length + 2, text, copy);
	va_end(copy);
	va_end(args);

	sksc_log_list.add(item);
}

///////////////////////////////////////////

void sksc_log_print(const char *file, const sksc_settings_t *settings) {
	for (size_t i = 0; i < sksc_log_list.count; i++) {
		if (sksc_log_list[i].level == log_level_info && !settings->silent_info) {
			printf("%s\n", sksc_log_list[i].text);
		}
	}
	for (size_t i = 0; i < sksc_log_list.count; i++) {
		if ((sksc_log_list[i].level == log_level_err_pre && !settings->silent_err)) {
			printf("%s", sksc_log_list[i].text);
		}
	}
	for (size_t i = 0; i < sksc_log_list.count; i++) {
		if ((sksc_log_list[i].level == log_level_warn && !settings->silent_warn) ||
			(sksc_log_list[i].level == log_level_err  && !settings->silent_err )) {

			const char* level = sksc_log_list[i].level == log_level_warn
				? "warning"
				: "error";

			if (sksc_log_list[i].line < 0) {
				printf("%s: %s: %s\n", file, level, sksc_log_list[i].text);
			} else {
				printf("%s(%d,%d): %s: %s\n", file, sksc_log_list[i].line, sksc_log_list[i].column, level, sksc_log_list[i].text);
			}
		}
	}
}

///////////////////////////////////////////

void sksc_log_clear() {
	sksc_log_list.each([](sksc_log_item_t &i) {free((void*)i.text); });
	sksc_log_list.clear();
}

///////////////////////////////////////////

int32_t sksc_log_count() {
	return (int32_t)sksc_log_list.count;
}

///////////////////////////////////////////

sksc_log_item_t sksc_log_get(int32_t index) {
	if (index < 0 || index >= sksc_log_list.count)
		return {};
	return sksc_log_list[index];
}