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
	FILE *fp;
#if _WIN32
	if (fopen_s(&fp, filename, "rb") != 0 || fp == nullptr) {
		return false;
	}
#else
	fp = fopen(filename, "rb");
	if (fp == nullptr) {
		return false;
	}
#endif

	fseek(fp, 0L, SEEK_END);
	*out_size = ftell(fp);
	rewind(fp);

	*out_data = malloc(*out_size);
	if (*out_data == nullptr) { *out_size = 0; fclose(fp); return false; }
	fread (*out_data, *out_size, 1, fp);
	fclose(fp);

	return true;
}

///////////////////////////////////////////

uint64_t skg_hash(const char *string) {
	uint64_t hash = 14695981039346656037UL;
	while (*string != '\0') {
		hash = (hash ^ *string) * 1099511628211;
		string++;
	}
	return hash;
}

///////////////////////////////////////////

uint32_t skg_mip_count(int32_t width, int32_t height) {
	return (uint32_t)log2f(fminf((float)width, (float)height)) + 1;
}

///////////////////////////////////////////

skg_color32_t skg_col_hsv32(float h, float s, float v, float a) {
	skg_color128_t col = skg_col_hsv128(h,s,v,a);
	return skg_color32_t{
		(uint8_t)(col.r*255),
		(uint8_t)(col.g*255),
		(uint8_t)(col.b*255),
		(uint8_t)(col.a*255)};
}

///////////////////////////////////////////

