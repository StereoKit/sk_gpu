#include "app.h"

#include "../src/sk_gpu_dev.h"
#include "HandmadeMath.h"
#include "shaders.h"

#include <stdlib.h>
#include <string.h>

///////////////////////////////////////////

struct app_mesh_t {
	skr_buffer_t vert_buffer;
	skr_buffer_t ind_buffer;
	skr_mesh_t   mesh;
	int32_t      ind_count;
};

struct app_shader_data_t {
	float view_proj[16];
};

struct app_shader_inst_t {
	float world[16];
};

///////////////////////////////////////////

app_shader_data_t    app_shader_data = {};
app_shader_inst_t    app_shader_inst[100] = {};
app_mesh_t           app_mesh1  = {};
app_mesh_t           app_mesh2  = {};
skr_shader_stage_t   app_ps     = {};
skr_shader_stage_t   app_vs     = {};
skr_pipeline_t       app_shader = {};
skr_shader_stage_t   app_cube_ps     = {};
skr_shader_stage_t   app_cube_vs     = {};
skr_pipeline_t       app_cube_shader = {};
skr_buffer_t         app_shader_data_buffer = {};
skr_buffer_t         app_shader_inst_buffer = {};
skr_tex_t            app_tex    = {};
skr_tex_t            app_target = {};
skr_tex_t            app_target_depth = {};
skr_tex_t            app_cubemap = {};

///////////////////////////////////////////

app_mesh_t app_mesh_create(const skr_vert_t *verts, int32_t vert_count, const uint32_t *inds, int32_t ind_count);
void       app_mesh_destroy(app_mesh_t *mesh);

///////////////////////////////////////////

