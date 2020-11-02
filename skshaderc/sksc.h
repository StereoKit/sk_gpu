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
	bool silent_info;
	bool silent_err;
	bool silent_warn;
	int  optimize;
	char folder[512];
	char vs_entrypoint[64];
	char ps_entrypoint[64];
	char cs_entrypoint[64];
	char shader_model[64];
	char shader_model_str[16];
} sksc_settings_t;

typedef struct sksc_log_item_t {
	int32_t     level;
	int32_t     line;
	int32_t     column;
	const char *text;
} sksc_log_item_t;

///////////////////////////////////////////

void    sksc_init       ();
void    sksc_shutdown   ();
bool    sksc_compile    (const char *filename, const char *hlsl_text, sksc_settings_t *settings, skg_shader_file_t *out_file);
void    sksc_build_file (const skg_shader_file_t *file, void **out_data, size_t *out_size);
void            sksc_log_print  (const sksc_settings_t *settings);
void            sksc_log_clear  ();
int32_t         sksc_log_count  ();
sksc_log_item_t sksc_log_get    (int32_t index);