#include "app.h"

#include "../../src/sk_gpu_dev.h"
#include "HandmadeMath.h"

#define MICRO_PLY_IMPL
#include "micro_ply.h"

#include "test.hlsl.h"
#include "cubemap.hlsl.h"
#include "compute_test.hlsl.h"

#include <stdlib.h>
#include <string.h>

#ifndef __EMSCRIPTEN__
#include <stdio.h>
#endif

///////////////////////////////////////////

struct app_mesh_t {
	skg_buffer_t vert_buffer;
	skg_buffer_t ind_buffer;
	skg_mesh_t   mesh;
	int32_t      ind_count;
};

struct app_shader_data_t {
	float view_proj[16];
};

struct app_shader_inst_t {
	float world[16];
};

///////////////////////////////////////////

app_shader_data_t app_shader_data        = {};
app_shader_inst_t app_shader_inst[100]   = {};
app_mesh_t        app_mesh_cube          = {};
app_mesh_t        app_mesh_pyramid       = {};
app_mesh_t        app_mesh_tri           = {};
app_mesh_t        app_mesh_quad          = {};
app_mesh_t        app_mesh_wave          = {};
app_mesh_t        app_mesh_model         = {};
skg_buffer_t      app_shader_data_buffer = {};
skg_buffer_t      app_shader_inst_buffer = {};
skg_tex_t         app_tex                = {};
skg_tex_t         app_tex_white          = {};
skg_tex_t         app_tex_gradient_srgb  = {};
skg_tex_t         app_tex_gradient_linear= {};
skg_tex_t         app_target             = {};
skg_tex_t         app_target_depth       = {};
skg_tex_t         app_cubemap            = {};
skg_tex_t         app_particle           = {};

skg_tex_t         app_tex_colspace[4];
skg_color32_t   (*app_col_func[4])(float h, float s, float v, float a) = {
	skg_col_hsl32,
	skg_col_helix32,
	skg_col_jsl32,
	skg_col_lch32 };
skg_color128_t   (*app_col_func128[4])(float h, float s, float v, float a) = {
	skg_col_hsl128,
	skg_col_helix128,
	skg_col_jsl128,
	skg_col_lch128 };
const char *app_col_name[4] = {
	"hsl.tga",
	"helix.tga",
	"jsl.tga",
	"lch.tga" };

skg_shader_t      app_sh_default           = {};
skg_bind_t        app_sh_default_tex_bind  = {};
skg_bind_t        app_sh_default_inst_bind = {};
skg_bind_t        app_sh_default_data_bind = {};
skg_pipeline_t    app_mat_default          = {};
skg_pipeline_t    app_mat_transparent      = {};
skg_shader_t      app_sh_cube              = {};
skg_pipeline_t    app_mat_cube             = {};
skg_bind_t        app_sh_cube_tex_bind     = {};
skg_bind_t        app_sh_cube_cubemap_bind = {};
skg_bind_t        app_sh_cube_inst_bind    = {};
skg_bind_t        app_sh_cube_data_bind    = {};

skg_buffer_t app_compute_buffer = {};

const int32_t app_wave_size = 32;
skg_vert_t    app_wave_verts[app_wave_size * app_wave_size];

// Make a cube
skg_vert_t app_cube_verts[24];
uint32_t   app_cube_inds [36] = {
	0, 1, 2,  0, 2, 3,  4, 5, 6,  4, 6, 7, 
	8, 9, 10, 8, 10,11, 12,13,14, 12,14,15, 
	16,17,18, 16,18,19, 20,21,22, 20,22,23 };

// Make a pyramid
skg_vert_t app_pyramid_verts[] = {
	skg_vert_t{ { 0, 1, 0}, { 0, 1, 0}, {0.00f,1}, {255,255,255,255}},
	skg_vert_t{ {-1,-1,-1}, {-1,-1,-1}, {0.00f,0}, {0,255,0,255}},
	skg_vert_t{ { 1,-1,-1}, { 1,-1,-1}, {0.25f,0}, {0,0,255,255}},
	skg_vert_t{ { 1,-1, 1}, {-1,-1, 1}, {0.50f,0}, {255,255,0,255}},
	skg_vert_t{ {-1,-1, 1}, { 1,-1, 1}, {0.75f,0}, {255,0,255,255}},};
uint32_t app_pyramid_inds[] = {
	2,1,0, 3,2,0, 4,3,0, 1,4,0, 1,2,3, 1,3,4 };

// make a double-sided triangle
skg_vert_t app_tri_verts[] = {
	skg_vert_t{ {-.7f,-.5f,0}, {0,1,0}, {0,0}, {255,0,0,255}},
	skg_vert_t{ { .0f, .5f,0}, {0,1,0}, {0,0}, {0,255,0,255}},
	skg_vert_t{ { .7f,-.5f,0}, {0,1,0}, {0,0}, {0,0,255,255}},};
