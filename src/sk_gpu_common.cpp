#include "sk_gpu_dev.h"
///////////////////////////////////////////
// Common Code                           //
///////////////////////////////////////////

#include <malloc.h>
#include <string.h>
#include <stdio.h>

#if __ANDROID__
#include <android/asset_manager.h>
#endif

void (*_skr_log)(const char *text);
void skr_log_callback(void (*callback)(const char *text)) {
	_skr_log = callback;
}
void skr_log(const char *text) {
	if (_skr_log) _skr_log(text);
}

///////////////////////////////////////////

bool (*_skr_read_file)(const char *filename, void **out_data, size_t *out_size);
void skr_file_read_callback(bool (*callback)(const char *filename, void **out_data, size_t *out_size)) {
	_skr_read_file = callback;
}
bool skr_read_file(const char *filename, void **out_data, size_t *out_size) {
	if (_skr_read_file) return _skr_read_file(filename, out_data, out_size);
#if _WIN32
	FILE *fp;
	if (fopen_s(&fp, filename, "rb") != 0 || fp == nullptr) {
		return false;
	}

	fseek(fp, 0L, SEEK_END);
	*out_size = ftell(fp);
	rewind(fp);

	*out_data = malloc(*out_size);
	if (*out_data == nullptr) { *out_size = 0; fclose(fp); return false; }
	fread (*out_data, *out_size, 1, fp);
	fclose(fp);

	return true;
#else
	return false;
#endif
}

///////////////////////////////////////////

bool skr_shader_file_load(const char *file, skr_shader_file_t *out_file) {
	void  *data = nullptr;
	size_t size = 0;

	if (!skr_read_file(file, &data, &size))
		return false;

	bool result = skr_shader_file_load_mem(data, size, out_file);
	free(data);

	return result;
}

///////////////////////////////////////////

bool skr_shader_file_load_mem(void *data, size_t size, skr_shader_file_t *out_file) {
	const char    *prefix  = "SKSHADER";
	const uint16_t version = 1;
	const uint8_t *bytes   = (uint8_t*)data;
	// check the first 5 bytes to see if this is a SKS shader file
	if (size < 10 || memcmp(bytes, prefix, 8) != 0 || memcmp(&bytes[8], &version, sizeof(uint16_t)) != 0)
		return false;

	uint32_t at = 10;
	memcpy(&out_file->stage_count, &bytes[at], sizeof(uint32_t)); at += sizeof(uint32_t);
	out_file->stages = (skr_shader_file_stage_t*)malloc(sizeof(skr_shader_file_stage_t) * out_file->stage_count);

	out_file->meta = (skr_shader_meta_t*)malloc(sizeof(skr_shader_meta_t));
	*out_file->meta = {};
	skr_shader_meta_reference(out_file->meta);
	memcpy( out_file->meta->name,          &bytes[at], sizeof(out_file->meta->name)); at += sizeof(out_file->meta->name);
	memcpy(&out_file->meta->buffer_count,  &bytes[at], sizeof(uint32_t)); at += sizeof(uint32_t);
	memcpy(&out_file->meta->texture_count, &bytes[at], sizeof(uint32_t)); at += sizeof(uint32_t);
	out_file->meta->buffers  = (skr_shader_meta_buffer_t *)malloc(sizeof(skr_shader_meta_buffer_t ) * out_file->meta->buffer_count );
	out_file->meta->textures = (skr_shader_meta_texture_t*)malloc(sizeof(skr_shader_meta_texture_t) * out_file->meta->texture_count);

	for (uint32_t i = 0; i < out_file->meta->buffer_count; i++) {
		skr_shader_meta_buffer_t *buffer = &out_file->meta->buffers[i];
		memcpy( buffer->name,      &bytes[at], sizeof(buffer->name)); at += sizeof(buffer->name);
		memcpy(&buffer->bind,      &bytes[at], sizeof(buffer->bind)); at += sizeof(buffer->bind);
		memcpy(&buffer->size,      &bytes[at], sizeof(buffer->size)); at += sizeof(buffer->size);
		memcpy(&buffer->var_count, &bytes[at], sizeof(buffer->size)); at += sizeof(buffer->var_count);
		buffer->defaults = malloc(buffer->size);
		//memcpy(&buffer->defaults,     &bytes[at], buffer->size);         at += buffer->size;
		buffer->vars = (skr_shader_meta_var_t*)malloc(sizeof(skr_shader_meta_var_t) * buffer->var_count);

		for (uint32_t t = 0; t < buffer->var_count; t++) {
			skr_shader_meta_var_t *var = &buffer->vars[t];
			memcpy( var->name,   &bytes[at], sizeof(var->name ));  at += sizeof(var->name  );
			memcpy( var->extra,  &bytes[at], sizeof(var->extra));  at += sizeof(var->extra );
			memcpy(&var->offset, &bytes[at], sizeof(var->offset)); at += sizeof(var->offset);
			memcpy(&var->size,   &bytes[at], sizeof(var->size));   at += sizeof(var->size  );
		}
	}

	for (uint32_t i = 0; i < out_file->meta->texture_count; i++) {
		skr_shader_meta_texture_t *tex = &out_file->meta->textures[i];
		memcpy( tex->name,  &bytes[at], sizeof(tex->name )); at += sizeof(tex->name );
		memcpy( tex->extra, &bytes[at], sizeof(tex->extra)); at += sizeof(tex->extra);
		memcpy(&tex->bind,  &bytes[at], sizeof(tex->bind )); at += sizeof(tex->bind );
	}

	for (uint32_t i = 0; i < out_file->stage_count; i++) {
		skr_shader_file_stage_t *stage = &out_file->stages[i];
		memcpy( &stage->language, &bytes[at], sizeof(stage->language)); at += sizeof(stage->language);
		memcpy( &stage->stage,    &bytes[at], sizeof(stage->stage));    at += sizeof(stage->stage);
		memcpy( &stage->code_size,&bytes[at], sizeof(stage->code_size));at += sizeof(stage->code_size);

		stage->code = malloc(stage->code_size);
		memcpy(stage->code, &bytes[at], stage->code_size); at += stage->code_size;
	}

	return true;
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
			free(meta->buffers[i].vars);
			free(meta->buffers[i].defaults);
		}
		free(meta->buffers);
		free(meta->textures);
		memset(meta, 0, sizeof(skr_shader_meta_t));
	}
}

