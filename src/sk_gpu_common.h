#pragma once
///////////////////////////////////////////
// API independant functions             //
///////////////////////////////////////////

typedef enum {
	skg_shader_lang_hlsl,
	skg_shader_lang_spirv,
	skg_shader_lang_glsl,
	skg_shader_lang_glsl_web,
} skg_shader_lang_;

typedef struct {
	skg_shader_lang_ language;
	skg_stage_       stage;
	uint32_t         code_size;
	void            *code;
} skg_shader_file_stage_t;

typedef struct {
	skg_shader_meta_t       *meta;
	uint32_t                 stage_count;
	skg_shader_file_stage_t *stages;
} skg_shader_file_t;

///////////////////////////////////////////

void               skg_log                     (skg_log_ level, const char *text);
bool               skg_read_file               (const char *filename, void **out_data, size_t *out_size);
uint64_t           skg_hash                    (const char *string);

// For Hue: 0 is red, 0.1667 is yellow, 0.3333 is green, 0.5 is cyan, 0.6667 
// is blue, 0.8333 is magenta, and 1 is red again!
skg_color32_t      skg_hsv32                   (float h, float s, float v, float a);
// For Hue: 0 is red, 0.1667 is yellow, 0.3333 is green, 0.5 is cyan, 0.6667 
// is blue, 0.8333 is magenta, and 1 is red again!
skg_color128_t     skg_hsv128                  (float h, float s, float v, float a);

bool               skg_shader_file_verify      (const void *file_memory, size_t file_size, uint16_t *out_version, char *out_name, size_t out_name_size);
bool               skg_shader_file_load_memory (const void *file_memory, size_t file_size, skg_shader_file_t *out_file);
bool               skg_shader_file_load        (const char *file, skg_shader_file_t *out_file);
skg_shader_stage_t skg_shader_file_create_stage(const skg_shader_file_t *file, skg_stage_ stage);
void               skg_shader_file_destroy     (      skg_shader_file_t *file);

void               skg_shader_meta_reference   (skg_shader_meta_t *meta);
void               skg_shader_meta_release     (skg_shader_meta_t *meta);