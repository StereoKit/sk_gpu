#pragma once

#include <stdint.h>

#include "imgui/imgui.h"
#include "../sk_gpu.h"

#include "app_shader.h"
#include "geo.h"

#define HANDMADE_MATH_IMPLEMENTATION
#include "HandmadeMath.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

float camera_arc_x = 3.14159f/4.0f;
float camera_arc_y = 3.14159f/8.0f;
float camera_arc_dist = 8;

struct preview_mesh_t {
	const char  *name;
	skg_buffer_t verts;
	skg_buffer_t inds;
	skg_mesh_t   mesh;
	int32_t      faces;
};

preview_mesh_t preview_meshes[] = { {"Cube"}, {"Sphere"} };
int32_t        preview_mesh_active = 1;

skg_tex_t    surface    = {};
skg_tex_t    zbuffer    = {};
skg_tex_t    cube_tex   = {};

void window_preview_resize(int32_t width, int32_t height) {
	if (width != surface.width || height != surface.height) {
		if (skg_tex_is_valid(&surface)) skg_tex_destroy(&surface);
		if (skg_tex_is_valid(&zbuffer)) skg_tex_destroy(&zbuffer);
		surface = skg_tex_create(skg_tex_type_rendertarget, skg_use_static, skg_tex_fmt_rgba32, skg_mip_none);
		zbuffer = skg_tex_create(skg_tex_type_depth, skg_use_static, skg_tex_fmt_depth32, skg_mip_none);
		skg_tex_set_contents(&surface, nullptr, width, height);
		skg_tex_set_contents(&zbuffer, nullptr, width, height);
		skg_tex_attach_depth(&surface, &zbuffer);
	}
}

void window_preview_init() {

	preview_mesh_t *mesh = &preview_meshes[0];
	gen_cube(&mesh->mesh, &mesh->verts, &mesh->inds, &mesh->faces);
	mesh = &preview_meshes[1];
	gen_sphere(2, 8, &mesh->mesh, &mesh->verts, &mesh->inds, &mesh->faces);

	int32_t width, height, comp;
	void *img_data = stbi_load("test.png", &width, &height, &comp, 4);
	cube_tex = skg_tex_create(skg_tex_type_image, skg_use_static, skg_tex_fmt_rgba32, skg_mip_generate);
	skg_tex_set_contents(&cube_tex, img_data, width, height);
}

void window_preview_render() {
	static float time = 0;
	time += 0.016f;

	float c = cosf(camera_arc_y);
	hmm_mat4 view = HMM_LookAt(
		HMM_Vec3(
			sinf(camera_arc_x) * c * camera_arc_dist,
			sinf(camera_arc_y) *     camera_arc_dist,
			cosf(camera_arc_x) * c * camera_arc_dist),
		HMM_Vec3(0, 0, 0),
		HMM_Vec3(0, 1, 0));
	hmm_mat4 proj     = HMM_Perspective(45, surface.width / (float)surface.height, 0.01f, 1000);
	hmm_mat4 viewproj = HMM_Transpose(proj * view);
	view = HMM_Transpose(view);
	proj = HMM_Transpose(proj);
	app_shader_set_engine_val(engine_val_time, &time);
	app_shader_set_engine_val(engine_val_matrix_view, &view);
	app_shader_set_engine_val(engine_val_matrix_projection, &proj);
	app_shader_set_engine_val(engine_val_matrix_view_projection, &viewproj);

	float clear[4] = { 0,0,0,0 };
	skg_tex_target_bind(&surface, -1, 0);
	skg_target_clear(true, clear);

	skg_pipeline_t *pipeline = app_shader_get_pipeline();
	if (pipeline) {
		skg_pipeline_bind(pipeline);
		skg_mesh_bind    (&preview_meshes[preview_mesh_active].mesh);
		skg_tex_bind     (&cube_tex, skg_bind_t{0, skg_stage_pixel, skg_register_resource});
		skg_draw         (0, 0, preview_meshes[preview_mesh_active].faces, 1);
	}

	skg_tex_target_bind(nullptr, -1, 0);
}

void window_preview() {
	static bool initialized = false;
	if (!initialized) {
		initialized = true;
		window_preview_init();
	}

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0,0 });
	ImGui::Begin("Preview");

	/*if (ImGui::BeginMenuBar()) {
		if (ImGui::BeginMenu("Mesh")) {
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}*/

	camera_arc_dist -= ImGui::GetIO().MouseWheel * .2f;
	if (ImGui::IsMouseDragging(ImGuiMouseButton_Left) && ImGui::IsWindowHovered()) {
		ImVec2 delta = ImGui::GetMouseDragDelta();
		ImGui::ResetMouseDragDelta();
		camera_arc_x -= delta.x * 0.005f;
		camera_arc_y += delta.y * 0.005f;
		if (camera_arc_y < -3.14159f/2.0f)
			camera_arc_y = -3.14159f/2.0f;
		if (camera_arc_y >  3.14159f/2.0f)
			camera_arc_y =  3.14159f/2.0f;
	}

	ImVec2 min  = ImGui::GetWindowContentRegionMin();
	ImVec2 max  = ImGui::GetWindowContentRegionMax();
	ImVec2 size = {max.x-min.x, max.y-min.y};
	ImGui::SetCursorPos(min);
	if (ImGui::IsRectVisible(size)) {
		window_preview_resize((int32_t)size.x, (int32_t)size.y);
		window_preview_render();
		ImGui::Image((ImTextureID)&surface, size);
	}
	
	ImGui::End();
	ImGui::PopStyleVar();
}