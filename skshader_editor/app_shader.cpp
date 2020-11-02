#define _CRT_SECURE_NO_WARNINGS

#include "app_shader.h"
#include "imgui/imgui.h"
#include "array.h"

#include "../skshaderc/sksc.h"
#include <stdio.h>

skg_shader_t   app_shader   = {};
skg_pipeline_t app_pipeline = {};
bool           app_pipeline_valid = false;

struct shader_buffer_t {
	skg_buffer_t buffer;
	skg_bind_t   bind;
	void        *data;
	int32_t      data_size;
};
array_t<shader_buffer_t> shader_buffers = {};

struct engine_val_t {
	engine_val_ type;
	const char *name;
	void       *value;
	int32_t     val_size;

	int32_t buffer_id;
	int32_t offset;
};
array_t<engine_val_t> engine_vals = {};

void    app_shader_remap();
void    app_shader_rebuild_buffers();
void    app_shader_update_buffers();
int32_t app_shader_engine_val_size(engine_val_ type);

int32_t app_shader_engine_val_size(engine_val_ type) {
	switch (type) {
	case engine_val_matrix_projection:
	case engine_val_matrix_view:
	case engine_val_matrix_world:
	case engine_val_matrix_view_projection: return sizeof(float[16]);
	case engine_val_time:                   return sizeof(float);
	default: printf("Unsized engine val type\n"); return 0;
	}
}

void app_shader_init() {
	app_shader_map_engine_val(engine_val_matrix_view, "view");
	app_shader_map_engine_val(engine_val_matrix_projection, "proj");
	app_shader_map_engine_val(engine_val_matrix_view_projection, "viewproj");
	app_shader_map_engine_val(engine_val_time, "time");
}

void app_shader_update_hlsl(const char *text) {
	printf("Updating hlsl\n");
	skg_shader_file_t file = {};
	sksc_settings_t settings = {};
	settings.debug         = false;
	settings.optimize      = 3;
	settings.replace_ext   = true;
	settings.output_header = false;
	settings.row_major     = false;
	settings.silent_err    = false;
	settings.silent_info   = false;
	settings.silent_warn   = false;
	if (settings.shader_model[0] == 0)
		strncpy(settings.shader_model, "5_0", sizeof(settings.shader_model));

	// If no entrypoints were provided, then these are the defaults!
	if (settings.ps_entrypoint[0] == 0 && settings.vs_entrypoint[0] == 0 && settings.cs_entrypoint[0] == 0) {
		strncpy(settings.ps_entrypoint, "ps", sizeof(settings.ps_entrypoint));
		strncpy(settings.vs_entrypoint, "vs", sizeof(settings.vs_entrypoint));
	}

	sksc_init();
	sksc_log_clear();
	if (sksc_compile("[err]", text, &settings, &file)) {
		skg_shader_stage_t vs     = skg_shader_file_create_stage(&file, skg_stage_vertex);
		skg_shader_stage_t ps     = skg_shader_file_create_stage(&file, skg_stage_pixel);
		skg_shader_stage_t cs     = skg_shader_file_create_stage(&file, skg_stage_compute);
		skg_shader_t       result = skg_shader_create_manual( file.meta, vs, ps, cs );

		skg_shader_stage_destroy(&vs);
		skg_shader_stage_destroy(&ps);
		skg_shader_stage_destroy(&cs);
		skg_shader_file_destroy (&file);

		if (app_pipeline_valid) {
			skg_shader_destroy  (&app_shader);
			skg_pipeline_destroy(&app_pipeline);
		}
		app_shader   = result;
		app_pipeline = skg_pipeline_create(&app_shader);
		app_pipeline_valid = true;

		skg_pipeline_set_depth_test(&app_pipeline, skg_depth_test_always);

		app_shader_remap();
		app_shader_rebuild_buffers();
	}
	sksc_shutdown();
}
skg_pipeline_t *app_shader_get_pipeline() {
	app_shader_update_buffers();
	return app_pipeline_valid ? &app_pipeline : nullptr;
}

void app_shader_show_log() {
	ImGui::Begin("Log");
	for (size_t i = 0; i < sksc_log_count(); i++) {
		sksc_log_item_t item = sksc_log_get(i);
		if (item.level > 0)
			ImGui::Text(item.text);
	}
	ImGui::End();
}