uint32_t app_tri_inds[] = {
	0,1,2, 2,1,0 };

// make a double-sided quad
skg_vert_t app_quad_verts[] = {
	skg_vert_t{ {-.5f, .5f,0}, {0,1,0}, {0,0}, {255,255,255,255}},
	skg_vert_t{ { .5f, .5f,0}, {0,1,0}, {1,0}, {255,255,255,255}},
	skg_vert_t{ { .5f,-.5f,0}, {0,1,0}, {1,1}, {255,255,255,255}},
	skg_vert_t{ {-.5f,-.5f,0}, {0,1,0}, {0,1}, {255,255,255,255}},};
uint32_t app_quad_inds[] = {
	0,1,2, 2,1,0, 0,2,3, 3,2,0 };

///////////////////////////////////////////

app_mesh_t app_mesh_create(const skg_vert_t *verts, int32_t vert_count, bool vert_dyn, const uint32_t *inds, int32_t ind_count);
void       app_mesh_destroy(app_mesh_t *mesh);
bool       ply_read_skg(const char *filename, skg_vert_t **out_verts, int32_t *out_vert_count, uint32_t **out_indices, int32_t *out_ind_count);
void       tga_write(const char *filename, uint32_t width, uint32_t height, uint8_t *dataBGRA, uint8_t dataChannels = 4, uint8_t fileChannels = 3);

///////////////////////////////////////////

