#include "app.h"

#include "../../src/sk_gpu_dev.h"
#include "HandmadeMath.h"

#include "test.hlsl.h"
#include "cubemap.hlsl.h"

#include <stdlib.h>
#include <string.h>

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
app_mesh_t        app_mesh_wave          = {};
app_mesh_t        app_mesh_model         = {};
skg_buffer_t      app_shader_data_buffer = {};
skg_buffer_t      app_shader_inst_buffer = {};
skg_tex_t         app_tex                = {};
skg_tex_t         app_tex_white          = {};
skg_tex_t         app_target             = {};
skg_tex_t         app_target_depth       = {};
skg_tex_t         app_cubemap            = {};

skg_shader_t      app_sh_default           = {};
skg_bind_t        app_sh_default_tex_bind  = {};
skg_bind_t        app_sh_default_inst_bind = {};
skg_bind_t        app_sh_default_data_bind = {};
skg_pipeline_t    app_mat_default          = {};
skg_shader_t      app_sh_cube              = {};
skg_pipeline_t    app_mat_cube             = {};
skg_bind_t        app_sh_cube_tex_bind     = {};
skg_bind_t        app_sh_cube_cubemap_bind = {};
skg_bind_t        app_sh_cube_inst_bind    = {};
skg_bind_t        app_sh_cube_data_bind    = {};

skg_buffer_t app_compute_buffer = {};

const int32_t app_wave_size = 32;
skg_vert_t    app_wave_verts[app_wave_size * app_wave_size];

///////////////////////////////////////////

app_mesh_t app_mesh_create(const skg_vert_t *verts, int32_t vert_count, bool vert_dyn, const uint32_t *inds, int32_t ind_count);
void       app_mesh_destroy(app_mesh_t *mesh);

///////////////////////////////////////////

