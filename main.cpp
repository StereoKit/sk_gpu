#ifndef __EMSCRIPTEN__
#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>

HWND app_hwnd;
#else
#define _countof(a) (sizeof(a)/sizeof(*(a)))
#endif

#include "sk_gpu.h"
#include "shaders.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifndef __EMSCRIPTEN__
#define HANDMADE_MATH_NO_SSE
#endif
#define HANDMADE_MATH_IMPLEMENTATION
#include "HandmadeMath.h"

///////////////////////////////////////////

struct app_mesh_t {
	skr_buffer_t vert_buffer;
	skr_buffer_t ind_buffer;
	skr_mesh_t mesh;
	int32_t ind_count;
};

struct app_shader_data_t {
	float world[16];
	float view_proj[16];
};

app_mesh_t app_mesh_create(const skr_vert_t *verts, int32_t vert_count, const uint32_t *inds, int32_t ind_count);
void       app_mesh_destroy(app_mesh_t *mesh);

///////////////////////////////////////////

app_shader_data_t    app_shader_data = {};
app_mesh_t           app_mesh1 = {};
app_mesh_t           app_mesh2 = {};
skr_shader_t         app_ps    = {};
skr_shader_t         app_vs    = {};
skr_shader_program_t app_shader = {};
skr_buffer_t         app_shader_buffer = {};
skr_tex_t            app_tex = {};
skr_swapchain_t      app_swapchain = {};

int         app_width  = 1280;
int         app_height = 720;
bool        app_run    = true;
const char *app_name   = "sk_gpu.h";

///////////////////////////////////////////

bool app_init();
void app_shutdown();
bool app_step();

uint8_t *load_file(const char *filename);

///////////////////////////////////////////

int main()
{
	if (!app_init())
		return -1;

	while (app_run && app_step());

	app_shutdown();
	return 1;
}

///////////////////////////////////////////

bool app_init() {
#ifdef __EMSCRIPTEN__
	if (!skr_init(app_name, nullptr, nullptr, app_width, app_height)) return false;
#else
	WNDCLASS wc = {}; 
	wc.lpfnWndProc = [](HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
		switch(message) {
		case WM_CLOSE: app_run = false; PostQuitMessage(0); break;
		case WM_SIZE: {
			app_width  = (UINT)LOWORD(lParam);
			app_height = (UINT)HIWORD(lParam);
			skr_swapchain_resize(&app_swapchain, app_width, app_height);
		} return DefWindowProc(hWnd, message, wParam, lParam);
		default: return DefWindowProc(hWnd, message, wParam, lParam);
		}
		return (LRESULT)0;
	};
	wc.hInstance     = GetModuleHandle(nullptr);
	wc.hbrBackground = (HBRUSH)(COLOR_BACKGROUND);
	wc.lpszClassName = app_name;
	if( !RegisterClass(&wc) ) return 1;
	app_hwnd = CreateWindow(
		wc.lpszClassName, app_name, 
		WS_OVERLAPPEDWINDOW | WS_VISIBLE, 
		0, 0, app_width, app_height, 
		nullptr, nullptr, wc.hInstance, nullptr);

	if( !app_hwnd ) return false;
	if (!skr_init(app_name, nullptr)) return false;
	app_swapchain = skr_swapchain_create(app_hwnd, skr_tex_fmt_rgba32_linear, skr_tex_fmt_depth32, app_width, app_height);
#endif

	

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
	app_mesh1 = app_mesh_create(verts, _countof(verts), inds, _countof(inds));

	// Make a pyramid
	skr_vert_t verts2[] = {
		skr_vert_t{ { 0, 1, 0}, { 0, 1, 0}, {0.00f,1}, {255,0,0,255}},
		skr_vert_t{ {-1,-1,-1}, {-1,-1,-1}, {0.00f,0}, {0,255,0,255}},
		skr_vert_t{ { 1,-1,-1}, { 1,-1,-1}, {0.25f,0}, {0,0,255,255}},
		skr_vert_t{ { 1,-1, 1}, {-1,-1, 1}, {0.50f,0}, {255,255,0,255}},
		skr_vert_t{ {-1,-1, 1}, { 1,-1, 1}, {0.75f,0}, {255,0,255,255}},};
	uint32_t inds2[] = {
		2,1,0, 3,2,0, 4,3,0, 1,4,0, 1,2,3, 1,3,4 };
	app_mesh2 = app_mesh_create(verts2, _countof(verts2), inds2, _countof(inds2));

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
	app_tex = skr_tex_create(skr_tex_type_image, skr_use_static, skr_tex_fmt_rgba32, skr_mip_none);
	skr_tex_settings(&app_tex, skr_tex_address_repeat, skr_tex_sample_linear, 0);
	skr_tex_set_data(&app_tex, color_arr, 1, w, h);

#ifdef SKR_OPENGL
	app_ps = skr_shader_create(shader_glsl_ps, skr_shader_pixel);
	app_vs = skr_shader_create(shader_glsl_vs, skr_shader_vertex);
#else
	app_ps = skr_shader_create(shader_hlsl, skr_shader_pixel);
	app_vs = skr_shader_create(shader_hlsl, skr_shader_vertex);
#endif
	app_shader = skr_shader_program_create(&app_vs, &app_ps);

	app_shader_buffer = skr_buffer_create(&app_shader_data, sizeof(app_shader_data_t), skr_buffer_type_constant, skr_use_dynamic);
	return true;
}

