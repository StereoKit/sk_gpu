#pragma once

#include <stdint.h>

#include "imgui/imgui.h"
#include "../sk_gpu.h"

#include "app_shader.h"

#define HANDMADE_MATH_IMPLEMENTATION
#include "HandmadeMath.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

skg_tex_t    surface    = {};
skg_tex_t    zbuffer    = {};
skg_buffer_t cube_verts = {};
skg_buffer_t cube_inds  = {};
skg_mesh_t   cube_mesh  = {};
skg_tex_t    cube_tex   = {};

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
	skg_vert_t verts[24];
	uint32_t   inds [36];
	for (size_t i = 0; i < 24; i++) {
		float neg = (float)((i / 4) % 2 ? -1 : 1);
		int nx  = ((i+24) / 16) % 2;
		int ny  = (i / 8)       % 2;
		int nz  = (i / 16)      % 2;
		int u   = ((i+1) / 2)   % 2; // U: 0,1,1,0
		int v   = (i / 2)       % 2; // V: 0,0,1,1

		verts[i].uv[0] = u;
		verts[i].uv[1] = v;
		verts[i].norm[0] = nx * neg;
		verts[i].norm[1] = ny * neg;
		verts[i].norm[2] = nz * neg;
		verts[i].pos[0] = (nx ? neg : ny ? (u?-1:1)*neg : (u?1:-1)*neg);
		verts[i].pos[1] = (nx || nz ? (v?1:-1) : neg);
		verts[i].pos[2] = (nx ? (u?-1:1)*neg : ny ? (v?1:-1) : neg);
		verts[i].col[0] = 255;
		verts[i].col[1] = 255;
		verts[i].col[2] = 255;
		verts[i].col[3] = 255;
	}
	for (size_t i = 0; i < 6; i++) {
		inds[i*6+0] = i*4;
		inds[i*6+1] = i*4+1;
		inds[i*6+2] = i*4+2;

		inds[i*6+3] = i*4;
		inds[i*6+4] = i*4+2;
		inds[i*6+5] = i*4+3;
	}

	cube_verts = skg_buffer_create(verts, sizeof(verts)/sizeof(skg_vert_t), sizeof(skg_vert_t), skg_buffer_type_vertex, skg_use_static);
	cube_inds  = skg_buffer_create(inds,  sizeof(inds )/sizeof(uint32_t  ), sizeof(uint32_t),   skg_buffer_type_index,  skg_use_static);
	cube_mesh  = skg_mesh_create  (&cube_verts, &cube_inds);

	int32_t width, height, comp;
	void *img_data = stbi_load("test.png", &width, &height, &comp, 4);
	cube_tex = skg_tex_create(skg_tex_type_image, skg_use_static, skg_tex_fmt_rgba32, skg_mip_generate);
	void *frames[] = { img_data };
	skg_tex_set_contents(&cube_tex, frames, 1, width, height);
}

void window_preview_render() {
	static float time = 0;
	time += 0.016f;

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

	float clear[4] = { 0,0,0,0 };
	skg_tex_target_bind(&surface, true, clear);

	skg_pipeline_t *pipeline = app_shader_get_pipeline();
	if (pipeline) {
		skg_pipeline_bind(pipeline);
		skg_mesh_bind    (&cube_mesh);
		skg_tex_bind     (&cube_tex, skg_bind_t{0, skg_stage_pixel});
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