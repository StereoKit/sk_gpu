#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX
///////////////////////////////////////////

#include "sksc.h"
#include "_sksc.h"
#include "../sk_gpu.h"

#include "array.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

///////////////////////////////////////////

void sksc_log_shader_info(const skg_shader_file_t *file);

///////////////////////////////////////////

void sksc_init() {
	sksc_glslang_init();
}

///////////////////////////////////////////

void sksc_shutdown() {
	sksc_glslang_shutdown();
}

///////////////////////////////////////////

bool sksc_compile(const char *filename, const char *hlsl_text, sksc_settings_t *settings, skg_shader_file_t *out_file) {
	*out_file = {};
	 out_file->meta = (skg_shader_meta_t*)malloc(sizeof(skg_shader_meta_t));
	*out_file->meta = {};
	 out_file->meta->references = 1;

	array_t<skg_shader_file_stage_t> stages   = {};
	array_t<sksc_meta_item_t>        var_meta = sksc_meta_find_defaults(hlsl_text);

	skg_stage_ compile_stages[3] = { skg_stage_vertex, skg_stage_pixel, skg_stage_compute };
	char      *entrypoints   [3] = { settings->vs_entrypoint, settings->ps_entrypoint, settings->cs_entrypoint };
	for (size_t i = 0; i < sizeof(compile_stages)/sizeof(compile_stages[0]); i++) {
		if (entrypoints[i][0] == 0)
			continue;

		// SPIRV is needed regardless, since we use it for reflection!
		skg_shader_file_stage_t spirv_stage  = {};
		compile_result_         spirv_result = sksc_hlsl_to_spirv(hlsl_text, settings, compile_stages[i], &spirv_stage);
		if (spirv_result == compile_result_fail) {
			sksc_log(log_level_err, "SPIRV compile failed");
			return false;
		} else if (spirv_result == compile_result_skip)
			continue;
		sksc_spirv_to_meta(&spirv_stage, out_file->meta);

		//// SPIRV ////

		if (settings->target_langs[skg_shader_lang_spirv]) {
			stages.add(spirv_stage);
		}

		//// HLSL ////

		if (settings->target_langs[skg_shader_lang_hlsl]) {
			stages.add({});
#if defined(SKSC_D3D11)
			if (!sksc_hlsl_to_bytecode(filename, hlsl_text, settings, compile_stages[i], &stages.last(), out_file->meta)) {
				sksc_log(log_level_err, "HLSL shader compile failed");
				return false;
			}
#else
			skg_shader_file_stage_t *hlsl_stage = &stages.last();
			hlsl_stage->language  = skg_shader_lang_hlsl;
			hlsl_stage->stage     = compile_stages[i];
			hlsl_stage->code_size = strlen(hlsl_text) + 1;
			hlsl_stage->code      = malloc(hlsl_stage->code_size);
			memcpy(hlsl_stage->code, hlsl_text, hlsl_stage->code_size);

			sksc_log(log_level_warn, "HLSL shader compiler not available in this build! Shaders on windows may load slowly.");
#endif
		}

		//// GLSL ////

		if (settings->target_langs[skg_shader_lang_glsl]) {
			stages.add({});
			if (!sksc_spirv_to_glsl(&spirv_stage, settings, skg_shader_lang_glsl, &stages.last(), out_file->meta, var_meta)) {
				sksc_log(log_level_err, "GLSL shader compile failed");
				return false;
			}
		}

		//// GLSL ES ////

		if (settings->target_langs[skg_shader_lang_glsl_es]) {
			stages.add({});
			if (!sksc_spirv_to_glsl(&spirv_stage, settings, skg_shader_lang_glsl_es, &stages.last(), out_file->meta, var_meta)) {
				sksc_log(log_level_err, "GLES shader compile failed");
				return false;
			}
		}

		//// GLSL Web ////

		if (settings->target_langs[skg_shader_lang_glsl_web] && compile_stages[i] != skg_stage_compute) {
			stages.add({});
			if (!sksc_spirv_to_glsl(&spirv_stage, settings, skg_shader_lang_glsl_web, &stages.last(), out_file->meta, var_meta)) {
				sksc_log(log_level_err, "GLSL web shader compile failed");
				return false;
			}
		}

		if (!settings->target_langs[skg_shader_lang_spirv])
			free(spirv_stage.code);
	}
	
	sksc_meta_assign_defaults(var_meta, out_file->meta);
	out_file->stage_count = (uint32_t)stages.count;
	out_file->stages      = stages.data;

	if (!settings->silent_info) {
		sksc_log_shader_info(out_file);
	}

	if (!sksc_meta_check_dup_buffers(out_file->meta)) {
		sksc_log(log_level_err, "Found constant buffers re-using slot ids");
		return false;
	}

	return true;
}