bool app_init() {

	// Make a cube
	skr_vert_t verts[] = {
		skr_vert_t{ {-1,-1,-1}, {-1,-1,-1}, {0.00f,0}, {255,255,255,255}}, // Bottom verts
		skr_vert_t{ { 1,-1,-1}, { 1,-1,-1}, {0.50f,0}, {255,255,255,255}},
		skr_vert_t{ { 1, 1,-1}, { 1, 1,-1}, {1.00f,0}, {255,255,255,255}},
		skr_vert_t{ {-1, 1,-1}, {-1, 1,-1}, {0.50f,0}, {255,255,255,255}},
		skr_vert_t{ {-1,-1, 1}, {-1,-1, 1}, {0.00f,1}, {255,255,255,255}}, // Top verts
		skr_vert_t{ { 1,-1, 1}, { 1,-1, 1}, {0.50f,1}, {255,255,255,255}},
		skr_vert_t{ { 1, 1, 1}, { 1, 1, 1}, {1.00f,1}, {255,255,255,255}},
		skr_vert_t{ {-1, 1, 1}, {-1, 1, 1}, {0.50f,1}, {255,255,255,255}}, };
	uint32_t inds[] = {
		0,2,1, 0,3,2, 5,6,4, 4,6,7,
		1,2,6, 1,6,5, 4,7,3, 4,3,0,
		1,5,4, 1,4,0, 3,7,2, 7,6,2, };
	app_mesh1 = app_mesh_create(verts, sizeof(verts)/sizeof(skr_vert_t), inds, sizeof(inds)/sizeof(uint32_t));

	// Make a pyramid
	skr_vert_t verts2[] = {
		skr_vert_t{ { 0, 1, 0}, { 0, 1, 0}, {0.00f,1}, {255,255,255,255}},
		skr_vert_t{ {-1,-1,-1}, {-1,-1,-1}, {0.00f,0}, {0,255,0,255}},
		skr_vert_t{ { 1,-1,-1}, { 1,-1,-1}, {0.25f,0}, {0,0,255,255}},
		skr_vert_t{ { 1,-1, 1}, {-1,-1, 1}, {0.50f,0}, {255,255,0,255}},
		skr_vert_t{ {-1,-1, 1}, { 1,-1, 1}, {0.75f,0}, {255,0,255,255}},};
	uint32_t inds2[] = {
		2,1,0, 3,2,0, 4,3,0, 1,4,0, 1,2,3, 1,3,4 };
	app_mesh2 = app_mesh_create(verts2, sizeof(verts2)/sizeof(skr_vert_t), inds2, sizeof(inds2)/sizeof(uint32_t));

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
	app_tex = skr_tex_create(skr_tex_type_image, skr_use_static, skr_tex_fmt_rgba32, skr_mip_generate);
	skr_tex_settings(&app_tex, skr_tex_address_repeat, skr_tex_sample_linear, 0);
	skr_tex_set_data(&app_tex, color_arr, 1, w, h);

	app_target       = skr_tex_create(skr_tex_type_rendertarget, skr_use_static, skr_tex_fmt_rgba32,  skr_mip_none);
	app_target_depth = skr_tex_create(skr_tex_type_depth,        skr_use_static, skr_tex_fmt_depth16, skr_mip_none);
	skr_tex_set_data (&app_target,       nullptr, 1, 512, 512);
	skr_tex_set_data (&app_target_depth, nullptr, 1, 512, 512);
	skr_tex_set_depth(&app_target, &app_target_depth);

	app_cubemap = skr_tex_create(skr_tex_type_cubemap, skr_use_static, skr_tex_fmt_rgba32, skr_mip_none);
	uint8_t *cube_cols[6];
	for (size_t f = 0; f < 6; f++) {
		cube_cols[f] = (uint8_t*)malloc(sizeof(uint8_t) * 4 * 4);
		size_t offset = f * 4 * 4;
		for (size_t p = 0; p < 4; p++) {
			cube_cols[f][p*4 + 0] = (f/2) % 3 == 0 ? (f%2==0?128:255) : 0;
			cube_cols[f][p*4 + 1] = (f/2) % 3 == 1 ? (f%2==0?128:255) : 0;
			cube_cols[f][p*4 + 2] = (f/2) % 3 == 2 ? (f%2==0?128:255) : 0;
			cube_cols[f][p*4 + 3] = 255;
		}
	}
	skr_tex_set_data(&app_cubemap, (void**)&cube_cols, 6, 2, 2);

#if defined(SKR_OPENGL)
	app_cube_ps = skr_shader_stage_create((uint8_t*)shader_cube_glsl_ps, strlen(shader_cube_glsl_ps), skr_shader_pixel);
	app_cube_vs = skr_shader_stage_create((uint8_t*)shader_cube_glsl_vs, strlen(shader_cube_glsl_vs), skr_shader_vertex);
#elif defined(SKR_DIRECT3D11)
	app_cube_ps = skr_shader_stage_create((uint8_t*)shader_cube_hlsl, strlen(shader_cube_hlsl), skr_shader_pixel);
	app_cube_vs = skr_shader_stage_create((uint8_t*)shader_cube_hlsl, strlen(shader_cube_hlsl), skr_shader_vertex);
#endif
	app_cube_shader = skr_pipeline_create(skr_transparency_none, skr_cull_back, true);
	skr_pipeline_set_shader(&app_cube_shader, &app_cube_vs);
	skr_pipeline_set_shader(&app_cube_shader, &app_cube_ps);

#if defined(SKR_OPENGL)
	app_ps = skr_shader_stage_create((uint8_t*)shader_glsl_ps, strlen(shader_glsl_ps), skr_shader_pixel);
	app_vs = skr_shader_stage_create((uint8_t*)shader_glsl_vs, strlen(shader_glsl_vs), skr_shader_vertex);
#elif defined(SKR_DIRECT3D11)
	app_ps = skr_shader_stage_create((uint8_t*)shader_hlsl, strlen(shader_hlsl), skr_shader_pixel);
	app_vs = skr_shader_stage_create((uint8_t*)shader_hlsl, strlen(shader_hlsl), skr_shader_vertex);
#elif defined(SKR_VULKAN)
	app_ps = skr_shader_stage_create(shader_spirv_ps, _countof(shader_spirv_ps), skr_shader_pixel);
	app_vs = skr_shader_stage_create(shader_spirv_vs, _countof(shader_spirv_vs), skr_shader_vertex);
#endif
	app_shader = skr_pipeline_create(skr_transparency_none, skr_cull_back, false);
	skr_pipeline_set_shader(&app_shader, &app_vs);
	skr_pipeline_set_shader(&app_shader, &app_ps);

	app_shader_data_buffer = skr_buffer_create(&app_shader_data, sizeof(app_shader_data_t),       skr_buffer_type_constant, skr_use_dynamic);
	app_shader_inst_buffer = skr_buffer_create(&app_shader_inst, sizeof(app_shader_inst_t) * 100, skr_buffer_type_constant, skr_use_dynamic);
	return true;
}

