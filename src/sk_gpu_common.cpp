#include "sk_gpu_dev.h"
///////////////////////////////////////////
// Common Code                           //
///////////////////////////////////////////

#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#if __ANDROID__
#include <android/asset_manager.h>
#endif

void (*_skg_log)(skg_log_ level, const char *text);
void skg_callback_log(void (*callback)(skg_log_ level, const char *text)) {
	_skg_log = callback;
}
void skg_log(skg_log_ level, const char *text) {
	if (_skg_log) _skg_log(level, text);
}

///////////////////////////////////////////

bool (*_skg_read_file)(const char *filename, void **out_data, size_t *out_size);
void skg_callback_file_read(bool (*callback)(const char *filename, void **out_data, size_t *out_size)) {
	_skg_read_file = callback;
}
bool skg_read_file(const char *filename, void **out_data, size_t *out_size) {
	if (_skg_read_file) return _skg_read_file(filename, out_data, out_size);
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

uint64_t skg_hash(const char *string) {
	uint64_t hash = 14695981039346656037UL;
	uint8_t  c;
	while ((c = *string++))
		hash = (hash ^ c) * 1099511628211;
	return hash;
}

///////////////////////////////////////////

skg_color32_t skg_hsv32(float h, float s, float v, float a) {
	skg_color128_t col = skg_hsv128(h,s,v,a);
	return skg_color32_t{
		(uint8_t)(col.r*255),
		(uint8_t)(col.g*255),
		(uint8_t)(col.b*255),
		(uint8_t)(col.a*255)};
}

///////////////////////////////////////////

skg_color128_t skg_hsv128(float h, float s, float v, float a) {
	const float K[4] = { 1.0f, 2.0f/3.0f, 1.0f/3.0f, 3.0f };
	float p[3] = {
		fabsf(((h + K[0]) - floorf(h + K[0])) * 6.0f - K[3]),
		fabsf(((h + K[1]) - floorf(h + K[1])) * 6.0f - K[3]),
		fabsf(((h + K[2]) - floorf(h + K[2])) * 6.0f - K[3]) };

	// lerp: a + (b - a) * t
	return skg_color128_t {
		(K[0] + (fmaxf(0,fminf(p[0] - K[0], 1.0f)) - K[0]) * s) * v,
		(K[0] + (fmaxf(0,fminf(p[1] - K[0], 1.0f)) - K[0]) * s) * v,
		(K[0] + (fmaxf(0,fminf(p[2] - K[0], 1.0f)) - K[0]) * s) * v,
		a };
}

///////////////////////////////////////////

bool skg_shader_file_load(const char *file, skg_shader_file_t *out_file) {
	void  *data = nullptr;
	size_t size = 0;

	if (!skg_read_file(file, &data, &size))
		return false;

	bool result = skg_shader_file_load_memory(data, size, out_file);
	free(data);

	return result;
}

///////////////////////////////////////////

bool skg_shader_file_verify(const void *data, size_t size, uint16_t *out_version, char *out_name, size_t out_name_size) {
	const char    *prefix  = "SKSHADER";
	const uint8_t *bytes   = (uint8_t*)data;

	// check the first 5 bytes to see if this is a SKS shader file
	if (size < 10 || memcmp(bytes, prefix, 8) != 0)
		return false;

	// Grab the file version
	if (out_version)
		memcpy(out_version, &bytes[8], sizeof(uint16_t));

	// And grab the name of the shader
	if (out_name != nullptr && out_name_size > 0) {
		memcpy(out_name, &bytes[14], out_name_size < 256 ? out_name_size : 256);
		out_name[out_name_size - 1] = '\0';
	}

	return true;
}

///////////////////////////////////////////

bool skg_shader_file_load_memory(const void *data, size_t size, skg_shader_file_t *out_file) {
	uint16_t file_version = 0;
	if (!skg_shader_file_verify(data, size, &file_version, nullptr, 0) || file_version != 1) {
		return false;
	}
	
	const uint8_t *bytes = (uint8_t*)data;
	size_t at = 10;
	memcpy(&out_file->stage_count, &bytes[at], sizeof(out_file->stage_count)); at += sizeof(out_file->stage_count);
	out_file->stages = (skg_shader_file_stage_t*)malloc(sizeof(skg_shader_file_stage_t) * out_file->stage_count);
	if (out_file->stages == nullptr) { skg_log(skg_log_critical, "Out of memory"); return false; }

	out_file->meta = (skg_shader_meta_t*)malloc(sizeof(skg_shader_meta_t));
	if (out_file->meta == nullptr) { skg_log(skg_log_critical, "Out of memory"); return false; }
	*out_file->meta = {};
	out_file->meta->global_buffer_id = -1;
	skg_shader_meta_reference(out_file->meta);
	memcpy( out_file->meta->name,          &bytes[at], sizeof(out_file->meta->name         )); at += sizeof(out_file->meta->name);
	memcpy(&out_file->meta->buffer_count,  &bytes[at], sizeof(out_file->meta->buffer_count )); at += sizeof(out_file->meta->buffer_count);
	memcpy(&out_file->meta->texture_count, &bytes[at], sizeof(out_file->meta->texture_count)); at += sizeof(out_file->meta->texture_count);
	out_file->meta->buffers  = (skg_shader_buffer_t *)malloc(sizeof(skg_shader_buffer_t ) * out_file->meta->buffer_count );
	out_file->meta->textures = (skg_shader_texture_t*)malloc(sizeof(skg_shader_texture_t) * out_file->meta->texture_count);
	if (out_file->meta->buffers == nullptr || out_file->meta->textures == nullptr) { skg_log(skg_log_critical, "Out of memory"); return false; }
	memset(out_file->meta->buffers,  0, sizeof(skg_shader_buffer_t ) * out_file->meta->buffer_count);
	memset(out_file->meta->textures, 0, sizeof(skg_shader_texture_t) * out_file->meta->texture_count);

	for (uint32_t i = 0; i < out_file->meta->buffer_count; i++) {
		skg_shader_buffer_t *buffer = &out_file->meta->buffers[i];
		memcpy( buffer->name,      &bytes[at], sizeof(buffer->name));      at += sizeof(buffer->name);
		memcpy(&buffer->bind,      &bytes[at], sizeof(buffer->bind));      at += sizeof(buffer->bind);
		memcpy(&buffer->size,      &bytes[at], sizeof(buffer->size));      at += sizeof(buffer->size);
		memcpy(&buffer->var_count, &bytes[at], sizeof(buffer->var_count)); at += sizeof(buffer->var_count);

		uint32_t default_size = 0;
		memcpy(&default_size, &bytes[at], sizeof(buffer->size)); at += sizeof(buffer->size);
		buffer->defaults = nullptr;
		if (default_size != 0) {
			buffer->defaults = malloc(buffer->size);
			memcpy(buffer->defaults, &bytes[at], default_size); at += default_size;
		}
		buffer->vars = (skg_shader_var_t*)malloc(sizeof(skg_shader_var_t) * buffer->var_count);
		if (buffer->vars == nullptr) { skg_log(skg_log_critical, "Out of memory"); return false; }
		memset(buffer->vars, 0, sizeof(skg_shader_var_t) * buffer->var_count);
		buffer->name_hash = skg_hash(buffer->name);

		for (uint32_t t = 0; t < buffer->var_count; t++) {
			skg_shader_var_t *var = &buffer->vars[t];
			memcpy( var->name,       &bytes[at], sizeof(var->name ));      at += sizeof(var->name  );
			memcpy( var->extra,      &bytes[at], sizeof(var->extra));      at += sizeof(var->extra );
			memcpy(&var->offset,     &bytes[at], sizeof(var->offset));     at += sizeof(var->offset);
			memcpy(&var->size,       &bytes[at], sizeof(var->size));       at += sizeof(var->size  );
			memcpy(&var->type,       &bytes[at], sizeof(var->type));       at += sizeof(var->type  );
			memcpy(&var->type_count, &bytes[at], sizeof(var->type_count)); at += sizeof(var->type_count);
			var->name_hash = skg_hash(var->name);
		}

		if (strcmp(buffer->name, "$Globals") == 0)
			out_file->meta->global_buffer_id = i;
	}

	for (uint32_t i = 0; i < out_file->meta->texture_count; i++) {
		skg_shader_texture_t *tex = &out_file->meta->textures[i];
		memcpy( tex->name,  &bytes[at], sizeof(tex->name )); at += sizeof(tex->name );
		memcpy( tex->extra, &bytes[at], sizeof(tex->extra)); at += sizeof(tex->extra);
		memcpy(&tex->bind,  &bytes[at], sizeof(tex->bind )); at += sizeof(tex->bind );
		tex->name_hash = skg_hash(tex->name);
	}

	for (uint32_t i = 0; i < out_file->stage_count; i++) {
		skg_shader_file_stage_t *stage = &out_file->stages[i];
		memcpy( &stage->language, &bytes[at], sizeof(stage->language)); at += sizeof(stage->language);
		memcpy( &stage->stage,    &bytes[at], sizeof(stage->stage));    at += sizeof(stage->stage);
		memcpy( &stage->code_size,&bytes[at], sizeof(stage->code_size));at += sizeof(stage->code_size);

		stage->code = 0;
		if (stage->code_size > 0) {
			stage->code = malloc(stage->code_size);
			if (stage->code == nullptr) { skg_log(skg_log_critical, "Out of memory"); return false; }
			memcpy(stage->code, &bytes[at], stage->code_size); at += stage->code_size;
		}
	}

	return true;
}

///////////////////////////////////////////

skg_shader_stage_t skg_shader_file_create_stage(const skg_shader_file_t *file, skg_stage_ stage) {
	skg_shader_lang_ language;
#if defined(SKG_DIRECT3D11) || defined(SKG_DIRECT3D12)
	language = skg_shader_lang_hlsl;
#elif defined(SKG_OPENGL)
	#ifdef __EMSCRIPTEN__
		language = skg_shader_lang_glsl_web;
	#else
		language = skg_shader_lang_glsl;
	#endif
#elif defined(SKG_VULKAN)
	language = skg_shader_lang_spirv;
#endif

	for (uint32_t i = 0; i < file->stage_count; i++) {
		if (file->stages[i].language == language && file->stages[i].stage == stage)
			return skg_shader_stage_create(file->stages[i].code, file->stages[i].code_size, stage);
	}
	return {};
}

///////////////////////////////////////////

void skg_shader_file_destroy(skg_shader_file_t *file) {
	for (uint32_t i = 0; i < file->stage_count; i++) {
		free(file->stages[i].code);
	}
	free(file->stages);
	skg_shader_meta_release(file->meta);
	*file = {};
}

///////////////////////////////////////////

void skg_shader_meta_reference(skg_shader_meta_t *meta) {
	meta->references += 1;
}

///////////////////////////////////////////

void skg_shader_meta_release(skg_shader_meta_t *meta) {
	if (!meta) return;
	meta->references -= 1;
	if (meta->references == 0) {
		for (uint32_t i = 0; i < meta->buffer_count; i++) {
			free(meta->buffers[i].vars);
			free(meta->buffers[i].defaults);
		}
		free(meta->buffers);
		free(meta->textures);
		*meta = {};
	}
}

///////////////////////////////////////////
// skg_shader_t                          //
///////////////////////////////////////////

skg_shader_t skg_shader_create_file(const char *sks_filename) {
	skg_shader_file_t file;
	if (!skg_shader_file_load(sks_filename, &file))
		return {};

	skg_shader_stage_t vs     = skg_shader_file_create_stage(&file, skg_stage_vertex);
	skg_shader_stage_t ps     = skg_shader_file_create_stage(&file, skg_stage_pixel);
	skg_shader_stage_t cs     = skg_shader_file_create_stage(&file, skg_stage_compute);
	skg_shader_t       result = skg_shader_create_manual( file.meta, vs, ps, cs );

	skg_shader_stage_destroy(&vs);
	skg_shader_stage_destroy(&ps);
	skg_shader_stage_destroy(&cs);
	skg_shader_file_destroy (&file);

	return result;
}

///////////////////////////////////////////

skg_shader_t skg_shader_create_memory(const void *sks_data, size_t sks_data_size) {
	skg_shader_file_t file;
	if (!skg_shader_file_load_memory(sks_data, sks_data_size, &file))
		return {};

	skg_shader_stage_t vs     = skg_shader_file_create_stage(&file, skg_stage_vertex);
	skg_shader_stage_t ps     = skg_shader_file_create_stage(&file, skg_stage_pixel);
	skg_shader_stage_t cs     = skg_shader_file_create_stage(&file, skg_stage_compute);
	skg_shader_t       result = skg_shader_create_manual( file.meta, vs, ps, cs );

	skg_shader_stage_destroy(&vs);
	skg_shader_stage_destroy(&ps);
	skg_shader_stage_destroy(&cs);
	skg_shader_file_destroy (&file);

	return result;
}

///////////////////////////////////////////

skg_bind_t skg_shader_get_tex_bind(const skg_shader_t *shader, const char *name) {
	for (uint32_t i = 0; i < shader->meta->texture_count; i++) {
		if (strcmp(name, shader->meta->textures[i].name) == 0)
			return shader->meta->textures[i].bind;
	}
	return {};
}

///////////////////////////////////////////

skg_bind_t skg_shader_get_buffer_bind(const skg_shader_t *shader, const char *name) {
	for (uint32_t i = 0; i < shader->meta->buffer_count; i++) {
		if (strcmp(name, shader->meta->buffers[i].name) == 0)
			return shader->meta->buffers[i].bind;
	}
	return {};
}

///////////////////////////////////////////

int32_t skg_shader_get_var_count(const skg_shader_t *shader) {
	return shader->meta->global_buffer_id != -1
		? shader->meta->buffers[shader->meta->global_buffer_id].var_count
		: 0;
}

///////////////////////////////////////////

int32_t skg_shader_get_var_index(const skg_shader_t *shader, const char *name) {
	return skg_shader_get_var_index_h(shader, skg_hash(name));
}

///////////////////////////////////////////

int32_t skg_shader_get_var_index_h(const skg_shader_t *shader, uint64_t name_hash) {
	if (shader->meta->global_buffer_id == -1) return -1;

	skg_shader_buffer_t *buffer = &shader->meta->buffers[shader->meta->global_buffer_id];
	for (uint32_t i = 0; i < buffer->var_count; i++) {
		if (buffer->vars[i].name_hash == name_hash) {
			return i;
		}
	}
	return -1;
}

///////////////////////////////////////////

const skg_shader_var_t *skg_shader_get_var_info(const skg_shader_t *shader, int32_t var_id) {
	if (shader->meta->global_buffer_id == -1 || var_id == -1) return nullptr;

	skg_shader_buffer_t *buffer = &shader->meta->buffers[shader->meta->global_buffer_id];
	return &buffer->vars[var_id];
}

///////////////////////////////////////////

uint32_t skg_tex_fmt_size(skg_tex_fmt_ format) {
	switch (format) {
	case skg_tex_fmt_rgba32:        return sizeof(uint8_t )*4;
	case skg_tex_fmt_rgba32_linear: return sizeof(uint8_t )*4;
	case skg_tex_fmt_bgra32:        return sizeof(uint8_t )*4;
	case skg_tex_fmt_bgra32_linear: return sizeof(uint8_t )*4;
	case skg_tex_fmt_rgba64:        return sizeof(uint16_t)*4;
	case skg_tex_fmt_rgba128:       return sizeof(uint32_t)*4;
	case skg_tex_fmt_depth16:       return sizeof(uint16_t);
	case skg_tex_fmt_depth32:       return sizeof(uint32_t);
	case skg_tex_fmt_depthstencil:  return sizeof(uint32_t);
	case skg_tex_fmt_r8:            return sizeof(uint8_t );
	case skg_tex_fmt_r16:           return sizeof(uint16_t);
	case skg_tex_fmt_r32:           return sizeof(uint32_t);
	default: return 0;
	}
}