///////////////////////////////////////////
// skr_shader_t                          //
///////////////////////////////////////////

skr_shader_t skr_shader_create_file(const char *sks_filename) {
	skr_shader_file_t file;
	if (!skr_shader_file_load(sks_filename, &file))
		return {};

	skr_shader_stage_t vs     = skr_shader_file_create_stage(&file, skr_shader_vertex);
	skr_shader_stage_t ps     = skr_shader_file_create_stage(&file, skr_shader_pixel);
	skr_shader_t       result = skr_shader_create_manual(file.meta, vs, ps );

	skr_shader_stage_destroy(&vs);
	skr_shader_stage_destroy(&ps);
	skr_shader_file_destroy (&file);

	return result;
}

///////////////////////////////////////////

skr_shader_t skr_shader_create_mem(void *sks_data, size_t sks_data_size) {
	skr_shader_file_t file;
	if (!skr_shader_file_load_mem(sks_data, sks_data_size, &file))
		return {};

	skr_shader_stage_t vs     = skr_shader_file_create_stage(&file, skr_shader_vertex);
	skr_shader_stage_t ps     = skr_shader_file_create_stage(&file, skr_shader_pixel);
	skr_shader_t       result = skr_shader_create_manual( file.meta, vs, ps );

	skr_shader_stage_destroy(&vs);
	skr_shader_stage_destroy(&ps);
	skr_shader_file_destroy (&file);

	return result;
}

///////////////////////////////////////////

skr_shader_bind_t skr_shader_get_tex_bind(const skr_shader_t *shader, const char *name) {
	for (uint32_t i = 0; i < shader->meta->texture_count; i++) {
		if (strcmp(name, shader->meta->textures[i].name) == 0)
			return shader->meta->textures[i].bind;
	}
	return {};
}

///////////////////////////////////////////

skr_shader_bind_t skr_shader_get_buffer_bind(const skr_shader_t *shader, const char *name) {
	for (uint32_t i = 0; i < shader->meta->buffer_count; i++) {
		if (strcmp(name, shader->meta->buffers[i].name) == 0)
			return shader->meta->buffers[i].bind;
	}
	return {};
}