// Reference from here: http://lolengine.net/blog/2013/07/27/rgb-to-hsv-in-glsl
skg_color128_t skg_col_hsv128(float h, float s, float v, float a) {
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

skg_color32_t skg_col_hsl32(float h, float c, float l, float a) {
	skg_color128_t col = skg_col_hsl128(h,c,l,a);
	return skg_color32_t{
		(uint8_t)(col.r*255),
		(uint8_t)(col.g*255),
		(uint8_t)(col.b*255),
		(uint8_t)(col.a*255)};
}

///////////////////////////////////////////

skg_color128_t skg_col_hsl128(float h, float s, float l, float a) {
	if (h < 0) h -= floorf(h);
	float r = fabsf(h * 6 - 3) - 1;
	float g = 2 - fabsf(h * 6 - 2);
	float b = 2 - fabsf(h * 6 - 4);
	r = fmaxf(0, fminf(1, r));
	g = fmaxf(0, fminf(1, g));
	b = fmaxf(0, fminf(1, b));

	float C = (1 - fabsf(2 * l - 1)) * s;
	return {
		(r - 0.5f) * C + l,
		(g - 0.5f) * C + l,
		(b - 0.5f) * C + l, a };
}

///////////////////////////////////////////

skg_color32_t skg_col_hcy32(float h, float c, float l, float a) {
	skg_color128_t col = skg_col_hcy128(h,c,l,a);
	return skg_color32_t{
		(uint8_t)(col.r*255),
		(uint8_t)(col.g*255),
		(uint8_t)(col.b*255),
		(uint8_t)(col.a*255)};
}

///////////////////////////////////////////

// Reference from here https://www.chilliant.com/rgb2hsv.html
skg_color128_t skg_col_hcy128(float h, float c, float y, float a) {
	if (h < 0) h -= floorf(h);
	float r = fabsf(h * 6 - 3) - 1;
	float g = 2 - fabsf(h * 6 - 2);
	float b = 2 - fabsf(h * 6 - 4);
	r = fmaxf(0, fminf(1, r));
	g = fmaxf(0, fminf(1, g));
	b = fmaxf(0, fminf(1, b));

	float Z = r*0.299f + g*0.587f + b*0.114f;
	if (y < Z) {
		c *= y / Z;
	} else if (Z < 1) {
		c *= (1 - y) / (1 - Z);
	}
	return skg_color128_t { 
		(r - Z) * c + y,
		(g - Z) * c + y,
		(b - Z) * c + y,
		a };
}

///////////////////////////////////////////

skg_color32_t skg_col_lch32(float h, float c, float l, float a) {
	skg_color128_t col = skg_col_lch128(h,c,l,a);
	return skg_color32_t{
		(uint8_t)(col.r*255),
		(uint8_t)(col.g*255),
		(uint8_t)(col.b*255),
		(uint8_t)(col.a*255)};
}

///////////////////////////////////////////

// Reference from here: https://www.easyrgb.com/en/math.php
skg_color128_t skg_col_lch128(float h, float c, float l, float alpha) {
	const float tau = 6.283185307179586476925286766559f;
	c = c * 200;
	l = l * 100;
	float a = cosf( h*tau ) * c;
	float b = sinf( h*tau ) * c;
	
	float
		y = (l + 16.f) / 116.f,
		x = (a / 500.f) + y,
		z = y - (b / 200.f);

	x = 0.95047f * ((x*x*x > 0.008856f) ? x*x*x : (x - 16/116.f) / 7.787f);
	y = 1.00000f * ((y*y*y > 0.008856f) ? y*y*y : (y - 16/116.f) / 7.787f);
	z = 1.08883f * ((z*z*z > 0.008856f) ? z*z*z : (z - 16/116.f) / 7.787f);

	float r = x *  3.2406f + y * -1.5372f + z * -0.4986f;
	float g = x * -0.9689f + y *  1.8758f + z *  0.0415f;
	      b = x *  0.0557f + y * -0.2040f + z *  1.0570f;

	r = (r > 0.0031308f) ? (1.055f * powf(r, 1/2.4f) - 0.055f) : 12.92f * r;
	g = (g > 0.0031308f) ? (1.055f * powf(g, 1/2.4f) - 0.055f) : 12.92f * g;
	b = (b > 0.0031308f) ? (1.055f * powf(b, 1/2.4f) - 0.055f) : 12.92f * b;

	return skg_color128_t { 
		fmaxf(0, fminf(1, r)),
		fmaxf(0, fminf(1, g)),
		fmaxf(0, fminf(1, b)), alpha };
}

///////////////////////////////////////////

skg_color32_t skg_col_helix32(float h, float c, float l, float a) {
	skg_color128_t col = skg_col_helix128(h,c,l,a);
	return skg_color32_t{
		(uint8_t)(col.r*255),
		(uint8_t)(col.g*255),
		(uint8_t)(col.b*255),
		(uint8_t)(col.a*255)};
}

///////////////////////////////////////////

// Reference here: http://www.mrao.cam.ac.uk/~dag/CUBEHELIX/
skg_color128_t skg_col_helix128(float h, float s, float l, float alpha) {
	const float tau = 6.28318f;
	l = fminf(1,l);
	float angle = tau * (h+(1/3.f));
	float amp   = s * l * (1.f - l); // Helix in some implementations will 
	// divide this by 2.0f and go at half s, but if we clamp rgb at the end, 
	// we can get full s at the cost of a bit of artifacting at high 
	// s+lightness values.

	float a_cos = cosf(angle);
	float a_sin = sinf(angle);
	float r = l + amp * (-0.14861f * a_cos + 1.78277f * a_sin);
	float g = l + amp * (-0.29227f * a_cos - 0.90649f * a_sin);
	float b = l + amp * ( 1.97294f * a_cos);
	r = fmaxf(0,fminf(1, r));
	g = fmaxf(0,fminf(1, g));
	b = fmaxf(0,fminf(1, b));
	return { r, g, b, alpha };
}

///////////////////////////////////////////

skg_color32_t skg_col_jab32(float j, float a, float b, float alpha) {
	skg_color128_t col = skg_col_jab128(j, a, b, alpha);
	return skg_color32_t{ (uint8_t)(col.r*255), (uint8_t)(col.g*255), (uint8_t)(col.b*255), (uint8_t)(col.a*255)};
}

///////////////////////////////////////////

float lms(float t) {
	if (t > 0.) {
		float r = powf(t, 0.007460772656268214f);
		float s = (0.8359375f - r) / (18.6875f*r + -18.8515625f);
		return powf(s, 6.277394636015326f);
	} else {
		return 0.f;
	}
}

float srgb(float c) {
	if (c <= 0.0031308049535603713f) {
		return c * 12.92f;
	} else {
		float c_ = powf(c, 0.41666666666666666f);
		return c_ * 1.055f + -0.055f;
	}
}

// ref : https://thebookofshaders.com/edit.php?log=180722032925
skg_color128_t jchz2srgb(float h, float s, float l, float alpha) {
	float jz = l*0.16717463120366200f + 1.6295499532821566e-11f;
	float cz = s*0.16717463120366200f;
	float hz = h*6.28318530717958647f;

	float iz = jz / (0.56f*jz + 0.44f);
	float az = cz * cosf(hz);
	float bz = cz * sinf(hz);

	float l_ = iz + az* +0.13860504327153930f + bz* +0.058047316156118830f;
	float m_ = iz + az* -0.13860504327153927f + bz* -0.058047316156118904f;
	float s_ = iz + az* -0.09601924202631895f + bz* -0.811891896056039000f;

	      l = lms(l_);
	float m = lms(m_);
	      s = lms(s_);

	float lr = l* +0.0592896375540425100e4f + m* -0.052239474257975140e4f + s* +0.003259644233339027e4f;
	float lg = l* -0.0222329579044572220e4f + m* +0.038215274736946150e4f + s* -0.005703433147128812e4f;
	float lb = l* +0.0006270913830078808e4f + m* -0.007021906556220012e4f + s* +0.016669756032437408e4f;

	lr = fmaxf(0,fminf(1, srgb(lr)));
	lg = fmaxf(0,fminf(1, srgb(lg)));
	lb = fmaxf(0,fminf(1, srgb(lb)));
	return skg_color128_t{lr, lg, lb, alpha };
}

// Reference here: https://observablehq.com/@jrus/jzazbz
float pqi(float x) {
	x = powf(x, .007460772656268214f);
	return x <= 0 ? 0 : powf(
		(0.8359375f - x) / (18.6875f*x - 18.8515625f),
		6.277394636015326f); 
};
skg_color128_t skg_col_jab128(float j, float a, float b, float alpha) {
	// JAB to XYZ
	j = j + 1.6295499532821566e-11f;
	float iz = j / (0.44f + 0.56f * j);
	float l  = pqi(iz + .1386050432715393f*a + .0580473161561187f*b);
	float m  = pqi(iz - .1386050432715393f*a - .0580473161561189f*b);
	float s  = pqi(iz - .0960192420263189f*a - .8118918960560390f*b);

	float r = l* +0.0592896375540425100e4f + m* -0.052239474257975140e4f + s* +0.003259644233339027e4f;
	float g = l* -0.0222329579044572220e4f + m* +0.038215274736946150e4f + s* -0.005703433147128812e4f;
	      b = l* +0.0006270913830078808e4f + m* -0.007021906556220012e4f + s* +0.016669756032437408e4f;

	/*float x = +1.661373055774069e+00f * L - 9.145230923250668e-01f * M + 2.313620767186147e-01f * S;
	float y = -3.250758740427037e-01f * L + 1.571847038366936e+00f * M - 2.182538318672940e-01f * S;
	float z = -9.098281098284756e-02f * L - 3.127282905230740e-01f * M + 1.522766561305260e+00f * S;

	// XYZ to sRGB
	float r = x *  3.2406f + y * -1.5372f + z * -0.4986f;
	float g = x * -0.9689f + y *  1.8758f + z *  0.0415f;
	      b = x *  0.0557f + y * -0.2040f + z *  1.0570f;*/

	// to sRGB
	r = (r > 0.0031308f) ? (1.055f * powf(r, 1/2.4f) - 0.055f) : 12.92f * r;
	g = (g > 0.0031308f) ? (1.055f * powf(g, 1/2.4f) - 0.055f) : 12.92f * g;
	b = (b > 0.0031308f) ? (1.055f * powf(b, 1/2.4f) - 0.055f) : 12.92f * b;

	return skg_color128_t { 
		fmaxf(0, fminf(1, r)),
		fmaxf(0, fminf(1, g)),
		fmaxf(0, fminf(1, b)), alpha };
}

skg_color32_t skg_col_jsl32(float h, float s, float l, float alpha) {
	skg_color128_t col = skg_col_jsl128(h, s, l, alpha);
	return skg_color32_t{ (uint8_t)(col.r*255), (uint8_t)(col.g*255), (uint8_t)(col.b*255), (uint8_t)(col.a*255)};
}
skg_color128_t skg_col_jsl128(float h, float s, float l, float alpha) {
	return jchz2srgb(h, s, l, alpha);/*
	const float tau = 6.28318f;
	h = h * tau - tau/2;
	s = s * 0.16717463120366200f;
	l = l * 0.16717463120366200f;
	return skg_col_jab128(fmaxf(0,l), s * cosf(h), s * sinf(h), alpha);*/
}

///////////////////////////////////////////

skg_color32_t skg_col_lab32(float l, float a, float b, float alpha) {
	skg_color128_t col = skg_col_lab128(l,a,b,alpha);
	return skg_color32_t{
		(uint8_t)(col.r*255),
		(uint8_t)(col.g*255),
		(uint8_t)(col.b*255),
		(uint8_t)(col.a*255)};
}

///////////////////////////////////////////

skg_color128_t skg_col_lab128(float l, float a, float b, float alpha) {
	l = l * 100;
	a = a * 400 - 200;
	b = b * 400 - 200;
	float
		y = (l + 16.f) / 116.f,
		x = (a / 500.f) + y,
		z = y - (b / 200.f);

	x = 0.95047f * ((x * x * x > 0.008856f) ? x * x * x : (x - 16/116.f) / 7.787f);
	y = 1.00000f * ((y * y * y > 0.008856f) ? y * y * y : (y - 16/116.f) / 7.787f);
	z = 1.08883f * ((z * z * z > 0.008856f) ? z * z * z : (z - 16/116.f) / 7.787f);

	float r = x *  3.2406f + y * -1.5372f + z * -0.4986f;
	float g = x * -0.9689f + y *  1.8758f + z *  0.0415f;
	      b = x *  0.0557f + y * -0.2040f + z *  1.0570f;

	r = (r > 0.0031308f) ? (1.055f * powf(r, 1/2.4f) - 0.055f) : 12.92f * r;
	g = (g > 0.0031308f) ? (1.055f * powf(g, 1/2.4f) - 0.055f) : 12.92f * g;
	b = (b > 0.0031308f) ? (1.055f * powf(b, 1/2.4f) - 0.055f) : 12.92f * b;

	return skg_color128_t { 
		fmaxf(0, fminf(1, r)),
		fmaxf(0, fminf(1, g)),
		fmaxf(0, fminf(1, b)), alpha };
}

///////////////////////////////////////////

skg_color128_t skg_col_rgb_to_lab128(skg_color128_t rgb) {
	rgb.r = (rgb.r > 0.04045f) ? powf((rgb.r + 0.055f) / 1.055f, 2.4f) : rgb.r / 12.92f;
	rgb.g = (rgb.g > 0.04045f) ? powf((rgb.g + 0.055f) / 1.055f, 2.4f) : rgb.g / 12.92f;
	rgb.b = (rgb.b > 0.04045f) ? powf((rgb.b + 0.055f) / 1.055f, 2.4f) : rgb.b / 12.92f;

	// D65, Daylight, sRGB, aRGB
	float x = (rgb.r * 0.4124f + rgb.g * 0.3576f + rgb.b * 0.1805f) / 0.95047f;
	float y = (rgb.r * 0.2126f + rgb.g * 0.7152f + rgb.b * 0.0722f) / 1.00000f;
	float z = (rgb.r * 0.0193f + rgb.g * 0.1192f + rgb.b * 0.9505f) / 1.08883f;

	x = (x > 0.008856f) ? powf(x, 1/3.f) : (7.787f * x) + 16/116.f;
	y = (y > 0.008856f) ? powf(y, 1/3.f) : (7.787f * y) + 16/116.f;
	z = (z > 0.008856f) ? powf(z, 1/3.f) : (7.787f * z) + 16/116.f;

	return {
		(1.16f * y) - .16f,
		1.25f * (x - y) + 0.5f,
		0.5f * (y - z) + 0.5f, rgb.a };
}

///////////////////////////////////////////

inline float _skg_to_srgb(float x) {
	return x < 0.0031308f
		? x * 12.92f
		: 1.055f * powf(x, 1 / 2.4f) - 0.055f;
}
skg_color128_t skg_col_to_srgb(skg_color128_t rgb_linear) {
	return {
		_skg_to_srgb(rgb_linear.r),
		_skg_to_srgb(rgb_linear.g),
		_skg_to_srgb(rgb_linear.b),
		rgb_linear.a };
}

///////////////////////////////////////////

inline float _skg_to_linear(float x) {
	return x < 0.04045f
		? x / 12.92f
		: powf((x + 0.055f) / 1.055f, 2.4f);
}
skg_color128_t skg_col_to_linear(skg_color128_t srgb) {
	return {
		_skg_to_linear(srgb.r),
		_skg_to_linear(srgb.g),
		_skg_to_linear(srgb.b),
		srgb.a };
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
	if (!skg_shader_file_verify(data, size, &file_version, nullptr, 0) || file_version != 2) {
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
	memcpy( out_file->meta->name,            &bytes[at], sizeof(out_file->meta->name          )); at += sizeof(out_file->meta->name);
	memcpy(&out_file->meta->buffer_count,    &bytes[at], sizeof(out_file->meta->buffer_count  )); at += sizeof(out_file->meta->buffer_count);
	memcpy(&out_file->meta->resource_count,  &bytes[at], sizeof(out_file->meta->resource_count)); at += sizeof(out_file->meta->resource_count);
	out_file->meta->buffers   = (skg_shader_buffer_t  *)malloc(sizeof(skg_shader_buffer_t  ) * out_file->meta->buffer_count  );
	out_file->meta->resources = (skg_shader_resource_t*)malloc(sizeof(skg_shader_resource_t) * out_file->meta->resource_count);
	if (out_file->meta->buffers == nullptr || out_file->meta->resources == nullptr) { skg_log(skg_log_critical, "Out of memory"); return false; }
	memset(out_file->meta->buffers,   0, sizeof(skg_shader_buffer_t  ) * out_file->meta->buffer_count);
	memset(out_file->meta->resources, 0, sizeof(skg_shader_resource_t) * out_file->meta->resource_count);

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

		if (strcmp(buffer->name, "$Global") == 0)
			out_file->meta->global_buffer_id = i;
	}

	for (uint32_t i = 0; i < out_file->meta->resource_count; i++) {
		skg_shader_resource_t *res = &out_file->meta->resources[i];
		memcpy( res->name,  &bytes[at], sizeof(res->name )); at += sizeof(res->name );
		memcpy( res->extra, &bytes[at], sizeof(res->extra)); at += sizeof(res->extra);
		memcpy(&res->bind,  &bytes[at], sizeof(res->bind )); at += sizeof(res->bind );
		res->name_hash = skg_hash(res->name);
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
	#if   defined(_SKG_GL_WEB)
		language = skg_shader_lang_glsl_web;
	#elif defined(_SKG_GL_ES)
		language = skg_shader_lang_glsl_es;
	#elif defined(_SKG_GL_DESKTOP)
		language = skg_shader_lang_glsl;
	#endif
#elif defined(SKG_VULKAN)
	language = skg_shader_lang_spirv;
#endif

	for (uint32_t i = 0; i < file->stage_count; i++) {
		if (file->stages[i].language == language && file->stages[i].stage == stage)
			return skg_shader_stage_create(file->stages[i].code, file->stages[i].code_size, stage);
	}
	skg_shader_stage_t empty = {};
	return empty;
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
// skg_shader_meta_t                     //
///////////////////////////////////////////

skg_bind_t skg_shader_meta_get_bind(const skg_shader_meta_t *meta, const char *name) {
	uint64_t hash = skg_hash(name);
	for (uint32_t i = 0; i < meta->buffer_count; i++) {
		if (meta->buffers[i].name_hash == hash)
			return meta->buffers[i].bind;
	}
	for (uint32_t i = 0; i < meta->resource_count; i++) {
		if (meta->resources[i].name_hash == hash)
			return meta->resources[i].bind;
	}
	skg_bind_t empty = {};
	return empty;
}

///////////////////////////////////////////

int32_t skg_shader_meta_get_var_count(const skg_shader_meta_t *meta) {
	return meta->global_buffer_id != -1
		? meta->buffers[meta->global_buffer_id].var_count
		: 0;
}

///////////////////////////////////////////

int32_t skg_shader_meta_get_var_index(const skg_shader_meta_t *meta, const char *name) {
	return skg_shader_meta_get_var_index_h(meta, skg_hash(name));
}

///////////////////////////////////////////

int32_t skg_shader_meta_get_var_index_h(const skg_shader_meta_t *meta, uint64_t name_hash) {
	if (meta->global_buffer_id == -1) return -1;

	skg_shader_buffer_t *buffer = &meta->buffers[meta->global_buffer_id];
	for (uint32_t i = 0; i < buffer->var_count; i++) {
		if (buffer->vars[i].name_hash == name_hash) {
			return i;
		}
	}
	return -1;
}

///////////////////////////////////////////

const skg_shader_var_t *skg_shader_meta_get_var_info(const skg_shader_meta_t *meta, int32_t var_index) {
	if (meta->global_buffer_id == -1 || var_index == -1) return nullptr;

	skg_shader_buffer_t *buffer = &meta->buffers[meta->global_buffer_id];
	return &buffer->vars[var_index];
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
		free(meta->resources);
		*meta = {};
	}
}

///////////////////////////////////////////
// skg_shader_t                          //
///////////////////////////////////////////

skg_shader_t skg_shader_create_file(const char *sks_filename) {
	skg_shader_file_t file;
	if (!skg_shader_file_load(sks_filename, &file)) {
		skg_shader_t empty = {};
		return empty;
	}

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
	if (!skg_shader_file_load_memory(sks_data, sks_data_size, &file)) {
		skg_shader_t empty = {};
		return empty;
	}

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

skg_bind_t skg_shader_get_bind(const skg_shader_t *shader, const char *name) {
	return skg_shader_meta_get_bind(shader->meta, name);
}

///////////////////////////////////////////

int32_t skg_shader_get_var_count(const skg_shader_t *shader) {
	return skg_shader_meta_get_var_count(shader->meta);
}

///////////////////////////////////////////

int32_t skg_shader_get_var_index(const skg_shader_t *shader, const char *name) {
	return skg_shader_meta_get_var_index_h(shader->meta, skg_hash(name));
}

///////////////////////////////////////////

int32_t skg_shader_get_var_index_h(const skg_shader_t *shader, uint64_t name_hash) {
	return skg_shader_meta_get_var_index_h(shader->meta, name_hash);
}

///////////////////////////////////////////

const skg_shader_var_t *skg_shader_get_var_info(const skg_shader_t *shader, int32_t var_index) {
	return skg_shader_meta_get_var_info(shader->meta, var_index);
}

///////////////////////////////////////////

uint32_t skg_tex_fmt_size(skg_tex_fmt_ format) {
	switch (format) {
	case skg_tex_fmt_rgba32:
	case skg_tex_fmt_rgba32_linear:
	case skg_tex_fmt_bgra32:
	case skg_tex_fmt_bgra32_linear:
	case skg_tex_fmt_rg11b10: 
	case skg_tex_fmt_rgb10a2:       return sizeof(uint8_t )*4;
	case skg_tex_fmt_rgba64u:
	case skg_tex_fmt_rgba64s:
	case skg_tex_fmt_rgba64f:       return sizeof(uint16_t)*4;
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