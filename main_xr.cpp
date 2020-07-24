// When using single file header like normal, do this
//#define SKR_OPENGL
//#define SKR_IMPL
//#include "sk_gpu.h"

// For easier development
#include "src/sk_gpu_dev.h"

// Also see here for OpenXR GL reference:
// https://github.com/jherico/OpenXR-Samples/blob/master/src/examples/sdl2_gl_single_file_example.cpp

#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>

HWND app_hwnd;

#include "shaders.h"

#define XR_APP_IMPL
#include "xr_app.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <vector>
using namespace std;

#define HANDMADE_MATH_IMPLEMENTATION
#include "HandmadeMath.h"

///////////////////////////////////////////

struct app_mesh_t {
	skr_buffer_t vert_buffer;
	skr_buffer_t ind_buffer;
	skr_mesh_t   mesh;
	int32_t      ind_count;
};

struct app_shader_data_t {
	float world[16];
	float view_proj[16];
};

struct swapchain_surfdata_t {
	skr_tex_t depth_tex;
	skr_tex_t render_tex;
};

struct app_swapchain_t {
	int32_t view_count;
	int32_t surf_count;
	swapchain_surfdata_t *surfaces;
};


app_mesh_t app_mesh_create(const skr_vert_t *verts, int32_t vert_count, const uint32_t *inds, int32_t ind_count);
void       app_mesh_destroy(app_mesh_t *mesh);

///////////////////////////////////////////

app_shader_data_t    app_shader_data = {};
app_mesh_t           app_mesh1  = {};
app_mesh_t           app_mesh2  = {};
skr_shader_stage_t   app_ps     = {};
skr_shader_stage_t   app_vs     = {};
skr_shader_t         app_shader = {};
skr_buffer_t         app_shader_buffer = {};
skr_tex_t            app_tex = {};
app_swapchain_t      app_swapchain = {};

int         app_width  = 1280;
int         app_height = 720;
bool        app_run    = true;
const char *app_name   = "Cross Platform Renderer";

xr_callbacks_t xr_functions = {};

///////////////////////////////////////////

bool app_init();
void app_shutdown();
bool app_step();

void app_render(void *user_data, const XrCompositionLayerProjectionView *view, int32_t swapchain_view_id, int32_t swapchain_surface_id);
hmm_mat4 xr_projection(XrFovf fov, float clip_near, float clip_far);

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

bool app_init_gfx(void *user_data, const XrGraphicsRequirements *requirements, XrGraphicsBinding *out_graphics) {
	void *luid = nullptr;
#ifdef SKR_DIRECT3D11
	luid = (void *)&requirement.adapterLuid;
#endif
	skr_log_callback([](const char *text) { printf("%s\n", text); });
	if (!skr_init(app_name, nullptr, luid))
		return false;

#if defined(SKR_OPENGL)
	skr_platform_data_t platform = skr_get_platform_data();
	out_graphics->hDC   = (HDC  )platform.gl_hdc;
	out_graphics->hGLRC = (HGLRC)platform.gl_hrc;
#elif defined(SKR_DIRECT3D11)
	out_graphics->device = (ID3D11Device*)skr_get_platform_data().d3d11_device;
#endif

	return true;
}

///////////////////////////////////////////

bool app_init_swapchain(void *user_data, int32_t view_count, int32_t surface_count, void **textures, int32_t width, int32_t height, int64_t fmt) {
	app_swapchain.view_count = view_count;
	app_swapchain.surf_count = surface_count;
	app_swapchain.surfaces   = (swapchain_surfdata_t*)malloc(sizeof(swapchain_surfdata_t) * view_count * surface_count);
	skr_tex_fmt_ skr_format = skr_tex_fmt_from_native(fmt);

	for (int32_t i = 0; i < view_count*surface_count; i++) {
		app_swapchain.surfaces[i].render_tex = skr_tex_from_native(textures[i], skr_tex_type_rendertarget, skr_format, width, height);
		app_swapchain.surfaces[i].depth_tex  = skr_tex_create(skr_tex_type_depth, skr_use_static, skr_tex_fmt_depth32, skr_mip_none);
		skr_tex_set_data(&app_swapchain.surfaces[i].depth_tex, nullptr, 1, width, height);
	}
	return true;
}

///////////////////////////////////////////

void app_destroy_swapchain(void *user_data) {
	for (int32_t i = 0; i < app_swapchain.surf_count * app_swapchain.view_count; i++) {
		skr_tex_destroy(&app_swapchain.surfaces[i].render_tex);
		skr_tex_destroy(&app_swapchain.surfaces[i].depth_tex);
	}
	free(app_swapchain.surfaces);
	app_swapchain = {};
}

///////////////////////////////////////////

