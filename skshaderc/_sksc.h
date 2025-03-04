#pragma once

#include "sksc.h"
#include "../sk_gpu.h"

enum compile_result_ {
	compile_result_success = 1,
	compile_result_fail    = 0,
	compile_result_skip    = -1,
};

compile_result_ sksc_glslang_compile_shader(const char *hlsl, sksc_settings_t *settings, skg_stage_ type, skg_shader_lang_ lang, skg_shader_file_stage_t *out_stage, skg_shader_meta_t *out_meta);
bool            sksc_d3d11_compile_shader  (const char *filename, const char *hlsl_text, sksc_settings_t *settings, skg_stage_ type, skg_shader_file_stage_t *out_stage, skg_shader_meta_t *ref_meta);