bool app_init() {
	//app_compute_buffer = skg_buffer_create(app_wave_verts, app_wave_size * app_wave_size, sizeof(skg_vert_t), skg_buffer_type_compute, skg_use_dynamic);
	skg_vert_t *platform_verts;
	uint32_t   *platform_inds;
	int32_t     platform_v_count, platform_i_count;
	if (ply_read_skg("../platform.ply", &platform_verts, &platform_v_count, &platform_inds, &platform_i_count)) {
		app_mesh_model = app_mesh_create(platform_verts, platform_v_count, false, platform_inds, platform_i_count);
		free(platform_verts);
		free(platform_inds );
	} else {
		skg_log(skg_log_warning, "Couldn't load platform.ply!");
	}
	
	// Generate cube verts
	for (size_t i = 0; i < 24; i++) {
		float neg = (float)((i / 4) % 2 ? -1 : 1);
		int nx  = ((i+24) / 16) % 2;
		int ny  = (i / 8)       % 2;
		int nz  = (i / 16)      % 2;
		int u   = ((i+1) / 2)   % 2; // U: 0,1,1,0
		int v   = (i / 2)       % 2; // V: 0,0,1,1
		skg_vert_t vert = {
			{ (nx ? neg : ny ? (u?-1:1)*neg : (u?1:-1)*neg), 
			  (nx || nz ? (v?1:-1) : neg), 
			  (nx ? (u?-1:1)*neg : ny ? (v?1:-1) : neg) },
			{ nx*neg, ny*neg, nz*neg }, 
			{ (float)u, (float)v },
			{ 255, 255, 255, 255 } };
		app_cube_verts[i] = vert;
	}

	app_mesh_cube    = app_mesh_create(app_cube_verts,    sizeof(app_cube_verts   )/sizeof(skg_vert_t), false, app_cube_inds,    sizeof(app_cube_inds   )/sizeof(uint32_t));
	app_mesh_pyramid = app_mesh_create(app_pyramid_verts, sizeof(app_pyramid_verts)/sizeof(skg_vert_t), false, app_pyramid_inds, sizeof(app_pyramid_inds)/sizeof(uint32_t));
	app_mesh_tri     = app_mesh_create(app_tri_verts,     sizeof(app_tri_verts    )/sizeof(skg_vert_t), false, app_tri_inds,     sizeof(app_tri_inds    )/sizeof(uint32_t));
	app_mesh_quad    = app_mesh_create(app_quad_verts,    sizeof(app_quad_verts   )/sizeof(skg_vert_t), false, app_quad_inds,    sizeof(app_quad_inds   )/sizeof(uint32_t));

	// Make wave indices
	uint32_t inds_wave[(app_wave_size - 1) * (app_wave_size - 1) * 6];
	int32_t  curr = 0;
	for (int32_t y = 0; y < app_wave_size-1; y++) {
		for (int32_t x = 0; x < app_wave_size-1; x++) {
			inds_wave[curr++] = (x+1) + (y+1) * app_wave_size;
			inds_wave[curr++] = (x+1) + (y  ) * app_wave_size;
			inds_wave[curr++] = (x  ) + (y+1) * app_wave_size;

			inds_wave[curr++] = (x  ) + (y+1) * app_wave_size;
			inds_wave[curr++] = (x+1) + (y  ) * app_wave_size;
			inds_wave[curr++] = (x  ) + (y  ) * app_wave_size;
		}
	}
	app_mesh_wave = app_mesh_create(app_wave_verts, sizeof(app_wave_verts)/sizeof(skg_vert_t), true, inds_wave, sizeof(inds_wave)/sizeof(uint32_t));

	// Make a checkered texture
	int32_t w = 512, h = 512;
	skg_color32_t *colors = (skg_color32_t*)malloc(sizeof(skg_color32_t)* w * h);
	for (int32_t y = 0; y < h; y++) {
	for (int32_t x = 0; x < w; x++) {
		int32_t i  = x + y*w;
		float   c  = (x/32 + y/32) % 2 == 0 ? 1 : y/(float)h;
		uint8_t c8 = (uint8_t)(c * 255);
		colors[i] = { c8,c8,c8,c8 };
	} }
	app_tex = skg_tex_create(skg_tex_type_image, skg_use_static, skg_tex_fmt_rgba32, skg_mip_generate);
	skg_tex_settings    (&app_tex, skg_tex_address_clamp, skg_tex_sample_linear, 0);
	skg_tex_set_contents(&app_tex, colors, w, h);
	free(colors);

	// Make a particle texture
	w = 64; h = 64;
	colors = (skg_color32_t*)malloc(sizeof(skg_color32_t)* w * h);
	for (int32_t y = 0; y < h; y++) {
	for (int32_t x = 0; x < w; x++) {
		int32_t i  = x + y*w;

		float dx = ((w/2.0f)-x) / (w/2.0f);
		float dy = ((h/2.0f)-y) / (h/2.0f);
		
		float   c  =  fmaxf(0,1-sqrtf(dx*dx+dy*dy));
		uint8_t c8 = (uint8_t)(c * 255);
		colors[i] = { c8,c8,c8,c8 };
	} }
	app_particle = skg_tex_create(skg_tex_type_image, skg_use_static, skg_tex_fmt_rgba32_linear, skg_mip_generate);
	skg_tex_settings    (&app_particle, skg_tex_address_clamp, skg_tex_sample_linear, 0);
	skg_tex_set_contents(&app_particle, colors, w, h);
	free(colors);

	// Make a plain white texture
	skg_color32_t colors_wht[2*2];
	for (int32_t i = 0; i < sizeof(colors_wht)/sizeof(skg_color32_t); i++) {
		colors_wht[i] = {255,255,255,255};
	}
	app_tex_white = skg_tex_create(skg_tex_type_image, skg_use_static, skg_tex_fmt_rgba32, skg_mip_generate);
	skg_tex_set_contents(&app_tex_white, colors_wht, 2, 2);

	// Make srgb and linear gradients
	const int32_t gw = 64, gh = 16;
	colors = (skg_color32_t*)malloc(sizeof(skg_color32_t)* gw * gh);
	for (int32_t y = 0; y < gh; y++) {
	for (int32_t x = 0; x < gw; x++) {
		int32_t  i = x + y*gw;
		uint8_t c8 = (uint8_t)((x/(float)gw) * 255);
		colors[i] = { c8,c8,c8,255 };
	} }
	app_tex_gradient_srgb   = skg_tex_create(skg_tex_type_image, skg_use_static, skg_tex_fmt_rgba32, skg_mip_generate);
	app_tex_gradient_linear = skg_tex_create(skg_tex_type_image, skg_use_static, skg_tex_fmt_rgba32_linear, skg_mip_generate);
	skg_tex_settings    (&app_tex_gradient_srgb,   skg_tex_address_clamp, skg_tex_sample_linear, 0);
	skg_tex_settings    (&app_tex_gradient_linear, skg_tex_address_clamp, skg_tex_sample_linear, 0);
	skg_tex_set_contents(&app_tex_gradient_srgb,   colors, gw, gh);
	skg_tex_set_contents(&app_tex_gradient_linear, colors, gw, gh);
	free(colors);

	// make color space gradients
	const int32_t grad_size = 128;
	skg_color32_t *space_colors = (skg_color32_t*)malloc(sizeof(skg_color32_t)*grad_size*grad_size);
	for (int32_t c = 0; c < sizeof(app_tex_colspace) / sizeof(app_tex_colspace[0]); c++) {
		skg_color128_t grad_lab_start = skg_col_rgb_to_lab128(app_col_func128[c](0.5f, 0.8f, 0.8f, 1));
		skg_color128_t grad_lab_end   = skg_col_rgb_to_lab128(app_col_func128[c](1.f,  0.8f, 0.2f, 1));

		for (int32_t y = 0; y < grad_size; y++) {
			for (int32_t x = 0; x < grad_size; x++) {
				float dx = (grad_size-1) / 2.0f - x;
				float dy = (grad_size-1) / 2.0f - y;
				float d = sqrtf(dx*dx+dy*dy);
				float a = d==0?0:atan2f(dy/d, dx/d)/(3.14159f*2);
				d = 1 - d / (grad_size / 2);
				//space_colors[x+y*grad_size] = app_col_func[c](a, .8f, fmaxf(0,fmin(1,d)), fmaxf(0,fmin(1,d*grad_size)));

				space_colors[x+y*grad_size] = app_col_func[c](x/(float)grad_size, 1-y/(float)grad_size, 0.5f, 1);

				/*float pct = x / (float)grad_size;
				skg_color128_t lab = {};
				for (int32_t h = 0; h < 3; h++)
					lab.arr[h] = grad_lab_start.arr[h] + (grad_lab_end.arr[h] - grad_lab_start.arr[h]) * pct;
				space_colors[x + y * grad_size] = skg_col_lab32(lab.r, lab.g, lab.b, 1); */
			}
		}
		app_tex_colspace[c] = skg_tex_create(skg_tex_type_image, skg_use_dynamic, skg_tex_fmt_rgba32, skg_mip_none);
		skg_tex_settings    (&app_tex_colspace[c], skg_tex_address_clamp, skg_tex_sample_linear, 1);
		skg_tex_set_contents(&app_tex_colspace[c], space_colors, grad_size, grad_size);
		tga_write(app_col_name[c], grad_size, grad_size, (uint8_t*)space_colors, 4, 4);
	}
	free(space_colors);

	app_target       = skg_tex_create(skg_tex_type_rendertarget, skg_use_static, skg_tex_fmt_rgba32_linear, skg_mip_none);
	app_target_depth = skg_tex_create(skg_tex_type_depth,        skg_use_static, skg_tex_fmt_depth16,       skg_mip_none);
	skg_tex_set_contents(&app_target,       nullptr, 512, 512);
	skg_tex_set_contents(&app_target_depth, nullptr, 512, 512);
	skg_tex_attach_depth(&app_target, &app_target_depth);

	app_cubemap = skg_tex_create(skg_tex_type_cubemap, skg_use_static, skg_tex_fmt_rgba32, skg_mip_none);
	skg_color32_t *cube_cols[6];
	const int32_t  cube_face_size = 64;
	for (size_t f = 0; f < 6; f++) {
		cube_cols[f] = (skg_color32_t*)malloc(sizeof(skg_color32_t) * cube_face_size*cube_face_size);
		for (size_t p = 0; p < cube_face_size*cube_face_size; p++) {
			skg_color32_t col;
			switch ((f/2) % 3) {
			case 0: col = skg_col_helix32(.09f, f%2==0?0.6f:.6f, f%2==0?0.7f:.4f, 1); break;
			case 1: col = skg_col_helix32(.45f, f%2==0?0.6f:.6f, f%2==0?0.7f:.4f, 1); break;
			case 2: col = skg_col_helix32(.78f, f%2==0?0.6f:.6f, f%2==0?0.7f:.4f, 1); break;
			}
			cube_cols[f][p] = col;
		}
	}
	skg_tex_set_contents_arr(&app_cubemap, (const void**)&cube_cols, 6, cube_face_size, cube_face_size);

	app_sh_cube              = skg_shader_create_memory(sks_cubemap_hlsl, sizeof(sks_cubemap_hlsl));
	app_sh_cube_tex_bind     = skg_shader_get_tex_bind   (&app_sh_cube, "tex");
	app_sh_cube_cubemap_bind = skg_shader_get_tex_bind   (&app_sh_cube, "cubemap");
	app_sh_cube_inst_bind    = skg_shader_get_buffer_bind(&app_sh_cube, "TransformBuffer");
	app_sh_cube_data_bind    = skg_shader_get_buffer_bind(&app_sh_cube, "SystemBuffer");
	app_mat_cube             = skg_pipeline_create(&app_sh_cube);
	skg_pipeline_set_cull       (&app_mat_cube, skg_cull_front);
	skg_pipeline_set_depth_write(&app_mat_cube, false);
	skg_pipeline_set_scissor    (&app_mat_cube, true);

	app_mat_transparent = skg_pipeline_create(&app_sh_cube);
	skg_pipeline_set_depth_write (&app_mat_transparent, false);
	skg_pipeline_set_cull        (&app_mat_transparent, skg_cull_none);
	skg_pipeline_set_transparency(&app_mat_transparent, skg_transparency_add);
	
	app_sh_default           = skg_shader_create_memory(sks_test_hlsl, sizeof(sks_test_hlsl));
	app_sh_default_tex_bind  = skg_shader_get_tex_bind   (&app_sh_default, "tex");
	app_sh_default_inst_bind = skg_shader_get_buffer_bind(&app_sh_default, "TransformBuffer");
	app_sh_default_data_bind = skg_shader_get_buffer_bind(&app_sh_default, "SystemBuffer");
	app_mat_default          = skg_pipeline_create(&app_sh_default);
	
	app_shader_data_buffer = skg_buffer_create(&app_shader_data, 1,   sizeof(app_shader_data_t), skg_buffer_type_constant, skg_use_dynamic);
	app_shader_inst_buffer = skg_buffer_create(&app_shader_inst, 100, sizeof(app_shader_inst_t), skg_buffer_type_constant, skg_use_dynamic);

	skg_color32_t      *data      = (skg_color32_t*)malloc(sizeof(skg_color32_t) * 1024*1024);
	for (size_t i = 0; i < 1024*1024; i++) {
		data[i] = { 255,0,0,0 };
	}
	skg_buffer_t cbuff     = skg_buffer_create(data, 1024*1024, sizeof(skg_color32_t), skg_buffer_type_compute, skg_use_compute_read);
	skg_buffer_t cbuff_out = skg_buffer_create(data, 1024*1024, sizeof(skg_color32_t), skg_buffer_type_compute, skg_use_compute_write);
	skg_shader_t cshader   = skg_shader_create_memory(sks_compute_test_hlsl, sizeof(sks_compute_test_hlsl));
	skg_buffer_compute_bind(&cbuff,     { 0 });
	skg_buffer_compute_bind(&cbuff_out, { 0 });
	skg_shader_compute_bind(&cshader);
	skg_compute(16, 16, 16);
	skg_buffer_get_contents(&cbuff_out, data, sizeof(skg_color32_t) * 1024 * 1024);
	for (size_t i = 0; i < 10; i++) {
		printf("%d, %d, %d, %d\n", data[i].r, data[i].g, data[i].b, data[i].a);
	}
	skg_buffer_destroy(&cbuff);
	skg_buffer_destroy(&cbuff_out);
	skg_shader_destroy       (&cshader);
	free(data);

	return true;
}