bool app_init() {
	app_compute_buffer = skg_buffer_create(app_wave_verts, app_wave_size * app_wave_size, sizeof(skg_vert_t), skg_buffer_type_compute, skg_use_dynamic);

	// Make a cube
	skg_vert_t verts[] = {
		skg_vert_t{ {-1,-1,-1}, {-1,-1,-1}, {0.00f,0}, {255,255,255,255}}, // Bottom verts
		skg_vert_t{ { 1,-1,-1}, { 1,-1,-1}, {0.50f,0}, {255,255,255,255}},
		skg_vert_t{ { 1, 1,-1}, { 1, 1,-1}, {1.00f,0}, {255,255,255,255}},
		skg_vert_t{ {-1, 1,-1}, {-1, 1,-1}, {0.50f,0}, {255,255,255,255}},
		skg_vert_t{ {-1,-1, 1}, {-1,-1, 1}, {0.00f,1}, {255,255,255,255}}, // Top verts
		skg_vert_t{ { 1,-1, 1}, { 1,-1, 1}, {0.50f,1}, {255,255,255,255}},
		skg_vert_t{ { 1, 1, 1}, { 1, 1, 1}, {1.00f,1}, {255,255,255,255}},
		skg_vert_t{ {-1, 1, 1}, {-1, 1, 1}, {0.50f,1}, {255,255,255,255}}, };
	uint32_t inds[] = {
		0,2,1, 0,3,2, 5,6,4, 4,6,7,
		1,2,6, 1,6,5, 4,7,3, 4,3,0,
		1,5,4, 1,4,0, 3,7,2, 7,6,2, };
	app_mesh_cube = app_mesh_create(verts, sizeof(verts)/sizeof(skg_vert_t), false, inds, sizeof(inds)/sizeof(uint32_t));

	// Make a pyramid
	skg_vert_t verts2[] = {
		skg_vert_t{ { 0, 1, 0}, { 0, 1, 0}, {0.00f,1}, {255,255,255,255}},
		skg_vert_t{ {-1,-1,-1}, {-1,-1,-1}, {0.00f,0}, {0,255,0,255}},
		skg_vert_t{ { 1,-1,-1}, { 1,-1,-1}, {0.25f,0}, {0,0,255,255}},
		skg_vert_t{ { 1,-1, 1}, {-1,-1, 1}, {0.50f,0}, {255,255,0,255}},
		skg_vert_t{ {-1,-1, 1}, { 1,-1, 1}, {0.75f,0}, {255,0,255,255}},};
	uint32_t inds2[] = {
		2,1,0, 3,2,0, 4,3,0, 1,4,0, 1,2,3, 1,3,4 };
	app_mesh_pyramid = app_mesh_create(verts2, sizeof(verts2)/sizeof(skg_vert_t), false, inds2, sizeof(inds2)/sizeof(uint32_t));

	// make a double-sided triangle
	skg_vert_t verts3[] = {
		skg_vert_t{ {-.7f,-.5f,0}, {0,1,0}, {0,0}, {255,0,0,255}},
		skg_vert_t{ { .0f, .5f,0}, {0,1,0}, {0,0}, {0,255,0,255}},
		skg_vert_t{ { .7f,-.5f,0}, {0,1,0}, {0,0}, {0,0,255,255}},};
	uint32_t inds3[] = {
		0,1,2, 2,1,0 };
	app_mesh_tri = app_mesh_create(verts3, sizeof(verts3)/sizeof(skg_vert_t), false, inds3, sizeof(inds3)/sizeof(uint32_t));

	// Make wave indices
	uint32_t inds_wave[(app_wave_size - 1) * (app_wave_size - 1) * 6];
	int32_t curr = 0;
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
	const int w = 128, h = 64;
	uint8_t colors[w * h * 4];
	for (int32_t y = 0; y < h; y++) {
		for (int32_t x = 0; x < w; x++) {
			int32_t i = (x + y * w) * 4;
			uint8_t c = (x/4 + y/4) % 2 == 0 ? 255 : 0;
			colors[i  ] = c;
			colors[i+1] = c;
			colors[i+2] = c;
			colors[i+3] = c;
		}
	}
	void *color_arr[1] = { colors };
	app_tex = skg_tex_create(skg_tex_type_image, skg_use_static, skg_tex_fmt_rgba32, skg_mip_generate);
	skg_tex_settings    (&app_tex, skg_tex_address_repeat, skg_tex_sample_linear, 0);
	skg_tex_set_contents(&app_tex, color_arr, 1, w, h);

	// Make a plain white texture
	uint8_t colors_wht[2*2*4];
	for (int32_t i = 0; i < sizeof(colors_wht); i++) {
		colors_wht[i] = 255;
	}
	void *color_wht_arr[1] = { colors };
	app_tex_white = skg_tex_create(skg_tex_type_image, skg_use_static, skg_tex_fmt_rgba32, skg_mip_generate);
	skg_tex_set_contents(&app_tex_white, color_wht_arr, 1, 2, 2);

	app_target       = skg_tex_create(skg_tex_type_rendertarget, skg_use_static, skg_tex_fmt_rgba32_linear, skg_mip_none);
	app_target_depth = skg_tex_create(skg_tex_type_depth,        skg_use_static, skg_tex_fmt_depth16,       skg_mip_none);
	skg_tex_set_contents(&app_target,       nullptr, 1, 512, 512);
	skg_tex_set_contents(&app_target_depth, nullptr, 1, 512, 512);
	skg_tex_attach_depth(&app_target, &app_target_depth);

	app_cubemap = skg_tex_create(skg_tex_type_cubemap, skg_use_static, skg_tex_fmt_rgba32, skg_mip_none);
	uint8_t *cube_cols[6];
	for (size_t f = 0; f < 6; f++) {
		cube_cols[f] = (uint8_t*)malloc(sizeof(uint8_t) * 4 * 4);
		for (size_t p = 0; p < 4; p++) {
			cube_cols[f][p*4 + 0] = (f/2) % 3 == 0 ? (f%2==0?128:255) : 0;
			cube_cols[f][p*4 + 1] = (f/2) % 3 == 1 ? (f%2==0?128:255) : 0;
			cube_cols[f][p*4 + 2] = (f/2) % 3 == 2 ? (f%2==0?128:255) : 0;
			cube_cols[f][p*4 + 3] = 255;
		}
	}
	skg_tex_set_contents(&app_cubemap, (void**)&cube_cols, 6, 2, 2);

	app_sh_cube              = skg_shader_create_memory(sks_cubemap_hlsl, sizeof(sks_cubemap_hlsl));
	app_sh_cube_tex_bind     = skg_shader_get_tex_bind   (&app_sh_cube, "tex");
	app_sh_cube_cubemap_bind = skg_shader_get_tex_bind   (&app_sh_cube, "cubemap");
	app_sh_cube_inst_bind    = skg_shader_get_buffer_bind(&app_sh_cube, "TransformBuffer");
	app_sh_cube_data_bind    = skg_shader_get_buffer_bind(&app_sh_cube, "SystemBuffer");
	app_mat_cube             = skg_pipeline_create(&app_sh_cube);
	skg_pipeline_set_cull(&app_mat_cube, skg_cull_front);
	skg_pipeline_set_depth_write(&app_mat_cube, false);
	
	app_sh_default           = skg_shader_create_memory(sks_test_hlsl, sizeof(sks_test_hlsl));
	app_sh_default_tex_bind  = skg_shader_get_tex_bind   (&app_sh_default, "tex");
	app_sh_default_inst_bind = skg_shader_get_buffer_bind(&app_sh_default, "TransformBuffer");
	app_sh_default_data_bind = skg_shader_get_buffer_bind(&app_sh_default, "SystemBuffer");
	app_mat_default          = skg_pipeline_create(&app_sh_default);
	
	app_shader_data_buffer = skg_buffer_create(&app_shader_data, 1,   sizeof(app_shader_data_t), skg_buffer_type_constant, skg_use_dynamic);
	app_shader_inst_buffer = skg_buffer_create(&app_shader_inst, 100, sizeof(app_shader_inst_t), skg_buffer_type_constant, skg_use_dynamic);
	return true;
}

