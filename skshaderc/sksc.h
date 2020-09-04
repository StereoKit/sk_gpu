// https://simoncoenen.com/blog/programming/graphics/DxcRevised.html
#pragma once

#include <stdint.h>

#define SKR_DIRECT3D11
#include "../sk_gpu.h"

///////////////////////////////////////////

typedef struct sksc_settings_t {
	bool replace_ext;
	bool debug;
	bool row_major;
	int  optimize;
	wchar_t folder[512];
	const wchar_t *vs_entrypoint;
	const wchar_t *ps_entrypoint;
	const wchar_t *shader_model;
	wchar_t shader_model_str[16];
} sksc_settings_t;

///////////////////////////////////////////

void sksc_init    ();
void sksc_shutdown();
bool sksc_compile (char *filename, char *hlsl_text, sksc_settings_t *settings, skr_shader_file_t *out_file);
void sksc_save    (char *filename, const skr_shader_file_t *file);