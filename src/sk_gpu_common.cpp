#include "sk_gpu_dev.h"
///////////////////////////////////////////
// Common Code                           //
///////////////////////////////////////////

#include <malloc.h>
#include <string.h>

void (*_skr_log)(const char *text);
void skr_log_callback(void (*callback)(const char *text)) {
	_skr_log = callback;
}
void skr_log(const char *text) {
	if (_skr_log) _skr_log(text);
}

///////////////////////////////////////////

skr_shader_file_t  skr_shader_file_load(const char *file) {
	return {};
}

///////////////////////////////////////////

skr_shader_file_t  skr_shader_file_load_mem(void *data, size_t size) {
	return {};
}

///////////////////////////////////////////

skr_shader_stage_t skr_shader_file_create_stage(const skr_shader_file_t *file, skr_shader_ stage) {
	skr_shader_lang_ language;
#if defined(SKR_DIRECT3D11) || defined(SKR_DIRECT3D12)
	language = skr_shader_lang_hlsl;
#elif defined(SKR_OPENGL)
	language = skr_shader_lang_glsl;
#elif defined(SKR_VULKAN)
	language = skr_shader_lang_spirv;
#endif

	for (uint32_t i = 0; i < file->stage_count; i++) {
		if (file->stages[i].language == language && file->stages[i].stage == stage)
			return skr_shader_stage_create(file->stages[i].code, file->stages[i].code_size, stage);
	}
	skr_log("Couldn't find a shader stage in shader file!");
	return {};
}

///////////////////////////////////////////

void skr_shader_file_destroy(skr_shader_file_t *file) {
	for (uint32_t i = 0; i < file->stage_count; i++) {
		free(file->stages[i].code);
	}
	free(file->stages);
	skr_shader_meta_release(file->meta);
	*file = {};
}

///////////////////////////////////////////

void skr_shader_meta_reference(skr_shader_meta_t *meta) {
	meta->references += 1;
}

///////////////////////////////////////////

void skr_shader_meta_release(skr_shader_meta_t *meta) {
	meta->references -= 1;
	if (meta->references == 0) {
		for (uint32_t i = 0; i < meta->buffer_count; i++) {
			for (uint32_t t = 0; t < meta->buffers[t].var_count; t++) {
				free(meta->buffers[i].vars[t].extra);
			}
			free(meta->buffers[i].vars);
			free(meta->buffers[i].defaults);
		}
		for (uint32_t i = 0; i < meta->texture_count; i++) {
			free(meta->textures[i].extra);
		}
		free(meta->buffers);
		free(meta->textures);
		memset(meta, 0, sizeof(skr_shader_meta_t));
	}
}