///////////////////////////////////////////

void sksc_log_shader_info(const skg_shader_file_t *file) {
	const skg_shader_meta_t *meta = file->meta;
	
	sksc_log(log_level_info, " ________________");
	// Write out our reflection information

	// A quick summary of performance
	sksc_log(log_level_info, "|--Performance--");
	if (meta->ops_vertex.total > 0 || meta->ops_pixel.total > 0)
	sksc_log(log_level_info, "| Instructions |  all | tex | flow |");
	if (meta->ops_vertex.total > 0) {
		sksc_log(log_level_info, "|       Vertex | %4d | %3d | %4d |",
			meta->ops_vertex.total,
			meta->ops_vertex.tex_read,
			meta->ops_vertex.dynamic_flow);
	} 
	if (meta->ops_pixel.total > 0) {
		sksc_log(log_level_info, "|        Pixel | %4d | %3d | %4d |",
			meta->ops_pixel.total, 
			meta->ops_pixel.tex_read, 
			meta->ops_pixel.dynamic_flow);
	}

	// List of all the buffers
	sksc_log(log_level_info, "|--Buffer Info--");
	for (size_t i = 0; i < meta->buffer_count; i++) {
		skg_shader_buffer_t *buff = &meta->buffers[i];
		sksc_log(log_level_info, "|  %s - %u bytes", buff->name, buff->size);
		for (size_t v = 0; v < buff->var_count; v++) {
			skg_shader_var_t *var = &buff->vars[v];
			const char *type_name = "misc";
			switch (var->type) {
			case skg_shader_var_double: type_name = "dbl";   break;
			case skg_shader_var_float:  type_name = "flt";   break;
			case skg_shader_var_int:    type_name = "int";   break;
			case skg_shader_var_uint:   type_name = "uint";  break;
			case skg_shader_var_uint8:  type_name = "uint8"; break;
			}
			sksc_log(log_level_info, "|    %-15s: +%-4u [%5u] - %s%u", var->name, var->offset, var->size, type_name, var->type_count);
		}
	}

	// Show the vertex shader's input format
	if (meta->vertex_input_count > 0) {
		sksc_log(log_level_info, "|--Mesh Input--");
		for (int32_t i=0; i<meta->vertex_input_count; i++) {
			const char *format   = "NA";
			const char *semantic = "NA";
			switch (meta->vertex_inputs[i].format) {
				case skg_fmt_f32:  format = "float"; break;
				case skg_fmt_i32:  format = "int  "; break;
				case skg_fmt_ui32: format = "uint "; break;
			}
			switch (meta->vertex_inputs[i].semantic) {
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
			sksc_log(log_level_info, "|  %s : %s%d", format, semantic, meta->vertex_inputs[i].semantic_slot);
		}
	} 

	// Only log buffer binds for the stages of a single language. Doesn't
	// matter which.
	skg_shader_lang_ stage_lang = file->stage_count > 0 ? file->stages[0].language : skg_shader_lang_hlsl;
	for (uint32_t s = 0; s < file->stage_count; s++) {
		const skg_shader_file_stage_t* stage = &file->stages[s];

		if (stage->language != stage_lang)
			continue;

		const char *stage_name = "";
		switch (stage->stage) {
		case skg_stage_vertex:  stage_name = "Vertex";  break;
		case skg_stage_pixel:   stage_name = "Pixel";   break;
		case skg_stage_compute: stage_name = "Compute"; break;
		}
		sksc_log(log_level_info, "|--%s Shader--", stage_name);
		for (uint32_t i = 0; i < meta->buffer_count; i++) {
			skg_shader_buffer_t *buff = &meta->buffers[i];
			if (buff->bind.stage_bits & stage->stage) {
				sksc_log(log_level_info, "|  b%u : %s", buff->bind.slot, buff->name);
			}
		}
		for (uint32_t i = 0; i < meta->resource_count; i++) {
			skg_shader_resource_t *tex = &meta->resources[i];
			if (tex->bind.stage_bits & stage->stage) {
				sksc_log(log_level_info, "|  %c%u : %s", tex->bind.register_type == skg_register_resource ? 't' : 'u', tex->bind.slot, tex->name);
			}
		}
	}
	sksc_log(log_level_info, "|________________");
}

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
