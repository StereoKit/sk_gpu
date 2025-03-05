#pragma once

#include "sksc.h"
#include "../sk_gpu.h"
#include "array.h"

enum compile_result_ {
	compile_result_success = 1,
	compile_result_fail    = 0,
	compile_result_skip    = -1,
};

struct sksc_meta_item_t {
	char name [32];
	char tag  [64];
	char value[512];
	int32_t row, col;
};

void                      sksc_glslang_init          ();
void                      sksc_glslang_shutdown      ();
compile_result_           sksc_hlsl_to_spirv         (const char *hlsl,                            const sksc_settings_t *settings, skg_stage_ type, skg_shader_file_stage_t *out_stage);
bool                      sksc_hlsl_to_bytecode      (const char *filename, const char *hlsl_text, const sksc_settings_t *settings, skg_stage_ type, skg_shader_file_stage_t *out_stage, skg_shader_meta_t *ref_meta);

array_t<sksc_meta_item_t> sksc_meta_find_defaults    (const char *hlsl_text);
void                      sksc_meta_assign_defaults  (array_t<sksc_meta_item_t> items, skg_shader_meta_t *ref_meta);
bool                      sksc_meta_check_dup_buffers(const skg_shader_meta_t *ref_meta);
bool                      sksc_spirv_to_meta         (const skg_shader_file_stage_t *spirv_stage, skg_shader_meta_t *meta);

bool                      sksc_spirv_to_glsl         (const skg_shader_file_stage_t *src_stage, const sksc_settings_t *settings, skg_shader_lang_ lang, skg_shader_file_stage_t *out_stage, const skg_shader_meta_t *meta, array_t<sksc_meta_item_t> var_meta);
