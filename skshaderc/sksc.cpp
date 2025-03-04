#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
///////////////////////////////////////////

#include "sksc.h"
#include "_sksc.h"
#include "../sk_gpu.h"

#include "array.h"

#if defined(SKSC_D3D11)
	#pragma comment(lib,"d3dcompiler.lib")
	#include <windows.h>
	#include <d3dcompiler.h>
#endif

#include <glslang/Public/ShaderLang.h>
#include <spirv-tools/optimizer.hpp>

#include <spirv_glsl.hpp>
#include <spirv_hlsl.hpp>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

///////////////////////////////////////////

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

struct sksc_meta_item_t {
	char name [32];
	char tag  [64];
	char value[512];
	int32_t row, col;
};

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

void sksc_init() {
	glslang::InitializeProcess();
}

///////////////////////////////////////////

void sksc_shutdown() {
	glslang::FinalizeProcess();
}

///////////////////////////////////////////


///////////////////////////////////////////

bool sksc_compile(const char *filename, const char *hlsl_text, sksc_settings_t *settings, skg_shader_file_t *out_file) {

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
		//if (src_stage->stage == skg_stage_vertex)
		//	options.ovr_multiview_view_count = 2;
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
		//glsl.add_header_line("#extension GL_OES_sample_variables : enable");
		if (use_external == true && lang == skg_shader_lang_glsl_es) {
			glsl.add_header_line("#extension GL_OES_EGL_image_external_essl3 : enable");
		}
		// Add custom header lines for vertex shaders
		if (src_stage->stage == skg_stage_vertex) {
			//glsl.add_header_line("#extension GL_OVR_multiview2 : require");
			//glsl.add_header_line("layout(num_views = 2) in;");
			glsl.add_header_line("#define gl_Layer int _dummy_gl_layer_var");
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
	spirv_cross::CompilerHLSL* compiler = nullptr;
	try                            { compiler = new spirv_cross::CompilerHLSL((const uint32_t*)spirv_stage->code, spirv_stage->code_size/sizeof(uint32_t)); }
	catch(const std::exception& e) { sksc_log(log_level_err, "[SPIRV-Cross] Failed to create compiler: %s", e.what()); }
	spirv_cross::ShaderResources resources = compiler->get_shader_resources();

	array_t<skg_shader_buffer_t> buffer_list = {};
	buffer_list.data      = ref_meta->buffers;
	buffer_list.capacity  = ref_meta->buffer_count;
	buffer_list.count     = ref_meta->buffer_count;
	array_t<skg_shader_resource_t> resource_list = {};
	resource_list.data     = ref_meta->resources;
	resource_list.capacity = ref_meta->resource_count;
	resource_list.count    = ref_meta->resource_count;

	for (spirv_cross::Resource& buffer : resources.uniform_buffers) {
		// Find or create a buffer
		int64_t id = buffer_list.index_where([](const skg_shader_buffer_t &buff, void *data) { 
			return strcmp(buff.name, (char*)data) == 0; 
		}, (void*)buffer.name.c_str());
		bool is_new = id == -1;
		if (is_new) id = buffer_list.add({});

		// Update the stage of this buffer
		skg_shader_buffer_t *buff = &buffer_list[id];
		buff->bind.stage_bits |= spirv_stage->stage;

		// And skip the rest if we've already seen it
		if (!is_new) continue;

		uint32_t              type_id   = buffer.base_type_id;
		spirv_cross::SPIRType type      = compiler->get_type(type_id);
		size_t                type_size = compiler->get_declared_struct_size(type);
		uint32_t              count     = (uint32_t)type.member_types.size();

		buff->size               = (uint32_t)(type_size%16==0? type_size : (type_size/16 + 1)*16);
		buff->bind.slot          = compiler->get_decoration(buffer.id, spv::DecorationBinding);
		buff->bind.stage_bits    = spirv_stage->stage;
		buff->bind.register_type = skg_register_constant;
		buff->var_count          = count;
		buff->vars               = (skg_shader_var_t*)malloc(count * sizeof(skg_shader_var_t));
		memset (buff->vars, 0, count * sizeof(skg_shader_var_t));
		strncpy(buff->name, buffer.name.c_str(), sizeof(buff->name));

		for (uint32_t m = 0; m < count; m++) {
			uint32_t              member_type_id = type.member_types[m];
			spirv_cross::SPIRType member_type    = compiler->get_type       (member_type_id);
			std::string           member_name    = compiler->get_member_name(type_id, m);
			uint32_t              member_offset  = compiler->type_struct_member_offset      (type, m);
			size_t                member_size    = compiler->get_declared_struct_member_size(type, m);
			
			uint32_t dimensions = (uint32_t)member_type.array.size();
			int32_t  dim_size   = 1;
			for (uint32_t d = 0; d < dimensions; d++) {
				dim_size = dim_size * member_type.array[d];
			}
			
			strncpy(buff->vars[m].name, member_name.c_str(), sizeof(buff->vars[m].name));
			buff->vars[m].offset     = member_offset;
			buff->vars[m].size       = (uint32_t)member_size;
			buff->vars[m].type_count = dim_size * member_type.vecsize * member_type.columns;
			
			if (buff->vars[m].type_count == 0)
				buff->vars[m].type_count = 1;
			
			switch (member_type.basetype) {
				case spirv_cross::SPIRType::SByte:
				case spirv_cross::SPIRType::Short:
				case spirv_cross::SPIRType::Int:
				case spirv_cross::SPIRType::Int64:  buff->vars[m].type = skg_shader_var_int;    break;
				case spirv_cross::SPIRType::UByte:  buff->vars[m].type = skg_shader_var_uint8;  break;
				case spirv_cross::SPIRType::UShort:
				case spirv_cross::SPIRType::UInt:
				case spirv_cross::SPIRType::UInt64: buff->vars[m].type = skg_shader_var_uint;   break;
				case spirv_cross::SPIRType::Half:
				case spirv_cross::SPIRType::Float:  buff->vars[m].type = skg_shader_var_float;  break;
				case spirv_cross::SPIRType::Double: buff->vars[m].type = skg_shader_var_double; break;
				default:                            buff->vars[m].type = skg_shader_var_none;   break;
			}
		}
		
		if (strcmp(buff->name, "$Global") == 0) {
			ref_meta->global_buffer_id = (int32_t)id;
		}
	}

	// Find textures
	for (const spirv_cross::Resource& image : resources.separate_images) {
		std::string name = compiler->get_name(image.id);
		int64_t     id   = resource_list.index_where([](auto &tex, void *data) { 
			return strcmp(tex.name, (char*)data) == 0; 
		}, (void*)name.c_str());
		if (id == -1)
			id = resource_list.add({});

		skg_shader_resource_t *tex = &resource_list[id]; 
		tex->bind.slot          = compiler->get_decoration(image.id, spv::DecorationBinding);
		tex->bind.stage_bits   |= spirv_stage->stage;
		tex->bind.register_type = skg_register_resource;
		strncpy(tex->name, name.c_str(), sizeof(tex->name));
	}

	// Look for RWTexture2D
	for (auto& image : resources.storage_images) {
		std::string name = compiler->get_name(image.id);
		int64_t     id   = resource_list.index_where([](auto &tex, void *data) { 
			return strcmp(tex.name, (char*)data) == 0; 
		}, (void*)name.c_str());
		if (id == -1)
			id = resource_list.add({});

		skg_shader_resource_t *tex = &resource_list[id];
		tex->bind.slot             = compiler->get_decoration(image.id, spv::DecorationBinding);
		tex->bind.stage_bits      |= spirv_stage->stage;
		tex->bind.register_type    = skg_register_readwrite;
		strncpy(tex->name, name.c_str(), sizeof(tex->name));
	}

	// Look for RWStructuredBuffers and StructuredBuffers
	for (auto& buffer : resources.storage_buffers) {
		std::string name     = compiler->get_name(buffer.id);
		bool        readonly = compiler->has_member_decoration(buffer.base_type_id, 0, spv::DecorationNonWritable);
		
		int64_t id = resource_list.index_where([](auto &tex, void *data) { 
			return strcmp(tex.name, (char*)data) == 0; 
		}, (void*)name.c_str());
		if (id == -1)
			id = resource_list.add({});

		skg_shader_resource_t *tex = &resource_list[id];
		tex->bind.slot          = compiler->get_decoration(buffer.id, spv::DecorationBinding);
		tex->bind.stage_bits   |= spirv_stage->stage;
		tex->bind.register_type = readonly ? skg_register_resource : skg_register_readwrite;
		strncpy(tex->name, name.c_str(), sizeof(tex->name));
	}

	ref_meta->buffers        = buffer_list.data;
	ref_meta->buffer_count   = (uint32_t)buffer_list.count;
	ref_meta->resources      = resource_list.data;
	ref_meta->resource_count = (uint32_t)resource_list.count;

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