bool app_init() {
	WNDCLASS wc = {}; 
	wc.lpfnWndProc = [](HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
		switch(message) {
		case WM_CLOSE: app_run = false; PostQuitMessage(0); break;
		case WM_SIZE: {
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

	xr_functions.init_gfx          = app_init_gfx;
	xr_functions.init_swapchain    = app_init_swapchain;
	xr_functions.destroy_swapchain = app_destroy_swapchain;
	xr_functions.draw              = app_render;
	xr_functions.pixel_formats      = (int64_t*)malloc(sizeof(int64_t) * 4);
	xr_functions.pixel_format_count = 4;
	xr_functions.depth_formats      = (int64_t*)malloc(sizeof(int64_t) * 3);
	xr_functions.depth_format_count = 3;
	xr_functions.pixel_formats[0] = skr_tex_fmt_to_native(skr_tex_fmt_rgba32_linear);
	xr_functions.pixel_formats[1] = skr_tex_fmt_to_native(skr_tex_fmt_bgra32_linear);
	xr_functions.pixel_formats[2] = skr_tex_fmt_to_native(skr_tex_fmt_rgba32);
	xr_functions.pixel_formats[3] = skr_tex_fmt_to_native(skr_tex_fmt_bgra32);
	xr_functions.depth_formats[0] = skr_tex_fmt_to_native(skr_tex_fmt_depth16);
	xr_functions.depth_formats[1] = skr_tex_fmt_to_native(skr_tex_fmt_depth32);
	xr_functions.depth_formats[2] = skr_tex_fmt_to_native(skr_tex_fmt_depthstencil);
	if (!openxr_init(app_name, &xr_functions, app_hwnd)) return false;

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
	app_shader = skr_shader_create(&app_vs, &app_ps);

	app_shader_buffer = skr_buffer_create(&app_shader_data, sizeof(app_shader_data_t), skr_buffer_type_constant, skr_use_dynamic);
	return true;
}

///////////////////////////////////////////

void app_shutdown() {
	skr_buffer_destroy(&app_shader_buffer);
	skr_shader_destroy(&app_shader);
	skr_shader_stage_destroy(&app_ps);
	skr_shader_stage_destroy(&app_vs);
	skr_tex_destroy(&app_tex);
	app_mesh_destroy(&app_mesh1);
	app_mesh_destroy(&app_mesh2);
	skr_shutdown();
}

///////////////////////////////////////////

bool app_step() {
	bool quit = false;
	openxr_poll_events(quit);

	if (xr_running) {
		openxr_poll_actions();
		openxr_render_frame();
	}
	return true;

#ifndef __EMSCRIPTEN__
	MSG msg = {};
	if (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
#endif

	skr_draw_begin();

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
	skr_shader_set(&app_shader);
	skr_tex_set_active(&app_tex, 0);
	skr_draw(0, app_mesh1.ind_count, 1);

	world = HMM_Translate({ 1.5f,0,0 }) * HMM_Rotate(frame, { 1,0,0 });
	world = HMM_Transpose(world);
	memcpy(app_shader_data.world, &world, sizeof(float) * 16);
	skr_buffer_update(&app_shader_buffer, &app_shader_data, sizeof(app_shader_data));
	skr_mesh_set(&app_mesh2.mesh);
	skr_shader_set(&app_shader);
	skr_draw(0, app_mesh2.ind_count, 1);

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

void app_render(void *user_data, const XrCompositionLayerProjectionView *view, int32_t view_id, int32_t surf_id) {
	float clear_color[4] = { 0,0,0,1 };
	skr_tex_t *target = &app_swapchain.surfaces[view_id * app_swapchain.surf_count + surf_id].render_tex;
	skr_tex_t *depth  = &app_swapchain.surfaces[view_id * app_swapchain.surf_count + surf_id].depth_tex;
	skr_set_render_target(clear_color, target, depth);

	hmm_quaternion head_orientation;
	memcpy(&head_orientation, &view->pose.orientation, sizeof(XrQuaternionf));
	hmm_vec3 head_pos;
	memcpy(&head_pos, &view->pose.position, sizeof(XrVector3f));
	
	hmm_mat4 view_mat = 
		HMM_QuaternionToMat4(HMM_InverseQuaternion(head_orientation)) *
		HMM_Translate(HMM_Vec3( -view->pose.position.x, -view->pose.position.y, -view->pose.position.z));
	hmm_mat4 proj_mat  = xr_projection(view->fov, 0.01f, 100.0f);
	hmm_mat4 view_proj = HMM_Transpose( proj_mat * view_mat );

	hmm_mat4 world = HMM_Translate({ -1.5f,0,0 });
	world = HMM_Transpose(world);
	memcpy(app_shader_data.world,     &world,     sizeof(float) * 16);
	memcpy(app_shader_data.view_proj, &view_proj, sizeof(float) * 16);
	skr_buffer_update(&app_shader_buffer, &app_shader_data, sizeof(app_shader_data));

	skr_buffer_set(&app_shader_buffer, 0, sizeof(app_shader_data_t), 0);
	skr_mesh_set(&app_mesh1.mesh);
	skr_shader_set(&app_shader);
	skr_tex_set_active(&app_tex, 0);
	skr_draw(0, app_mesh1.ind_count, 1);

	world = HMM_Translate({ 1.5f,0,0 }) * HMM_Rotate(0, { 1,0,0 });
	world = HMM_Transpose(world);
	memcpy(app_shader_data.world, &world, sizeof(float) * 16);
	skr_buffer_update(&app_shader_buffer, &app_shader_data, sizeof(app_shader_data));
	skr_mesh_set(&app_mesh2.mesh);
	skr_shader_set(&app_shader);
	skr_draw(0, app_mesh2.ind_count, 1);
}

///////////////////////////////////////////

hmm_mat4 xr_projection_dx(XrFovf fov, float clip_near, float clip_far) {
	// Mix of XMMatrixPerspectiveFovRH from DirectXMath and XrMatrix4x4f_CreateProjectionFov from xr_linear.h
	const float tanLeft        = tanf(fov.angleLeft);
	const float tanRight       = tanf(fov.angleRight);
	const float tanDown        = tanf(fov.angleDown);
	const float tanUp          = tanf(fov.angleUp);
	const float tanAngleWidth  = tanRight - tanLeft;
	const float tanAngleHeight = tanUp - tanDown;
	const float range          = clip_far / (clip_near - clip_far);

	// [row][column]
	float arr[16] = { 0 };
	arr[0]  = 2 / tanAngleWidth;                    // [0][0] Different, DX uses: Width (Height / AspectRatio);
	arr[5]  = 2 / tanAngleHeight;                   // [1][1] Same as DX's: Height (CosFov / SinFov)
	arr[8]  = (tanRight + tanLeft) / tanAngleWidth; // [2][0] Only present in xr's
	arr[9]  = (tanUp + tanDown) / tanAngleHeight;   // [2][1] Only present in xr's
	arr[10] = range;                               // [2][2] Same as xr's: -(farZ + offsetZ) / (farZ - nearZ)
	arr[11] = -1;                                  // [2][3] Same
	arr[14] = range * clip_near;                   // [3][2] Same as xr's: -(farZ * (nearZ + offsetZ)) / (farZ - nearZ);

	hmm_mat4 result;
	memcpy(&result, arr, sizeof(hmm_mat4));
	return result;
}

hmm_mat4 xr_projection(XrFovf fov, float nearZ, float farZ)
{
	const float tanLeft        = tanf(fov.angleLeft);
	const float tanRight       = tanf(fov.angleRight);
	const float tanDown        = tanf(fov.angleDown);
	const float tanUp          = tanf(fov.angleUp);

	const float tanAngleWidth = tanRight - tanLeft;

	// Set to tanAngleDown - tanAngleUp for a clip space with positive Y
	// down (Vulkan). Set to tanAngleUp - tanAngleDown for a clip space with
	// positive Y up (OpenGL / D3D / Metal).
#if defined(SKR_VULKAN)
	const float tanAngleHeight = (tanDown - tanUp);
#else
	const float tanAngleHeight = (tanUp - tanDown);
#endif

	// Set to nearZ for a [-1,1] Z clip space (OpenGL / OpenGL ES).
	// Set to zero for a [0,1] Z clip space (Vulkan / D3D / Metal).
#if defined(SKR_OPENGL) || defined(SKR_OPENGLES)
	const float offsetZ = nearZ;
#else
	const float offsetZ = 0;
#endif

	float result[16] = { 0 };
	if (farZ <= nearZ) {
		// place the far plane at infinity
		result[0] = 2 / tanAngleWidth;
		result[4] = 0;
		result[8] = (tanRight + tanLeft) / tanAngleWidth;
		result[12] = 0;

		result[1] = 0;
		result[5] = 2 / tanAngleHeight;
		result[9] = (tanUp + tanDown) / tanAngleHeight;
		result[13] = 0;

		result[2] = 0;
		result[6] = 0;
		result[10] = -1;
		result[14] = -(nearZ + offsetZ);

		result[3] = 0;
		result[7] = 0;
		result[11] = -1;
		result[15] = 0;
	} else {
		// normal projection
		result[0] = 2 / tanAngleWidth;
		result[4] = 0;
		result[8] = (tanRight + tanLeft) / tanAngleWidth;
		result[12] = 0;

		result[1] = 0;
		result[5] = 2 / tanAngleHeight;
		result[9] = (tanUp + tanDown) / tanAngleHeight;
		result[13] = 0;

		result[2] = 0;
		result[6] = 0;
		result[10] = -(farZ + offsetZ) / (farZ - nearZ);
		result[14] = -(farZ * (nearZ + offsetZ)) / (farZ - nearZ);

		result[3] = 0;
		result[7] = 0;
		result[11] = -1;
		result[15] = 0;
	}

	hmm_mat4 resultMat;
	memcpy(&resultMat, result, sizeof(hmm_mat4));
	return resultMat;
}