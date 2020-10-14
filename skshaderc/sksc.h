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
	bool output_header;
	int  optimize;
	char folder[512];
	char vs_entrypoint[64];
	char ps_entrypoint[64];
	char cs_entrypoint[64];
	char shader_model[64];
	char shader_model_str[16];
} sksc_settings_t;

///////////////////////////////////////////

void sksc_init       ();
void sksc_shutdown   ();
bool sksc_compile    (char *filename, char *hlsl_text, sksc_settings_t *settings, skr_shader_file_t *out_file);
void sksc_save       (char *filename, const skr_shader_file_t *file);
void sksc_save_header(char *sks_file);