///////////////////////////////////////////

void app_test_dyn_update(float time) {

	for (int32_t y = 0; y < app_wave_size; y++) {
		for (int32_t x = 0; x < app_wave_size; x++) {
			int32_t i  = x + y * app_wave_size;
			float   xp = x/(float)(app_wave_size-1);
			float   yp = y/(float)(app_wave_size-1);
			float   t  = ((xp + yp)*8 + ((float)time*0.005f)) * 0.7f;
			app_wave_verts[i].pos[0] = x/(float)app_wave_size-0.5f;
			app_wave_verts[i].pos[1] = sinf(t)*0.1f;
			app_wave_verts[i].pos[2] = y/(float)app_wave_size-0.5f;

			float c   = -cosf(t);
			float mag = sqrtf(c*c + 1*1 + c*c);
			app_wave_verts[i].norm[0] = c/mag;
			app_wave_verts[i].norm[1] = .6f/mag;
			app_wave_verts[i].norm[2] = c/mag;

			app_wave_verts[i].col = {255,255,255,255};

			app_wave_verts[i].uv[0] = xp;
			app_wave_verts[i].uv[1] = yp;
		}
	}
	skg_buffer_set_contents(&app_mesh_wave.vert_buffer, app_wave_verts, sizeof(app_wave_verts));

	hmm_mat4 world = HMM_Transpose(HMM_Translate(hmm_vec3{ {0,-2,0} }) * HMM_Scale(hmm_vec3{ {6,6,6} }));
	memcpy(&app_shader_inst[0].world, &world, sizeof(float) * 16);
	skg_buffer_set_contents(&app_shader_inst_buffer, &app_shader_inst, sizeof(app_shader_inst_t) );
	skg_buffer_bind        (&app_shader_inst_buffer, app_sh_default_inst_bind, 0);

	skg_mesh_bind    (&app_mesh_wave.mesh);
	skg_pipeline_bind(&app_mat_default);
	skg_tex_bind     (&app_target, app_sh_default_tex_bind);
	skg_draw(0, 0, app_mesh_wave.ind_count, 1);
}

