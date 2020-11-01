#pragma once

#include <stdint.h>

#include "imgui/imgui.h"
#include "../sk_gpu.h"

#include "app_shader.h"

#define HANDMADE_MATH_IMPLEMENTATION
#include "HandmadeMath.h"

skg_tex_t    surface    = {};
skg_tex_t    zbuffer    = {};
skg_buffer_t cube_verts = {};
skg_buffer_t cube_inds  = {};
skg_mesh_t   cube_mesh  = {};

void window_preview_resize(int32_t width, int32_t height) {
	if (width != surface.width || height != surface.height) {
		skg_tex_destroy(&surface);
		skg_tex_destroy(&zbuffer);
		surface = skg_tex_create(skg_tex_type_rendertarget, skg_use_static, skg_tex_fmt_rgba32, skg_mip_none);
		zbuffer = skg_tex_create(skg_tex_type_depth, skg_use_static, skg_tex_fmt_depth32, skg_mip_none);
		skg_tex_set_contents(&surface, nullptr, 1, width, height);
		skg_tex_set_contents(&zbuffer, nullptr, 1, width, height);
		skg_tex_attach_depth(&surface, &zbuffer);
	}
}

void window_preview_init() {
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

	cube_verts = skg_buffer_create(verts, sizeof(verts)/sizeof(skg_vert_t), sizeof(skg_vert_t), skg_buffer_type_vertex, skg_use_static);
	cube_inds  = skg_buffer_create(inds,  sizeof(inds )/sizeof(uint32_t  ), sizeof(uint32_t),   skg_buffer_type_index,  skg_use_static);
	cube_mesh  = skg_mesh_create(&cube_verts, &cube_inds);
}

void window_preview_render() {


	static float time = 0;
	time += 0.016f;
	float clear[4] = { 0,0,0,0 };
	skg_tex_target_bind(&surface, true, clear);

	hmm_mat4 view = HMM_LookAt(
		HMM_Vec3(sinf(time) * 3, 2, cosf(time) * 3),
		HMM_Vec3(0, 0, 0),
		HMM_Vec3(0, 1, 0));
	hmm_mat4 proj     = HMM_Perspective(90, surface.width / (float)surface.height, 0.01f, 1000);
	hmm_mat4 viewproj = HMM_Transpose(proj * view);
	view = HMM_Transpose(view);
	proj = HMM_Transpose(proj);
	app_shader_set_named_val("time", &time);
	app_shader_set_named_val("view", &view);
	app_shader_set_named_val("proj", &proj);
	app_shader_set_named_val("viewproj", &viewproj);

	skg_pipeline_t *pipeline = app_shader_get_pipeline();
	if (pipeline) {
		skg_pipeline_bind(pipeline);
		skg_mesh_bind    (&cube_mesh);
		skg_draw         (0, 0, 36, 1);
	}

	skg_tex_target_bind(nullptr, false, nullptr);
}

void window_preview() {
	static bool initialized = false;
	if (!initialized) {
		initialized = true;
		window_preview_init();
	}

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0,0 });
	ImGui::Begin("Preview");

	ImVec2 min  = ImGui::GetWindowContentRegionMin();
	ImVec2 max  = ImGui::GetWindowContentRegionMax();
	ImVec2 size = {max.x - min.x, max.y-min.y};
	window_preview_resize(size.x, size.y);
	window_preview_render();
	ImGui::SetCursorPos(min);
	ImGui::Image((ImTextureID)&surface, size);
	
	ImGui::End();
	ImGui::PopStyleVar();
}