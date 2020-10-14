
///////////////////////////////////////////

#pragma comment(lib,"dxcompiler.lib")
#pragma comment(lib,"d3dcompiler.lib")
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
#include <d3dcompiler.h>
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
	int64_t     index_where(const D T::*key, const D &item) const { const size_t offset = (size_t)&((T*)0->*key); for (size_t i = 0; i < count; i++) if (memcmp(((uint8_t *)&data[i]) + offset, &item, sizeof(D)) == 0) return i; return -1; }
	int64_t     index_where(bool (*c)(const T &item, void *user_data), void *user_data) const { for (size_t i=0; i<count; i++) if (c(data[i], user_data)) return i; return -1;}
	int64_t     index_where(bool (*c)(const T &item)) const                                   { for (size_t i=0; i<count; i++) if (c(data[i]))            return i; return -1;}
};

///////////////////////////////////////////

void                  sksc_meta_find_defaults    (char *hlsl_text, skr_shader_meta_t *ref_meta);
bool                  sksc_meta_check_dup_buffers(const skr_shader_meta_t *ref_meta);

DWORD                 sksc_d3d11_build_flags     (const sksc_settings_t *settings);
bool                  sksc_d3d11_compile_shader  (char *filename, char *hlsl_text, sksc_settings_t *settings, skr_stage_ type, skr_shader_file_stage_t *out_stage);

array_t<const char *> sksc_dxc_build_flags       (sksc_settings_t settings, skr_stage_ type, skr_shader_lang_ lang);
void                  sksc_dxc_shader_meta       (IDxcResult *compile_result, skr_stage_ stage, skr_shader_meta_t *out_meta);
bool                  sksc_dxc_compile_shader    (DxcBuffer *source_buff, IDxcIncludeHandler *include_handler, sksc_settings_t *settings, skr_stage_ type, skr_shader_lang_ lang, skr_shader_file_stage_t *out_stage, skr_shader_meta_t *out_meta);