///////////////////////////////////////////

void app_test_colors(float t) {
	// Here's how this triangle should look:
	// https://medium.com/@heypete/hello-triangle-meet-swift-and-wide-color-6f9e246616d9
	hmm_mat4 world = HMM_Transpose(HMM_Translate(hmm_vec3{ {0,2,0} }) * HMM_Scale(hmm_vec3{ {1,1,1} }));
	memcpy(&app_shader_inst[0].world, &world, sizeof(float) * 16);
	skg_buffer_set_contents(&app_shader_inst_buffer, &app_shader_inst, sizeof(app_shader_inst_t) );
	skg_buffer_bind        (&app_shader_inst_buffer, app_sh_default_inst_bind, 0);

	skg_mesh_bind    (&app_mesh_tri.mesh);
	skg_pipeline_bind(&app_mat_default);
	skg_tex_bind     (&app_tex_white, app_sh_default_tex_bind);
	skg_draw(0, 0, app_mesh_tri.ind_count, 1);

	// Just to make sure, here's a pair of gradient strips, srgb and linear
	// sRGB
	world = HMM_Transpose(HMM_Translate(hmm_vec3{ {0,3,0} }) * HMM_Scale(hmm_vec3{ {1,.25f,1} }));
	memcpy(&app_shader_inst[0].world, &world, sizeof(float) * 16);
	skg_buffer_set_contents(&app_shader_inst_buffer, &app_shader_inst, sizeof(app_shader_inst_t) );
	skg_buffer_bind        (&app_shader_inst_buffer, app_sh_default_inst_bind, 0);

	skg_mesh_bind    (&app_mesh_quad.mesh);
	skg_pipeline_bind(&app_mat_default);
	skg_tex_bind     (&app_tex_gradient_srgb, app_sh_default_tex_bind);
	skg_draw(0, 0, app_mesh_quad.ind_count, 1);

	// and linear
	world = HMM_Transpose(HMM_Translate(hmm_vec3{ {0,2.75f,0} }) * HMM_Scale(hmm_vec3{ {1,.25f,1} }));
	memcpy(&app_shader_inst[0].world, &world, sizeof(float) * 16);
	skg_buffer_set_contents(&app_shader_inst_buffer, &app_shader_inst, sizeof(app_shader_inst_t) );
	skg_buffer_bind        (&app_shader_inst_buffer, app_sh_default_inst_bind, 0);

	skg_mesh_bind    (&app_mesh_quad.mesh);
	skg_pipeline_bind(&app_mat_default);
	skg_tex_bind     (&app_tex_gradient_linear, app_sh_default_tex_bind);
	skg_draw(0, 0, app_mesh_quad.ind_count, 1);

	for (int32_t i = 0; i < sizeof(app_tex_colspace) / sizeof(app_tex_colspace[0]); i++) {
		world = HMM_Transpose(HMM_Translate(hmm_vec3{ {(i/2+1) * (i%2==0?-2.0f:2.0f),2,0} }) * HMM_Scale(hmm_vec3{ {1,1,1} }));
		memcpy(&app_shader_inst[0].world, &world, sizeof(float) * 16);
		skg_buffer_set_contents(&app_shader_inst_buffer, &app_shader_inst, sizeof(app_shader_inst_t) );
		skg_buffer_bind        (&app_shader_inst_buffer, app_sh_default_inst_bind, 0);

		skg_mesh_bind    (&app_mesh_quad.mesh);
		skg_pipeline_bind(&app_mat_default);
		skg_tex_bind     (&app_tex_colspace[i], app_sh_default_tex_bind);
		skg_draw(0, 0, app_mesh_quad.ind_count, 1);
	}
}