///////////////////////////////////////////

void app_test_cubemap() {
	hmm_mat4 world = HMM_Transpose(HMM_Translate(hmm_vec3{ 0,0,0 }) * HMM_Scale(hmm_vec3{ .4f,.4f,.4f }));
	memcpy(&app_shader_inst[0].world, &world, sizeof(float) * 16);
	skr_buffer_update(&app_shader_inst_buffer, &app_shader_inst, sizeof(app_shader_inst_t));
	skr_buffer_set   (&app_shader_inst_buffer, 1, sizeof(app_shader_inst_t), 0);

	skr_mesh_set      (&app_mesh1.mesh);
	skr_pipeline_set  (&app_cube_shader);
	skr_tex_set_active(&app_tex, 0);
	skr_tex_set_active(&app_cubemap, 9);
	skr_draw(0, app_mesh1.ind_count, 1);
	skr_tex_set_active(nullptr, 1);
}

///////////////////////////////////////////

void app_test_rendertarget() {
	skr_tex_t *old_target = skr_get_render_target();

	float color[4] = { 1,1,1,1 };
	skr_set_render_target(color, true, &app_target);

	hmm_mat4 view = HMM_LookAt(
		HMM_Vec3(1,1,1),
		HMM_Vec3(0,0,0),
		HMM_Vec3(0,1,0));
	hmm_mat4 proj = HMM_Perspective(45, 1, 0.01f, 100);
	hmm_mat4 view_proj = HMM_Transpose( proj * view );
	memcpy(app_shader_data.view_proj, &view_proj, sizeof(float) * 16);
	skr_buffer_update(&app_shader_data_buffer, &app_shader_data, sizeof(app_shader_data));
	skr_buffer_set   (&app_shader_data_buffer, 0, sizeof(app_shader_data_t), 0);

	static int frame = 0;
	frame++;
	hmm_mat4 world = HMM_Transpose(HMM_Translate(hmm_vec3{ 0,0,0 }) * HMM_Scale(hmm_vec3{ .4f,.4f,.4f }) * HMM_Rotate(frame*0.8f, hmm_vec3{1,0,0}));
	memcpy(&app_shader_inst[0].world, &world, sizeof(float) * 16);
	skr_buffer_update(&app_shader_inst_buffer, &app_shader_inst, sizeof(app_shader_inst_t));
	skr_buffer_set   (&app_shader_inst_buffer, 1, sizeof(app_shader_inst_t), 0);

	skr_mesh_set      (&app_mesh1.mesh);
	skr_pipeline_set  (&app_shader);
	skr_tex_set_active(&app_tex, 0);
	skr_draw(0, app_mesh1.ind_count, 1);

	skr_set_render_target(color, false, old_target);
}

///////////////////////////////////////////