void app_shader_show_meta() {
	ImGui::Begin("Meta");
	
	if (app_shader.meta) {
		ImGui::Text("Shader: %s", app_shader.meta->name);
		ImGui::Separator();

		for (size_t i = 0; i < app_shader.meta->buffer_count; i++) {
			skg_shader_buffer_t *b = &app_shader.meta->buffers[i];
			if (ImGui::CollapsingHeader(b->name,ImGuiTreeNodeFlags_DefaultOpen)) {
				for (size_t v = 0; v < b->var_count; v++) {

					const char *name = "";
					switch (b->vars[v].type) {
					case skg_shader_var_double: name = "double"; break;
					case skg_shader_var_float:  name = "float";  break;
					case skg_shader_var_int:    name = "int";    break;
					case skg_shader_var_uint:   name = "uint";   break;
					case skg_shader_var_uint8:  name = "byte";   break;
					}
					if (app_shader_is_engine_val(b->vars[v].name)) {
						ImGui::LabelText(b->vars[v].name, "engine");
					} else {
						if (b->vars[v].type == skg_shader_var_float) {
							if      (b->vars[v].type_count == 1) { ImGui::InputFloat (b->vars[v].name, (float*)app_shader_get_named_val(b->vars[v].name)); }
							else if (b->vars[v].type_count == 2) { ImGui::InputFloat2(b->vars[v].name, (float*)app_shader_get_named_val(b->vars[v].name)); }
							else if (b->vars[v].type_count == 3) { ImGui::InputFloat3(b->vars[v].name, (float*)app_shader_get_named_val(b->vars[v].name)); }
							else if (b->vars[v].type_count == 4) { ImGui::InputFloat4(b->vars[v].name, (float*)app_shader_get_named_val(b->vars[v].name)); }
						} else {
							ImGui::LabelText(b->vars[v].name, "%s%d", name, b->vars[v].type_count);
						}
					}
				}
			}
		}
	}
	ImGui::End();
}

void app_shader_set_engine_val(engine_val_ type, void *value) {
	int32_t size = app_shader_engine_val_size(type);
	int     id   = engine_vals.index_where(&engine_val_t::type, type);
	if (id != -1) {
		memcpy(engine_vals[id].value, value, size);
	}
}

bool app_shader_is_engine_val(const char *name) {
	int id = -1;
	for (size_t i = 0; i < engine_vals.count; i++) {
		if (strcmp(engine_vals[i].name, name) == 0) {
			return engine_vals[i].type != engine_val_named;
		}
	}
	return false;
}

void app_shader_set_named_val(const char *name, void *value) {
	int id = -1;
	for (size_t i = 0; i < engine_vals.count; i++) {
		if (strcmp(engine_vals[i].name, name) == 0) {
			id = i;
			break;
		}
	}
	if (id == -1 && app_pipeline_valid) {
		skg_shader_var_t *var = nullptr;
		for (size_t b = 0; b < app_shader.meta->buffer_count; b++) {
			skg_shader_buffer_t *buff = &app_shader.meta->buffers[b];
			for (size_t v = 0; v < buff->var_count; v++) {
				if (strcmp(buff->vars[v].name, name) == 0) {
					var = &buff->vars[v];
				}
			}
		}
		if (var) {
			engine_val_t val = {};
			val.type      = engine_val_named;
			val.value     = malloc(var->size);
			val.val_size  = var->size;
			val.name      = (char*)malloc(strlen(name) + 1);
			val.buffer_id = -1;
			snprintf((char*)val.name, strlen(name) + 1, "%s", name);
			id = engine_vals.add(val);

			app_shader_remap();
		}
	}
	if (id != -1)
		memcpy(engine_vals[id].value, value, engine_vals[id].val_size);
}