///////////////////////////////////////////

void app_test_dyn_update(double time) {

	for (int32_t y = 0; y < app_wave_size; y++) {
		for (int32_t x = 0; x < app_wave_size; x++) {
			int32_t i  = x + y * app_wave_size;
			float   xp = x/(float)(app_wave_size-1);
			float   yp = y/(float)(app_wave_size-1);
			float   t  = ((xp + yp)*8 + (time*0.005f)) * 0.7f;
			app_wave_verts[i].pos[0] = x/(float)app_wave_size-0.5f;
			app_wave_verts[i].pos[1] = sinf(t)*0.1f;
			app_wave_verts[i].pos[2] = y/(float)app_wave_size-0.5f;

			float c   = -cosf(t);
			float mag = sqrtf(c*c + 1*1 + c*c);
			app_wave_verts[i].norm[0] = c/mag;
			app_wave_verts[i].norm[1] = .6f/mag;
			app_wave_verts[i].norm[2] = c/mag;

			app_wave_verts[i].col[0] = 255;
			app_wave_verts[i].col[1] = 255;
			app_wave_verts[i].col[2] = 255;
			app_wave_verts[i].col[3] = 255;

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

void app_test_colors() {
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

void app_test_rendertarget(double t) {
	skg_tex_t *old_target = skg_tex_target_get();

	float color[4] = { 1,1,1,1 };
	skg_tex_target_bind(&app_target, true, color);

	hmm_mat4 view = HMM_LookAt(
		HMM_Vec3(0,0,.75f),
		HMM_Vec3(0,0,0),
		HMM_Vec3(0,1,0));
	hmm_mat4 proj      = HMM_Perspective(45, 1, 0.01f, 100);
	hmm_mat4 view_proj = HMM_Transpose( proj * view );
	memcpy(app_shader_data.view_proj, &view_proj, sizeof(float) * 16);
	skg_buffer_set_contents(&app_shader_data_buffer, &app_shader_data, sizeof(app_shader_data));
	skg_buffer_bind        (&app_shader_data_buffer, app_sh_default_data_bind, 0);

	hmm_mat4 world = HMM_Transpose(HMM_Translate(hmm_vec3{ {0,0,0} }) * HMM_Scale(hmm_vec3{ {.4f,.4f,.4f} }) *HMM_Rotate(t * 0.05f, hmm_vec3{ {0,1,0} }));
	memcpy(&app_shader_inst[0].world, &world, sizeof(float) * 16);
	skg_buffer_set_contents(&app_shader_inst_buffer, &app_shader_inst,         sizeof(app_shader_inst_t));
	skg_buffer_bind        (&app_shader_inst_buffer, app_sh_default_inst_bind, 0);

	skg_mesh_bind    (&app_mesh_tri.mesh);
	skg_pipeline_bind(&app_mat_default);
	skg_tex_bind     (&app_tex_white, app_sh_default_tex_bind);
	skg_draw         (0, 0, app_mesh_tri.ind_count, 1);

	skg_tex_target_bind(old_target, false, color);
}

///////////////////////////////////////////

void app_test_instancing() {
	// Set transforms for 100 instances
	for (int32_t i = 0; i < 100; i++) {
		int32_t y = i / 10 - 4, x = i % 10 -4;
		hmm_mat4 world = HMM_Transpose(HMM_Translate(hmm_vec3{ {(float)x - 0.5f,0,(float)y - 0.5f} }) * HMM_Scale(hmm_vec3{ {.2f,.2f,.2f} }));
		memcpy(&app_shader_inst[i].world, &world, sizeof(float) * 16);
	}
	skg_buffer_set_contents(&app_shader_inst_buffer, &app_shader_inst,         sizeof(app_shader_inst));
	skg_buffer_bind        (&app_shader_inst_buffer, app_sh_default_inst_bind, 0);

	skg_mesh_bind    (&app_mesh_cube.mesh);
	skg_pipeline_bind(&app_mat_default);
	skg_tex_bind     (&app_tex, app_sh_default_tex_bind);
	skg_draw         (0, 0, app_mesh_cube.ind_count, 100);

	// Set transforms for another 100 instances
	for (int32_t i = 0; i < 100; i++) {
		int32_t y = i / 10 - 4, x = i % 10 -4;
		hmm_mat4 world = HMM_Transpose(HMM_Translate(hmm_vec3{ {(float)x -0.5f,1,(float)y-0.5f} }) * HMM_Scale(hmm_vec3{{.2f,.2f,.2f}}));
		memcpy(&app_shader_inst[i].world, &world, sizeof(float) * 16);
	}
	skg_buffer_set_contents(&app_shader_inst_buffer, &app_shader_inst,         sizeof(app_shader_inst));
	skg_buffer_bind        (&app_shader_inst_buffer, app_sh_default_inst_bind, 0);

	skg_mesh_bind    (&app_mesh_model.mesh);
	skg_pipeline_bind(&app_mat_default);
	skg_tex_bind     (&app_target, app_sh_default_tex_bind);
	skg_draw         (0, 0, app_mesh_model.ind_count, 100);
}

///////////////////////////////////////////

void app_render(double t, hmm_mat4 view, hmm_mat4 proj) {
	
	app_test_rendertarget(t);

	hmm_mat4 view_proj = HMM_Transpose( proj * view );
	memcpy(app_shader_data.view_proj, &view_proj, sizeof(float) * 16);
	skg_buffer_set_contents(&app_shader_data_buffer, &app_shader_data,         sizeof(app_shader_data));
	skg_buffer_bind        (&app_shader_data_buffer, app_sh_default_data_bind, 0);

	
	app_test_colors();
	app_test_instancing();
	app_test_cubemap();
	app_test_dyn_update(t);
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