void app_test_instancing() {
	// Set transforms for 100 instances
	for (size_t i = 0; i < 100; i++) {
		int y = i / 10 - 4, x = i % 10 -4;
		hmm_mat4 world = HMM_Transpose(HMM_Translate(hmm_vec3{ (float)x -0.5f,0,(float)y -0.5f })*HMM_Scale(hmm_vec3{.2f,.2f,.2f}));
		memcpy(&app_shader_inst[i].world, &world, sizeof(float) * 16);
	}
	skr_buffer_update(&app_shader_inst_buffer, &app_shader_inst, sizeof(app_shader_inst));
	skr_buffer_set   (&app_shader_inst_buffer, 1, sizeof(app_shader_inst_t), 0);

	skr_mesh_set      (&app_mesh1.mesh);
	skr_pipeline_set  (&app_shader);
	skr_tex_set_active(&app_tex, 0);
	skr_draw(0, app_mesh1.ind_count, 100);

	// Set transforms for another 100 instances
	for (size_t i = 0; i < 100; i++) {
		int y = i / 10 - 4, x = i % 10 -4;
		hmm_mat4 world = HMM_Transpose(HMM_Translate(hmm_vec3{ (float)x -0.5f,1,(float)y-0.5f }) * HMM_Scale(hmm_vec3{.2f,.2f,.2f}));
		memcpy(&app_shader_inst[i].world, &world, sizeof(float) * 16);
	}
	skr_buffer_update(&app_shader_inst_buffer, &app_shader_inst, sizeof(app_shader_inst));
	skr_buffer_set   (&app_shader_inst_buffer, 1, sizeof(app_shader_inst_t), 0);

	skr_mesh_set      (&app_mesh2.mesh);
	skr_pipeline_set  (&app_shader);
	skr_tex_set_active(&app_target, 0);
	skr_draw(0, app_mesh2.ind_count, 100);
}

///////////////////////////////////////////

void app_render(hmm_mat4 view, hmm_mat4 proj) {
	
	app_test_rendertarget();

	hmm_mat4 view_proj = HMM_Transpose( proj * view );
	memcpy(app_shader_data.view_proj, &view_proj, sizeof(float) * 16);
	skr_buffer_update(&app_shader_data_buffer, &app_shader_data, sizeof(app_shader_data));
	skr_buffer_set(&app_shader_data_buffer, 0, sizeof(app_shader_data_t), 0);

	app_test_instancing();
	app_test_cubemap();
}

///////////////////////////////////////////

void app_shutdown() {
	skr_buffer_destroy(&app_shader_data_buffer);
	skr_buffer_destroy(&app_shader_inst_buffer);
	skr_pipeline_destroy(&app_shader);
	skr_pipeline_destroy(&app_cube_shader);
	skr_shader_stage_destroy(&app_ps);
	skr_shader_stage_destroy(&app_vs);
	skr_tex_destroy(&app_target_depth);
	skr_tex_destroy(&app_target);
	skr_tex_destroy(&app_tex);
	app_mesh_destroy(&app_mesh1);
	app_mesh_destroy(&app_mesh2);
}

///////////////////////////////////////////

app_mesh_t app_mesh_create(const skr_vert_t *verts, int32_t vert_count, const uint32_t *inds, int32_t ind_count) {
	app_mesh_t result = {};
	result.vert_buffer = skr_buffer_create(verts, vert_count * sizeof(skr_vert_t), skr_buffer_type_vertex, skr_use_static);
	result.ind_buffer  = skr_buffer_create(inds,  ind_count  * sizeof(uint32_t),   skr_buffer_type_index,  skr_use_static);
	result.mesh        = skr_mesh_create(&result.vert_buffer, &result.ind_buffer);
	result.ind_count   = ind_count;
	return result;
}

///////////////////////////////////////////

void app_mesh_destroy(app_mesh_t *mesh) {
	skr_mesh_destroy  (&mesh->mesh);
	skr_buffer_destroy(&mesh->vert_buffer);
	skr_buffer_destroy(&mesh->ind_buffer);
	*mesh = {};
}

///////////////////////////////////////////