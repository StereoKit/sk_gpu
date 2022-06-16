// https://simoncoenen.com/blog/programming/graphics/DxcRevised.html
#pragma once

#include <stdint.h>
#include "../sk_gpu.h"

#if defined(_WIN32)
#define SKSC_D3D11
#endif
#define SKSC_SPIRV_GLSLANG
//#define SKSC_SPIRV_DXC

///////////////////////////////////////////

typedef struct sksc_settings_t {
	bool replace_ext;
	bool debug;
	bool row_major;
	bool output_header;
	bool output_zipped;
	bool silent_info;
	bool silent_err;
	bool silent_warn;
	bool only_if_changed;
	int  optimize;
	char folder[512];
	char *out_folder;
	char vs_entrypoint[64];
	bool vs_entry_require;
	char ps_entrypoint[64];
	bool ps_entry_require;
	char cs_entrypoint[64];
	bool cs_entry_require;
	char shader_model[64];
	char shader_model_str[16];
	int32_t gl_version;
	char  **include_folders;
	int32_t include_folder_ct;
	bool target_langs[5];
} sksc_settings_t;

typedef struct sksc_log_item_t {
	int32_t     level;
	int32_t     line;
	int32_t     column;
	const char *text;
} sksc_log_item_t;

typedef enum log_level_ {
	log_level_info,
	log_level_warn,
	log_level_err,
	log_level_err_pre,
} log_level_;

///////////////////////////////////////////

void            sksc_init       ();
void            sksc_shutdown   ();
bool            sksc_compile    (const char *filename, const char *hlsl_text, sksc_settings_t *settings, skg_shader_file_t *out_file);
void            sksc_build_file (const skg_shader_file_t *file, void **out_data, size_t *out_size);
void            sksc_log        (log_level_ level, const char* text, ...);
void            sksc_log_print  (const char* file, const sksc_settings_t* settings);
void            sksc_log_clear  ();
int32_t         sksc_log_count  ();
sksc_log_item_t sksc_log_get    (int32_t index);