///////////////////////////////////////////

void app_test_cubemap() {
	hmm_mat4 world = HMM_Transpose(HMM_Translate(hmm_vec3{ {0,0,0} }) * HMM_Scale(hmm_vec3{ {6,6,6} }));
	memcpy(&app_shader_inst[0].world, &world, sizeof(float) * 16);
	skg_buffer_set_contents(&app_shader_inst_buffer, &app_shader_inst, sizeof(app_shader_inst_t) );
	skg_buffer_bind        (&app_shader_inst_buffer, app_sh_cube_inst_bind, 0);

	skg_mesh_bind    (&app_mesh_cube.mesh);
	skg_pipeline_bind(&app_mat_cube);
	skg_tex_bind     (&app_tex,     app_sh_cube_tex_bind);
	skg_tex_bind     (&app_cubemap, app_sh_cube_cubemap_bind);
	skg_draw(0, 0, app_mesh_cube.ind_count, 1);
}

///////////////////////////////////////////

void app_test_rendertarget(float t) {
	skg_tex_t *old_target = skg_tex_target_get();
	int32_t old_view[4];
	skg_viewport_get(old_view);

	float color[4] = { 1,1,1,1 };
	skg_tex_target_bind(&app_target);
	skg_target_clear(true, color);

	hmm_mat4 view = HMM_LookAt(
		HMM_Vec3(0,0,.75f),
		HMM_Vec3(0,0,0),
		HMM_Vec3(0,1,0));
	hmm_mat4 proj      = HMM_Perspective(45, 1, 0.01f, 100);
	hmm_mat4 view_proj = HMM_Transpose( proj * view );
	memcpy(app_shader_data.view_proj, &view_proj, sizeof(float) * 16);
	skg_buffer_set_contents(&app_shader_data_buffer, &app_shader_data, sizeof(app_shader_data));
	skg_buffer_bind        (&app_shader_data_buffer, app_sh_default_data_bind, 0);

	hmm_mat4 world = HMM_Transpose(HMM_Translate(hmm_vec3{ {0,0,0} }) * HMM_Scale(hmm_vec3{ {.4f,.4f,.4f} }) *HMM_Rotate((float)t * 0.05f, hmm_vec3{ {0,1,0} }));
	memcpy(&app_shader_inst[0].world, &world, sizeof(float) * 16);
	skg_buffer_set_contents(&app_shader_inst_buffer, &app_shader_inst,         sizeof(app_shader_inst_t));
	skg_buffer_bind        (&app_shader_inst_buffer, app_sh_default_inst_bind, 0);

	skg_mesh_bind    (&app_mesh_tri.mesh);
	skg_pipeline_bind(&app_mat_default);
	skg_tex_bind     (&app_tex_white, app_sh_default_tex_bind);
	skg_draw         (0, 0, app_mesh_tri.ind_count, 1);

	skg_tex_target_bind(old_target);
	skg_viewport(old_view);

	static bool has_saved = false;
	if (!has_saved) {
		has_saved = true;
		size_t   size       = skg_tex_fmt_size(app_target.format) * app_target.width * app_target.height;
		uint8_t *color_data = (uint8_t*)malloc(size);
		if (skg_tex_get_contents(&app_target, color_data, size))
			tga_write("test.tga", app_target.width, app_target.height, color_data);
		free(color_data);
	}
}