///////////////////////////////////////////

void app_shutdown() {
	skr_buffer_destroy(&app_shader_buffer);
	skr_shader_program_destroy(&app_shader);
	skr_shader_destroy(&app_ps);
	skr_shader_destroy(&app_vs);
	skr_tex_destroy(&app_tex);
	app_mesh_destroy(&app_mesh1);
	app_mesh_destroy(&app_mesh2);
	skr_shutdown();
}

///////////////////////////////////////////

bool app_step() {
#ifndef __EMSCRIPTEN__
	MSG msg = {};
	if (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
#endif

	skr_draw_begin();
	float clear_color[4] = { 0,0,0,1 };
	skr_set_render_target(clear_color,
		skr_swapchain_get_target(&app_swapchain),
		skr_swapchain_get_depth (&app_swapchain));

	static int32_t frame = 0;
	frame++;

	hmm_mat4 view = HMM_LookAt(
		HMM_Vec3(sinf(frame / 30.0f) * 5, 3, cosf(frame / 30.0f) * 5),
		HMM_Vec3(0, 0, 0),
		HMM_Vec3(0, 1, 0));
	hmm_mat4 proj = HMM_Perspective(90, app_width / (float)app_height, 0.01f, 1000);
	hmm_mat4 view_proj = HMM_Transpose( proj * view );

	hmm_mat4 world = HMM_Translate({ -1.5f,0,0 });
	world = HMM_Transpose(world);
	memcpy(app_shader_data.world,     &world,     sizeof(float) * 16);
	memcpy(app_shader_data.view_proj, &view_proj, sizeof(float) * 16);
	skr_buffer_update(&app_shader_buffer, &app_shader_data, sizeof(app_shader_data));
	
	skr_buffer_set(&app_shader_buffer, 0, sizeof(app_shader_data_t), 0);
	skr_mesh_set(&app_mesh1.mesh);
	skr_shader_program_set(&app_shader);
	skr_tex_set_active(&app_tex, 0);
	skr_draw(0, app_mesh1.ind_count, 1);

	world = HMM_Translate({ 1.5f,0,0 }) * HMM_Rotate(frame, { 1,0,0 });
	world = HMM_Transpose(world);
	memcpy(app_shader_data.world, &world, sizeof(float) * 16);
	skr_buffer_update(&app_shader_buffer, &app_shader_data, sizeof(app_shader_data));
	skr_mesh_set(&app_mesh2.mesh);
	skr_shader_program_set(&app_shader);
	skr_draw(0, app_mesh2.ind_count, 1);

	skr_swapchain_present(&app_swapchain);
	return true;
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