void *app_shader_get_named_val(const char *name) {
	int id = -1;
	for (size_t i = 0; i < engine_vals.count; i++) {
		if (strcmp(engine_vals[i].name, name) == 0) {
			id = i;
			break;
		}
	}
	if (id == -1) {
		skg_shader_buffer_t *var_buff = nullptr;
		skg_shader_var_t *var = nullptr;
		for (size_t b = 0; b < app_shader.meta->buffer_count; b++) {
			skg_shader_buffer_t *buff = &app_shader.meta->buffers[b];
			for (size_t v = 0; v < buff->var_count; v++) {
				if (strcmp(buff->vars[v].name, name) == 0) {
					var = &buff->vars[v];
					var_buff = buff;
				}
			}
		}
		if (var) {
			engine_val_t val = {};
			val.type      = engine_val_named;
			val.value     = malloc(var->size);
			val.val_size  = var->size;
			val.name      = (char*)malloc(strlen(name) + 1);
			val.buffer_id = -1;
			snprintf((char*)val.name, strlen(name) + 1, "%s", name);
			if (var_buff->defaults) {
				memcpy(val.value, (uint8_t*)var_buff->defaults + var->offset, var->size);
			} else {
				memset(val.value, 0, var->size);
			}
			id = engine_vals.add(val);

			app_shader_remap();
		}
	}
	return engine_vals[id].value;
}

void app_shader_map_engine_val(engine_val_ type, const char *shader_param_name) {
	for (size_t i = 0; i < engine_vals.count; i++) {
		if (strcmp(engine_vals[i].name, shader_param_name) == 0) {
			if (engine_vals[i].type != type) {
				printf("Collision with shader mappings??\n");
			}
			return;
		}
	}
	engine_val_t map = {};
	map.type       = type;
	map.buffer_id  = -1;
	map.val_size   = app_shader_engine_val_size(type);
	map.value      = malloc(map.val_size);
	map.name       = (char *)malloc(strlen(shader_param_name) + 1);
	snprintf((char*)map.name, strlen(shader_param_name) + 1, "%s", shader_param_name);
	engine_vals.add(map);
}

void app_shader_map_clear() {
	for (size_t i = 0; i < engine_vals.count; i++) {
		if (engine_vals[i].type != engine_val_named) {
			free(engine_vals[i].value);
			engine_vals.remove(i);
			i--;
		}
	}
}

void app_shader_remap() {
	for (size_t i = 0; i < engine_vals.count; i++) {
		for (size_t b = 0; b < app_shader.meta->buffer_count; b++) {
			skg_shader_buffer_t *buff = &app_shader.meta->buffers[b]; 
			for (size_t v = 0; v < buff->var_count; v++) {
				if (strcmp(buff->vars[v].name, engine_vals[i].name) == 0) {
					if (engine_vals[i].val_size != buff->vars[v].size) {
						printf("Size mismatch on shader var mapping for '%s'!\n", engine_vals[i].name);
					} else {
						engine_vals[i].buffer_id = b;
						engine_vals[i].offset    = buff->vars[v].offset;
						engine_vals[i].val_size  = buff->vars[v].size;
					}
				}
			}
		}
	}
}

void app_shader_rebuild_buffers() {
	shader_buffers.each([](shader_buffer_t &b) {skg_buffer_destroy(&b.buffer); });
	shader_buffers.free();

	for (size_t i = 0; i < app_shader.meta->buffer_count; i++) {
		skg_shader_buffer_t *buffer = &app_shader.meta->buffers[i];
		shader_buffers.add({ 
			skg_buffer_create(nullptr, 1, buffer->size, skg_buffer_type_constant, skg_use_dynamic), 
			buffer->bind,
			malloc(buffer->size),
			(int32_t)buffer->size });
		memset(shader_buffers.last().data, 0, buffer->size);
		if (buffer->defaults != nullptr) {
			memcpy(shader_buffers.last().data, buffer->defaults, buffer->size);
		}
	}
}

void app_shader_update_buffers() {
	for (size_t i = 0; i < engine_vals.count; i++) {
		if (engine_vals[i].buffer_id != -1) {
			memcpy(shader_buffers[engine_vals[i].buffer_id].data, engine_vals[i].value, engine_vals[i].val_size);
		}
	}
	for (size_t i = 0; i < shader_buffers.count; i++) {
		skg_buffer_set_contents(&shader_buffers[i].buffer, shader_buffers[i].data, shader_buffers[i].data_size);
		skg_buffer_bind(&shader_buffers[i].buffer, shader_buffers[i].bind, 0);
	}
}