///////////////////////////////////////////

void app_test_blend() {
	hmm_mat4 world = HMM_Transpose(HMM_Translate(hmm_vec3{ {0,.5f,0} }) * HMM_Scale(hmm_vec3{ {.5f,.5f,.5f} }));
	memcpy(&app_shader_inst[0].world, &world, sizeof(float) * 16);
	skg_buffer_set_contents(&app_shader_inst_buffer, &app_shader_inst, sizeof(app_shader_inst_t) );
	skg_buffer_bind        (&app_shader_inst_buffer, app_sh_cube_inst_bind, 0);

	skg_mesh_bind    (&app_mesh_cube.mesh);
	skg_pipeline_bind(&app_mat_transparent);
	skg_tex_bind     (&app_particle, app_sh_default_tex_bind);
	skg_draw(0, 0, app_mesh_cube.ind_count, 1);
}

///////////////////////////////////////////

void app_test_instancing() {
	// Set transforms for another 16 instances
	for (int32_t i = 0; i < 16; i++) {
		int32_t y = i / 4 - 1, x = i % 4 - 1;
		hmm_mat4 world = HMM_Transpose(HMM_Translate(hmm_vec3{ {((float)x-0.5f)*2.5f,0,((float)y-0.5f)*2.5f} }) * HMM_Scale(hmm_vec3{{.4f,.4f,.4f}}));
		memcpy(&app_shader_inst[i].world, &world, sizeof(float) * 16);
	}
	skg_buffer_set_contents(&app_shader_inst_buffer, &app_shader_inst,         sizeof(app_shader_inst));
	skg_buffer_bind        (&app_shader_inst_buffer, app_sh_default_inst_bind, 0);

	app_mesh_t *mesh = skg_buffer_is_valid(&app_mesh_model.vert_buffer)
		? &app_mesh_model
		: &app_mesh_cube;
	skg_mesh_bind    (&mesh->mesh);
	skg_pipeline_bind(&app_mat_default);
	skg_tex_bind     (&app_tex, app_sh_default_tex_bind);
	skg_draw         (0, 0, mesh->ind_count, 16);
}

///////////////////////////////////////////

void app_render(float t, hmm_mat4 view, hmm_mat4 proj) {
	int32_t viewport[4];
	skg_viewport_get(viewport);
	viewport[0] += 40;
	viewport[1] += 40;
	viewport[2] -= 80;
	viewport[3] -= 100;
	skg_scissor(viewport);

	app_test_rendertarget(t);

	hmm_mat4 view_proj = HMM_Transpose( proj * view );
	memcpy(app_shader_data.view_proj, &view_proj, sizeof(float) * 16);
	skg_buffer_set_contents(&app_shader_data_buffer, &app_shader_data,         sizeof(app_shader_data));
	skg_buffer_bind        (&app_shader_data_buffer, app_sh_default_data_bind, 0);

	app_test_colors(t);
	app_test_cubemap();
	app_test_dyn_update(t);
	app_test_instancing();
	app_test_blend();
}

///////////////////////////////////////////

void app_shutdown() {
	skg_buffer_destroy(&app_shader_data_buffer);
	skg_buffer_destroy(&app_shader_inst_buffer);
	skg_pipeline_destroy(&app_mat_default);
	skg_pipeline_destroy(&app_mat_cube);
	skg_shader_destroy(&app_sh_default);
	skg_shader_destroy(&app_sh_cube);
	skg_tex_destroy(&app_target_depth);
	skg_tex_destroy(&app_target);
	skg_tex_destroy(&app_tex);
	app_mesh_destroy(&app_mesh_cube);
	app_mesh_destroy(&app_mesh_pyramid);
	app_mesh_destroy(&app_mesh_tri);
	app_mesh_destroy(&app_mesh_wave);
}

///////////////////////////////////////////

