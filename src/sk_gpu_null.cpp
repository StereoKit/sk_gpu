#include "sk_gpu_dev.h"
#ifdef SKG_NULL
///////////////////////////////////////////
// Null Implementation                   //
///////////////////////////////////////////

skg_shader_stage_t skg_shader_stage_create(const void *file_data, size_t shader_size, skg_stage_ type) {
	skg_shader_stage_t result = {};
	result.type = type;

	return result;
}

///////////////////////////////////////////

void skg_shader_stage_destroy(skg_shader_stage_t *shader) {
}

///////////////////////////////////////////

skg_shader_t skg_shader_create_manual(skg_shader_meta_t *meta, skg_shader_stage_t v_shader, skg_shader_stage_t p_shader, skg_shader_stage_t c_shader) {
	skg_shader_t result = {};
	result.meta = meta;
	skg_shader_meta_reference(result.meta);
	return result;
}

#endif