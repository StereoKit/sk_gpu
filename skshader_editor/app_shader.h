#pragma once

#include "../sk_gpu.h"

void app_shader_init();
void app_shader_update_hlsl(const char *text);
skg_pipeline_t *app_shader_get_pipeline();

void app_shader_show_log();
void app_shader_show_meta();

enum engine_val_ {
	engine_val_named,
	engine_val_time,
	engine_val_matrix_world,
	engine_val_matrix_view,
	engine_val_matrix_projection,
	engine_val_matrix_view_projection,
};

void app_shader_set_engine_val(engine_val_ type, void *value);
void app_shader_set_named_val (const char *name, void *value);
void app_shader_map_engine_val(engine_val_ type, const char *shader_param_name);
void app_shader_map_clear     ();