app_mesh_t app_mesh_create(const skg_vert_t *verts, int32_t vert_count, bool vert_dyn, const uint32_t *inds, int32_t ind_count) {
	app_mesh_t result = {};
	result.vert_buffer = skg_buffer_create(verts, vert_count, sizeof(skg_vert_t), skg_buffer_type_vertex, vert_dyn ? skg_use_dynamic : skg_use_static);
	result.ind_buffer  = skg_buffer_create(inds,  ind_count,  sizeof(uint32_t),   skg_buffer_type_index,  skg_use_static);
	result.mesh        = skg_mesh_create(&result.vert_buffer, &result.ind_buffer);
	result.ind_count   = ind_count;
	return result;
}

///////////////////////////////////////////

void app_mesh_destroy(app_mesh_t *mesh) {
	skg_mesh_destroy  (&mesh->mesh);
	skg_buffer_destroy(&mesh->vert_buffer);
	skg_buffer_destroy(&mesh->ind_buffer);
	*mesh = {};
}

///////////////////////////////////////////

bool ply_read_skg(const char *filename, skg_vert_t **out_verts, int32_t *out_vert_count, uint32_t **out_indices, int32_t *out_ind_count) {
	void  *data;
	size_t size;
	if (!skg_read_file(filename, &data, &size))
		return false;

	ply_file_t file;
	if (!ply_read(data, size, &file))
		return false;

	float     fzero = 0;
	uint8_t   white = 255;
	ply_map_t map_verts[] = {
		{ PLY_PROP_POSITION_X,  ply_prop_decimal, sizeof(float), 0,  &fzero },
		{ PLY_PROP_POSITION_Y,  ply_prop_decimal, sizeof(float), 4,  &fzero },
		{ PLY_PROP_POSITION_Z,  ply_prop_decimal, sizeof(float), 8,  &fzero },
		{ PLY_PROP_NORMAL_X,    ply_prop_decimal, sizeof(float), 12, &fzero },
		{ PLY_PROP_NORMAL_Y,    ply_prop_decimal, sizeof(float), 16, &fzero },
		{ PLY_PROP_NORMAL_Z,    ply_prop_decimal, sizeof(float), 20, &fzero },
		{ PLY_PROP_TEXCOORD_X,  ply_prop_decimal, sizeof(float), 24, &fzero },
		{ PLY_PROP_TEXCOORD_Y,  ply_prop_decimal, sizeof(float), 28, &fzero },
		{ PLY_PROP_COLOR_R,     ply_prop_uint,    sizeof(uint8_t), 32, &white },
		{ PLY_PROP_COLOR_G,     ply_prop_uint,    sizeof(uint8_t), 33, &white },
		{ PLY_PROP_COLOR_B,     ply_prop_uint,    sizeof(uint8_t), 34, &white },
		{ PLY_PROP_COLOR_A,     ply_prop_uint,    sizeof(uint8_t), 35, &white }, };
	ply_convert(&file, PLY_ELEMENT_VERTICES, map_verts, sizeof(map_verts)/sizeof(map_verts[0]), sizeof(skg_vert_t), (void **)out_verts, out_vert_count);

	uint32_t  izero = 0;
	ply_map_t map_inds[] = { { PLY_PROP_INDICES, ply_prop_uint, sizeof(uint32_t), 0, &izero } };
	ply_convert(&file, PLY_ELEMENT_FACES, map_inds, sizeof(map_inds)/sizeof(map_inds[0]), sizeof(uint32_t), (void **)out_indices, out_ind_count);

	ply_free(&file);
	free(data);
	return true;
}

///////////////////////////////////////////

void tga_write(const char *filename, uint32_t width, uint32_t height, uint8_t *dataRGBA, uint8_t dataChannels, uint8_t fileChannels) {
#ifndef __EMSCRIPTEN__
	FILE *fp = NULL;
#ifdef __ANDROID__
	fp = fopen(filename, "wb");
#else
	fopen_s(&fp, filename, "wb");
#endif
	if (fp == NULL) return;

	// You can find details about TGA headers here: http://www.paulbourke.net/dataformats/tga/
	uint8_t header[18] = { 0,0,2,0,0,0,0,0,0,0,0,0, (uint8_t)(width%256), (uint8_t)(width/256), (uint8_t)(height%256), (uint8_t)(height/256), (uint8_t)(fileChannels*8), 0x20 };
	fwrite(&header, 18, 1, fp);

	for (uint32_t i = 0; i < width*height; i++) {
		if (fileChannels > 0) fputc(dataRGBA[(i*dataChannels) + (2%dataChannels)], fp);
		if (fileChannels > 1) fputc(dataRGBA[(i*dataChannels) + (1%dataChannels)], fp);
		if (fileChannels > 2) fputc(dataRGBA[(i*dataChannels) + (0%dataChannels)], fp);
		if (fileChannels > 3) fputc(dataRGBA[(i*dataChannels) + (3%dataChannels)], fp);
	}
	fclose(fp);
#endif
}

///////////////////////////////////////////