bool                  sksc_spvc_compile_stage    (const skr_shader_file_stage_t *src_stage, skr_shader_lang_ lang, skr_shader_file_stage_t *out_stage, const skr_shader_meta_t *meta);

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
	printf(" ________________\n| Compiling %s...\n|\n", filename);

	IDxcBlobEncoding *source;
	if (FAILED(sksc_utils->CreateBlob(hlsl_text, (uint32_t)strlen(hlsl_text), CP_UTF8, &source))) {
		printf("| CreateBlob failed!\n|_/__/__/__/__/__\n\n");
		return false;
	}

	DxcBuffer source_buff;
	source_buff.Ptr      = source->GetBufferPointer();
	source_buff.Size     = source->GetBufferSize();
	source_buff.Encoding = 0;

	IDxcIncludeHandler* include_handler = nullptr;
	if (FAILED(sksc_utils->CreateDefaultIncludeHandler(&include_handler))) {
		printf("| CreateDefaultIncludeHandler failed!\n|_/__/__/__/__/__\n\n");
		return false;
	}

	*out_file = {};
	 out_file->meta = (skr_shader_meta_t*)malloc(sizeof(skr_shader_meta_t));
	*out_file->meta = {};

	array_t<skr_shader_file_stage_t> stages = {};

	skr_stage_ compile_stages[3] = { skr_stage_vertex, skr_stage_pixel, skr_stage_compute };
	char      *entrypoints   [3] = { settings->vs_entrypoint, settings->ps_entrypoint, settings->cs_entrypoint };
	for (size_t i = 0; i < sizeof(compile_stages)/sizeof(compile_stages[0]); i++) {
		if (entrypoints[i][0] == 0)
			continue;

		skr_shader_file_stage_t d3d12_hlsl_stage = {};
		stages.add({});
		if (!sksc_dxc_compile_shader  (&source_buff, include_handler, settings, compile_stages[i], skr_shader_lang_hlsl, &d3d12_hlsl_stage, out_file->meta) ||
			!sksc_d3d11_compile_shader(filename, hlsl_text, settings, compile_stages[i], &stages.last())) {
			include_handler->Release();
			source         ->Release();
			return false;
		}

		size_t spirv_stage = stages.add({});
		if (!sksc_dxc_compile_shader(&source_buff, include_handler, settings, compile_stages[i], skr_shader_lang_spirv, &stages.last(), nullptr)) {
			include_handler->Release();
			source         ->Release();
			return false;
		}

		stages.add({});
		if (!sksc_spvc_compile_stage(&stages[spirv_stage], skr_shader_lang_glsl, &stages.last(), out_file->meta)) {
			include_handler->Release();
			source         ->Release();
			return false;
		}
		stages.add({});
		if (!sksc_spvc_compile_stage(&stages[spirv_stage], skr_shader_lang_glsl_web, &stages.last(), out_file->meta)) {
			include_handler->Release();
			source         ->Release();
			return false;
		}

		free(d3d12_hlsl_stage.code);
	}
	
	// cleanup
	include_handler->Release();
	source         ->Release();

	sksc_meta_find_defaults(hlsl_text, out_file->meta);
	out_file->stage_count = stages.count;
	out_file->stages      = stages.data;

	// Write out our reflection information
	printf("|--Buffer Info--\n");
	for (size_t i = 0; i < out_file->meta->buffer_count; i++) {
		skr_shader_buffer_t *buff = &out_file->meta->buffers[i];
		printf("|  %s - %u bytes\n", buff->name, buff->size);
		for (size_t v = 0; v < buff->var_count; v++) {
			skr_shader_var_t *var = &buff->vars[v];
			const char *type_name = "misc";
			switch (var->type) {
			case skr_shader_var_double: type_name = "dbl"; break;
			case skr_shader_var_float:  type_name = "flt"; break;
			case skr_shader_var_int:    type_name = "int"; break;
			case skr_shader_var_uint:   type_name = "uint"; break;
			case skr_shader_var_uint8:  type_name = "uint8"; break;
			}
			printf("|    %-15s: +%-4u [%5u] - %s%u\n", var->name, var->offset, var->size, type_name, var->type_count);
		}
	}

	for (size_t s = 0; s < sizeof(compile_stages)/sizeof(compile_stages[0]); s++) {
		const char *stage_name = "";
		switch (compile_stages[s]) {
		case skr_stage_vertex:  stage_name = "Vertex";  break;
		case skr_stage_pixel:   stage_name = "Pixel";   break;
		case skr_stage_compute: stage_name = "Compute"; break;
		}
		printf("|--%s Shader--\n", stage_name);
		for (size_t i = 0; i < out_file->meta->buffer_count; i++) {
			skr_shader_buffer_t *buff = &out_file->meta->buffers[i];
			if (buff->bind.stage_bits & compile_stages[s]) {
				printf("|  b%u : %s\n", buff->bind.slot, buff->name);
			}
		}
		for (size_t i = 0; i < out_file->meta->texture_count; i++) {
			skr_shader_texture_t *tex = &out_file->meta->textures[i];
			if (tex->bind.stage_bits & compile_stages[s]) {
				printf("|  s%u : %s\n", tex->bind.slot, tex->name );
			}
		}
	}

	if (!sksc_meta_check_dup_buffers(out_file->meta)) {
		printf("| !! Found constant buffers re-using slot ids !!\n|_/__/__/__/__/__\n\n");
		return false;
	}

	printf("|________________\n\n");

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
		skr_shader_buffer_t *buff = &file->meta->buffers[i];
		fwrite( buff->name,      sizeof(buff->name     ), 1, fp);
		fwrite(&buff->bind,      sizeof(buff->bind     ), 1, fp);
		fwrite(&buff->size,      sizeof(buff->size     ), 1, fp);
		fwrite(&buff->var_count, sizeof(buff->var_count), 1, fp);
		if (buff->defaults) {
			fwrite(&buff->size, sizeof(buff->size), 1, fp);
			fwrite( buff->defaults, buff->size, 1, fp);
		} else {
			uint32_t zero = 0;
			fwrite(&zero, sizeof(buff->size), 1, fp);
		}

		for (uint32_t t = 0; t < buff->var_count; t++) {
			skr_shader_var_t *var = &buff->vars[t];
			fwrite(var->name,        sizeof(var->name),       1, fp);
			fwrite(var->extra,       sizeof(var->extra),      1, fp);
			fwrite(&var->offset,     sizeof(var->offset),     1, fp);
			fwrite(&var->size,       sizeof(var->size),       1, fp);
			fwrite(&var->type,       sizeof(var->type),       1, fp);
			fwrite(&var->type_count, sizeof(var->type_count), 1, fp);
		}
	}

	for (uint32_t i = 0; i < file->meta->texture_count; i++) {
		skr_shader_texture_t *tex = &file->meta->textures[i];
		fwrite( tex->name,  sizeof(tex->name ), 1, fp); 
		fwrite( tex->extra, sizeof(tex->extra), 1, fp); 
		fwrite(&tex->bind,  sizeof(tex->bind ), 1, fp);
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

void sksc_save_header(char *sks_file) {
	FILE *fp;
	if (fopen_s(&fp, sks_file, "rb") != 0 || fp == nullptr) {
		return;
	}

	fseek(fp, 0L, SEEK_END);
	size_t size = ftell(fp);
	rewind(fp);

	void* data = (char*)malloc(size+1);
	if (data == nullptr) { size = 0; fclose(fp); return; }
	fread (data, 1, size, fp);
	fclose(fp);

	char new_filename[512];
	char drive[16];
	char dir  [512];
	char name [128];
	_splitpath_s(sks_file,
		drive, sizeof(drive),
		dir,   sizeof(dir),
		name,  sizeof(name), nullptr, 0);
	sprintf_s(new_filename, "%s%s%s.h", drive, dir, name);

	// '.' may be common, and will bork the variable name
	size_t len = strlen(name);
	for (size_t i = 0; i < len; i++) {
		if (name[i] == '.') name[i] = '_';
	}

	fp = nullptr;
	if (fopen_s(&fp, new_filename, "w") != 0 || fp == nullptr) {
		return;
	}
	fprintf(fp, "#pragma once\n\n");
	int32_t ct = fprintf_s(fp, "const unsigned char sks_%s[%zu] = {", name, size);
	for (size_t i = 0; i < size; i++) {
		unsigned char byte = ((unsigned char *)data)[i];
		ct += fprintf_s(fp, "%d,", byte);
		if (ct > 80) { 
			fprintf(fp, "\n"); 
			ct = 0; 
		}
	}
	fprintf_s(fp, "};\n");
	fclose(fp);

	free(data);
}

///////////////////////////////////////////

void sksc_meta_find_defaults(char *hlsl_text, skr_shader_meta_t *ref_meta) {
	// Searches for metadata in comments that look like this:
	//--name                 = unlit/test
	//--time: color          = 1,1,1,1
	//--tex: 2D              = white
	//--uv_scale: range(0,2) = 0.5
	// Where --name is a unique keyword indicating the shader's name, and
	// other elements follow the pattern of:
	// |indicator|param name|tag separator|tag string|default separator|comma separated default values
	//  --        time       :             color      =                 1,1,1,1
	// Metadata can be in // as well as /**/ comments

	// This function will get each line of comment from the file
	char *(*next_comment)(char *src, char **ref_end, bool *ref_state) = [](char *src, char **ref_end, bool *ref_state) {
		char *c      = *ref_end == nullptr ? src : *ref_end;
		char *result = nullptr;

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
	char *(*is_relevant)(char *start, char *end) = [](char *start, char *end) {
		char *c = start;
		while (c != end && (*c == ' ' || *c == '\t')) c++;

		return end - c > 1 && c[0] == '-' && c[1] == '-' 
			? &c[2] 
			: (char*)nullptr;
	};

	void (*trim_str)(char **ref_start, char **ref_end) = [] (char **ref_start, char **ref_end){
		while (**ref_start   == ' ' || **ref_start   == '\t') (*ref_start)++;
		while (*(*ref_end-1) == ' ' || *(*ref_end-1) == '\t') (*ref_end)--;
	};

	char *(*index_of)(char *start, char *end, char ch) = [](char *start, char *end, char ch) {
		while (start != end) {
			if (*start == ch)
				return start;
			start++;
		}
		return (char*)nullptr;
	};

	int32_t(*count_ch)(const char *str, char ch) = [](const char *str, char ch) {
		const char *c      = str;
		int32_t     result = 0;
		while (*c != '\0') {
			if (*c == ch) result++;
			c++;
		}
		return result;
	};

	bool  in_comment  = false;
	char *comment_end = nullptr;
	char *comment     = next_comment(hlsl_text, &comment_end, &in_comment);
	while (comment) {
		comment = is_relevant(comment, comment_end);
		if (comment) {
			char *tag_str   = index_of(comment, comment_end, ':');
			char *value_str = index_of(comment, comment_end, '=');

			char *name_start = comment;
			char *name_end   = tag_str?tag_str:(value_str?value_str:comment_end);
			trim_str(&name_start, &name_end);
			char name[32];
			memcpy(name, name_start, min(sizeof(name), name_end - name_start));
			name[name_end-name_start] = '\0';

			char tag[64]; tag[0] = '\0';
			if (tag_str) {
				char *tag_start = tag_str + 1;
				char *tag_end   = value_str ? value_str : comment_end;
				trim_str(&tag_start, &tag_end);
				memcpy(tag, tag_start, min(sizeof(tag), tag_end - tag_start));
				tag[tag_end-tag_start] = '\0';
			}

			char value[512]; value[0] = '\0';
			if (value_str) {
				char *value_start = value_str + 1;
				char *value_end   = comment_end;
				trim_str(&value_start, &value_end);
				memcpy(value, value_start, min(sizeof(value), value_end - value_start));
				value[value_end-value_start] = '\0';
			}

			skr_shader_buffer_t *buff  = &ref_meta->buffers[ref_meta->global_buffer_id];
			int32_t              found = 0;
			for (size_t i = 0; i < buff->var_count; i++) {
				if (strcmp(buff->vars[i].name, name) == 0) {
					found += 1;
					strcpy_s(buff->vars[i].extra, tag);

					if (value_str) {
						int32_t commas = count_ch(value, ',');

						if (buff->vars[i].type == skr_shader_var_none) {
							printf("| !! Can't set default for --%s, unimplemented type !!\n", name);
						} else if (commas + 1 != buff->vars[i].type_count) {
							printf("| !! Default value for --%s has an incorrect number of arguments !!\n", name);
						} else {
							if (buff->defaults == nullptr) {
								buff->defaults = malloc(buff->size);
								memset(buff->defaults, 0, buff->size);
							}
							uint8_t *write_at = ((uint8_t *)buff->defaults) + buff->vars[i].offset;

							char *start = value;
							char *end   = strchr(start, ',');
							char  item[64];
							for (size_t c = 0; c <= commas; c++) {
								int32_t length = (int32_t)(end == nullptr ? min(sizeof(item)-1, strlen(value)) : end - start);
								memcpy(item, start, min(sizeof(item), length));
								item[length] = '\0';

								double d = atof(item);

								switch (buff->vars[i].type) {
								case skr_shader_var_float:  {float    v = (float   )d; memcpy(write_at, &v, sizeof(v)); write_at += sizeof(v); }break;
								case skr_shader_var_double: {double   v =           d; memcpy(write_at, &v, sizeof(v)); write_at += sizeof(v); }break;
								case skr_shader_var_int:    {int32_t  v = (int32_t )d; memcpy(write_at, &v, sizeof(v)); write_at += sizeof(v); }break;
								case skr_shader_var_uint:   {uint32_t v = (uint32_t)d; memcpy(write_at, &v, sizeof(v)); write_at += sizeof(v); }break;
								case skr_shader_var_uint8:  {uint8_t  v = (uint8_t )d; memcpy(write_at, &v, sizeof(v)); write_at += sizeof(v); }break;
								}

								if (end != nullptr) {
									start = end + 1;
									end   = strchr(start, ',');
								}
							}
						}
					}
					break;
				}
			}
			for (size_t i = 0; i < ref_meta->texture_count; i++) {
				if (strcmp(ref_meta->textures[i].name, name) == 0) {
					if (value_str) {
						found += 1;
						strcpy_s(ref_meta->textures[i].extra, value);
					} else {
						printf("| !! --%s doesn't properly provide a value !!\n", name);
					}
					break;
				}
			}
			if (strcmp(name, "name") == 0) {
				found += 1;
				strcpy_s(ref_meta->name, value);
			}
			

			if (found != 1) {
				printf("| !! Can't find shader var named '%s' !!\n", name);
			} else if (!tag_str && !value_str) {
				printf("| !! Shader var tag for %s not used, missing a ':' or '='? !!\n", name);
			}
		}
		comment = next_comment(hlsl_text, &comment_end, &in_comment);
	}
}

///////////////////////////////////////////

bool sksc_meta_check_dup_buffers(const skr_shader_meta_t *ref_meta) {
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

DWORD sksc_d3d11_build_flags(const sksc_settings_t *settings) {
	DWORD result = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS;

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

bool sksc_d3d11_compile_shader(char *filename, char *hlsl_text, sksc_settings_t *settings, skr_stage_ type, skr_shader_file_stage_t *out_stage) {
	DWORD flags = sksc_d3d11_build_flags(settings);

	const char *entrypoint = nullptr;
	char target[64];
	switch (type) {
	case skr_stage_vertex:  entrypoint = settings->vs_entrypoint; break;
	case skr_stage_pixel:   entrypoint = settings->ps_entrypoint; break;
	case skr_stage_compute: entrypoint = settings->cs_entrypoint; break;
	}
	switch (type) {
	case skr_stage_vertex:  snprintf(target, sizeof(target), "vs_%s", settings->shader_model); break;
	case skr_stage_pixel:   snprintf(target, sizeof(target), "ps_%s", settings->shader_model); break;
	case skr_stage_compute: snprintf(target, sizeof(target), "cs_%s", settings->shader_model); break;
	}

	ID3DBlob *errors, *compiled = nullptr;
	if (FAILED(D3DCompile(hlsl_text, strlen(hlsl_text), filename, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, entrypoint, target, flags, 0, &compiled, &errors))) {
		printf("| Error - D3DCompile failed:\n");
		printf((char *)errors->GetBufferPointer());
		if (errors) errors->Release();
		return false;
	}
	if (errors) errors->Release();

	out_stage->language  = skr_shader_lang_hlsl;
	out_stage->stage     = type;
	out_stage->code_size = (uint32_t)compiled->GetBufferSize();
	out_stage->code       = malloc(out_stage->code_size);
	memcpy(out_stage->code, compiled->GetBufferPointer(), out_stage->code_size);

	compiled->Release();
	return true;
}

///////////////////////////////////////////

bool sksc_dxc_compile_shader(DxcBuffer *source_buff, IDxcIncludeHandler* include_handler, sksc_settings_t *settings, skr_stage_ type, skr_shader_lang_ lang, skr_shader_file_stage_t *out_stage, skr_shader_meta_t *out_meta) {
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
	if (FAILED(sksc_compiler->Compile(source_buff, (LPCWSTR*)wflags.data, (uint32_t)wflags.count, include_handler, __uuidof(IDxcResult), (void **)(&compile_result)))) {
		printf("| Compile failed!\n|________________\n");
		return false;
	}

	const char *lang_name = lang == skr_shader_lang_hlsl ? "HLSL"  : "SPIRV";
	const char *type_name = type == skr_stage_pixel      ? "Pixel" : "Vertex";
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
		out_stage->code_size = (uint32_t)size;
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
	for (size_t i = 0; i < wflags.count; i++)
		free(wflags[i]);
	wflags.free();

	return result;
}

///////////////////////////////////////////

void sksc_dxc_shader_meta(IDxcResult *compile_result, skr_stage_ stage, skr_shader_meta_t *out_meta) {
	// Get information about the shader!
	IDxcBlob *reflection;
	compile_result->GetOutput(DXC_OUT_REFLECTION,  __uuidof(IDxcBlob), (void **)(&reflection), nullptr);

	array_t<skr_shader_buffer_t> buffer_list = {};
	buffer_list.data     = out_meta->buffers;
	buffer_list.capacity = out_meta->buffer_count;
	buffer_list.count    = out_meta->buffer_count;
	array_t<skr_shader_texture_t> texture_list = {};
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
			int64_t id = buffer_list.index_where([](auto &buff, void *data) { 
				return strcmp(buff.name, (char*)data) == 0; 
			}, (void*)bind_desc.Name);
			bool is_new = id == -1;
			if (is_new) id = buffer_list.add({});

			// flag it as used by this shader stage
			buffer_list[id].bind.stage_bits = (skr_stage_)(buffer_list[id].bind.stage_bits | stage);

			if (!is_new) continue;

			// Initialize the buffer with data from the shaders!
			sprintf_s(buffer_list[id].name, _countof(buffer_list[id].name), "%s", bind_desc.Name);
			buffer_list[id].bind.slot = bind_desc.BindPoint;
			buffer_list[id].size      = shader_buff.Size;
			buffer_list[id].var_count = shader_buff.Variables;
			buffer_list[id].vars      = (skr_shader_var_t*)malloc(sizeof(skr_shader_var_t) * buffer_list[i].var_count);
			*buffer_list[id].vars     = {};

			for (uint32_t v = 0; v < shader_buff.Variables; v++) {
				ID3D12ShaderReflectionVariable *var  = cb->GetVariableByIndex(v);
				ID3D12ShaderReflectionType     *type = var->GetType();
				D3D12_SHADER_TYPE_DESC          type_desc;
				D3D12_SHADER_VARIABLE_DESC      var_desc;
				type->GetDesc(&type_desc);
				var ->GetDesc(&var_desc );

				skr_shader_var_ skr_type = skr_shader_var_none;
				switch (type_desc.Type) {
				case D3D_SVT_FLOAT:  skr_type = skr_shader_var_float;  break;
				case D3D_SVT_DOUBLE: skr_type = skr_shader_var_double; break;
				case D3D_SVT_INT:    skr_type = skr_shader_var_int;    break;
				case D3D_SVT_UINT:   skr_type = skr_shader_var_uint;   break;
				case D3D_SVT_UINT8:  skr_type = skr_shader_var_uint8;  break;
				}
				buffer_list[id].vars[v].type = skr_type;
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
				sprintf_s(buffer_list[id].vars[v].name, _countof(buffer_list[id].vars[v].name), "%s", var_desc.Name);
			}
		} 
		if (bind_desc.Type == D3D_SIT_TEXTURE) {
			int64_t id = texture_list.index_where([](auto &tex, void *data) { 
				return strcmp(tex.name, (char*)data) == 0; 
			}, (void*)bind_desc.Name);
			if (id == -1)
				id = texture_list.add({});

			sprintf_s(texture_list[id].name, _countof(texture_list[id].name), "%s", bind_desc.Name);
			texture_list[id].bind.stage_bits = (skr_stage_)(texture_list[id].bind.stage_bits | stage);
			texture_list[id].bind.slot       = bind_desc.BindPoint;
		}
	}

	buffer_list .trim();
	texture_list.trim();
	out_meta->buffers       =           buffer_list .data;
	out_meta->buffer_count  = (uint32_t)buffer_list .count;
	out_meta->textures      =           texture_list.data;
	out_meta->texture_count = (uint32_t)texture_list.count;

	shader_reflection->Release();
	reflection       ->Release();
}

///////////////////////////////////////////

array_t<const char *> sksc_dxc_build_flags(sksc_settings_t settings, skr_stage_ type, skr_shader_lang_ lang) {
	// https://simoncoenen.com/blog/programming/graphics/DxcCompiling.html

	array_t<const char *> result = {};
	if (lang == skr_shader_lang_spirv) {
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
	case skr_stage_pixel:   result.add(settings.ps_entrypoint); break;
	case skr_stage_vertex:  result.add(settings.vs_entrypoint); break;
	case skr_stage_compute: result.add(settings.cs_entrypoint); break;
	}

	// Target
	result.add("-T");
	switch (type) {
	case skr_stage_vertex:  snprintf(settings.shader_model_str, sizeof(settings.shader_model_str), "vs_6_0", settings.shader_model); result.add(settings.shader_model_str); break;
	case skr_stage_pixel:   snprintf(settings.shader_model_str, sizeof(settings.shader_model_str), "ps_6_0", settings.shader_model); result.add(settings.shader_model_str); break;
	case skr_stage_compute: snprintf(settings.shader_model_str, sizeof(settings.shader_model_str), "cs_6_0", settings.shader_model); result.add(settings.shader_model_str); break;
	}

	// Include folder
	result.add("-I");
	result.add(settings.folder);

	return result;
}

///////////////////////////////////////////

bool sksc_spvc_compile_stage(const skr_shader_file_stage_t *src_stage, skr_shader_lang_ lang, skr_shader_file_stage_t *out_stage, const skr_shader_meta_t *meta) {
	spvc_context context = nullptr;
	spvc_context_create            (&context);
	spvc_context_set_error_callback( context, [](void *userdata, const char *error) {
		printf("GLSL err: %s\n", error);
	}, nullptr);

	spvc_compiler  compiler_glsl = nullptr;
	spvc_parsed_ir ir            = nullptr;
	spvc_context_parse_spirv    (context, (const SpvId*)src_stage->code, src_stage->code_size/sizeof(SpvId), &ir);
	spvc_context_create_compiler(context, SPVC_BACKEND_GLSL, ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &compiler_glsl);

	spvc_resources resources = nullptr;
	spvc_compiler_create_shader_resources(compiler_glsl, &resources);

	const char *lang_name = "GLSL";
	const char *type_name = src_stage->stage == skr_stage_pixel ? "Pixel" : "Vertex";
	//printf("|--%s %s shader--\n", lang_name, type_name);

	// Ensure buffer ids stay the same
	const spvc_reflected_resource *list = nullptr;
	size_t                         count;
	spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_UNIFORM_BUFFER, &list, &count);
	for (size_t i = 0; i < count; i++) {
		for (size_t b = 0; b < meta->buffer_count; b++) {
			const char *name = spvc_compiler_get_name(compiler_glsl, list[i].id);
			if (strcmp(meta->buffers[b].name, name) == 0 || (strcmp(name, "_Globals") == 0 && strcmp(meta->buffers[b].name, "$Globals") == 0)) {
				spvc_compiler_set_decoration(compiler_glsl, list[i].id, SpvDecorationBinding, meta->buffers[b].bind.slot);
				break;
			}
		}
		//printf("| Param b%u : %s\n", spvc_compiler_get_decoration(compiler_glsl, list[i].id, SpvDecorationBinding), spvc_compiler_get_name(compiler_glsl, list[i].id));
	}
	/*spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_SEPARATE_SAMPLERS, &list, &count);
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
	if (lang == skr_shader_lang_glsl_web) {
		spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_GLSL_VERSION, 300);
		spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_GLSL_ES, SPVC_TRUE);
	} else if (lang == skr_shader_lang_glsl) {
		spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_GLSL_VERSION, 320);
		spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_GLSL_ES, SPVC_TRUE);
	}
	//spvc_compiler_options_set_uint(options, SPVC_COMPILER_OPTION_GLSL_VERSION, 450);
	//spvc_compiler_options_set_bool(options, SPVC_COMPILER_OPTION_GLSL_ES, SPVC_FALSE);
	spvc_compiler_install_compiler_options(compiler_glsl, options);
	if (src_stage->stage == skr_stage_vertex) {

		spvc_compiler_add_header_line(compiler_glsl, "#ifdef GL_AMD_vertex_shader_layer");
		spvc_compiler_add_header_line(compiler_glsl, "#extension GL_AMD_vertex_shader_layer : enable");
		spvc_compiler_add_header_line(compiler_glsl, "#elif defined(GL_NV_viewport_array2)");
		spvc_compiler_add_header_line(compiler_glsl, "#extension GL_NV_viewport_array2 : enable");
		spvc_compiler_add_header_line(compiler_glsl, "#else");
		spvc_compiler_add_header_line(compiler_glsl, "#define gl_Layer int __gl_Layer");
		spvc_compiler_add_header_line(compiler_glsl, "#endif");
	}

	spvc_variable_id id;
	spvc_compiler_build_dummy_sampler_for_combined_images(compiler_glsl, &id);

	// combiner samplers/textures for OpenGL/ES
	spvc_compiler_build_combined_image_samplers(compiler_glsl);

	// Make sure sampler names stay the same in GLSL
	const spvc_combined_image_sampler *samplers = nullptr;
	spvc_compiler_get_combined_image_samplers(compiler_glsl, &samplers, &count);
	spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_SEPARATE_IMAGE, &list, &count);
	for (size_t i = 0; i < count; i++) {
		spvc_compiler_set_name      (compiler_glsl, samplers[i].combined_id, list[i].name);
		spvc_compiler_set_decoration(compiler_glsl, samplers[i].combined_id, SpvDecorationBinding, spvc_compiler_get_decoration(compiler_glsl, list[i].id, SpvDecorationBinding));
	}

	if (src_stage->stage == skr_stage_vertex || src_stage->stage == skr_stage_pixel) {
		size_t             off = src_stage->stage == skr_stage_vertex ? 3 : 2;
		spvc_resource_type res = src_stage->stage == skr_stage_vertex
			? SPVC_RESOURCE_TYPE_STAGE_OUTPUT
			: SPVC_RESOURCE_TYPE_STAGE_INPUT;
		
		spvc_resources_get_resource_list_for_type(resources, res, &list, &count);
		for (size_t i = 0; i < count; i++) {
			char fs_name[64];
			sprintf_s(fs_name, "fs%s", list[i].name+off);
			spvc_compiler_set_name(compiler_glsl, list[i].id, fs_name);
		}
	}

	const char *result = nullptr;
	if (spvc_compiler_compile(compiler_glsl, &result) != SPVC_SUCCESS) {
		spvc_context_destroy(context);
		return false;
	}

	out_stage->stage     = src_stage->stage;
	out_stage->language  = lang;
	out_stage->code_size = (uint32_t)strlen(result) + 1;
	out_stage->code      = malloc(out_stage->code_size);
	strcpy_s((char*)out_stage->code, out_stage->code_size, result);

	//printf("%s\n", (char*)out_stage->code);

	// Frees all memory we allocated so far.
	spvc_context_destroy(context);
	return true;
}