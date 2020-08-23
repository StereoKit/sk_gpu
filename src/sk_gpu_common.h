#pragma once
///////////////////////////////////////////
// API independant functions             //
///////////////////////////////////////////

typedef enum skr_shader_lang_ {
	skr_shader_lang_hlsl,
	skr_shader_lang_spirv,
	skr_shader_lang_glsl,
} skr_shader_lang_;

typedef struct skr_shader_file_stage_t {
	skr_shader_lang_ language;
	skr_shader_      stage;
	size_t           code_size;
	void            *code;
} skr_shader_file_stage_t;

typedef struct skr_shader_meta_var_t {
	char     name[32];
	char     extra[64];
	size_t   offset;
	size_t   size;
} skr_shader_meta_var_t;

typedef struct skr_shader_meta_buffer_t {
	char              name[32];
	skr_shader_bind_t bind;
	size_t            size;
	void             *defaults;
	uint32_t               var_count;
	skr_shader_meta_var_t *vars;
} skr_shader_meta_buffer_t;

typedef struct skr_shader_meta_texture_t {
	char              name [32];
	char              extra[64];
	skr_shader_bind_t bind;
	size_t            size;
} skr_shader_meta_texture_t;

typedef struct skr_shader_meta_t {
	char                       name[256];
	uint32_t                   buffer_count;
	skr_shader_meta_buffer_t  *buffers;
	uint32_t                   texture_count;
	skr_shader_meta_texture_t *textures;
	int32_t                    references;
} skr_shader_meta_t;

typedef struct skr_shader_file_t {
	skr_shader_meta_t       *meta;
	uint32_t                 stage_count;
	skr_shader_file_stage_t *stages;
} skr_shader_file_t;

///////////////////////////////////////////

void               skr_log(const char *text);

bool               skr_shader_file_load        (const char *file, skr_shader_file_t *out_file);
bool               skr_shader_file_load_mem    (void *data, size_t size, skr_shader_file_t *out_file);
skr_shader_stage_t skr_shader_file_create_stage(const skr_shader_file_t *file, skr_shader_ stage);
void               skr_shader_file_destroy     (      skr_shader_file_t *file);

void               skr_shader_meta_reference   (      skr_shader_meta_t *meta);
void               skr_shader_meta_release     (      skr_shader_meta_t *meta);
skr_shader_bind_t  skr_shader_meta_get_tex_bind(const skr_shader_meta_t *meta, const char *name);
