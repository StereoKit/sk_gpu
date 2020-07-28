
// When using single file header like normal, do this
//#define SKR_OPENGL
//#define SKR_IMPL
//#include "sk_gpu.h"

// For easier development
#include "../src/sk_gpu_dev.h"

// Also see here for OpenXR GL reference:
// https://github.com/jherico/OpenXR-Samples/blob/master/src/examples/sdl2_gl_single_file_example.cpp

#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>

#define XR_APP_IMPL
#include "xr_app.h"
#include "app.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define HANDMADE_MATH_IMPLEMENTATION
#include "HandmadeMath.h"

///////////////////////////////////////////

struct swapchain_surfdata_t {
	skr_tex_t depth_tex;
	skr_tex_t render_tex;
};

struct app_swapchain_t {
	int32_t view_count;
	int32_t surf_count;
	swapchain_surfdata_t *surfaces;
};

///////////////////////////////////////////

app_swapchain_t app_swapchain = {};
const char     *app_name      = "sk_gpu.h";
xr_settings_t  xr_functions  = {};

///////////////////////////////////////////

bool main_init    ();
void main_shutdown();
bool main_step    ();

bool main_init_gfx         (void *user_data, const XrGraphicsRequirements *requirements, XrGraphicsBinding *out_graphics);
bool main_init_swapchain   (void *user_data, int32_t view_count, int32_t surface_count, void **textures, int32_t width, int32_t height, int64_t fmt);
void main_destroy_swapchain(void *user_data);
void main_render           (void *user_data, const XrCompositionLayerProjectionView *view, int32_t swapchain_view_id, int32_t swapchain_surface_id);

///////////////////////////////////////////

int main() {
	if (!main_init())
		return -1;

	while (main_step());

	main_shutdown();
	return 1;
}

///////////////////////////////////////////

bool main_init() {
	xr_functions.init_gfx           = main_init_gfx;
	xr_functions.init_swapchain     = main_init_swapchain;
	xr_functions.destroy_swapchain  = main_destroy_swapchain;
	xr_functions.draw               = main_render;
	xr_functions.pixel_formats      = (int64_t*)malloc(sizeof(int64_t) * 4);
	xr_functions.pixel_format_count = 4;
	xr_functions.depth_formats      = (int64_t*)malloc(sizeof(int64_t) * 3);
	xr_functions.depth_format_count = 3;
	xr_functions.pixel_formats[0]   = skr_tex_fmt_to_native(skr_tex_fmt_rgba32_linear);
	xr_functions.pixel_formats[1]   = skr_tex_fmt_to_native(skr_tex_fmt_bgra32_linear);
	xr_functions.pixel_formats[2]   = skr_tex_fmt_to_native(skr_tex_fmt_rgba32);
	xr_functions.pixel_formats[3]   = skr_tex_fmt_to_native(skr_tex_fmt_bgra32);
	xr_functions.depth_formats[0]   = skr_tex_fmt_to_native(skr_tex_fmt_depth16);
	xr_functions.depth_formats[1]   = skr_tex_fmt_to_native(skr_tex_fmt_depth32);
	xr_functions.depth_formats[2]   = skr_tex_fmt_to_native(skr_tex_fmt_depthstencil);
	if (!openxr_init(app_name, &xr_functions)) return false;

	app_init();
	return true;
}

///////////////////////////////////////////

void main_shutdown() {
	app_shutdown();
	skr_shutdown();
}

///////////////////////////////////////////

bool main_step() {
	return openxr_step();
}

///////////////////////////////////////////

bool main_init_gfx(void *user_data, const XrGraphicsRequirements *requirements, XrGraphicsBinding *out_graphics) {
	void *luid = nullptr;
#ifdef SKR_DIRECT3D11
	luid = (void *)&requirement.adapterLuid;
#endif
	skr_log_callback([](const char *text) { printf("%s\n", text); });
	if (!skr_init(app_name, nullptr, luid))
		return false;

	skr_platform_data_t platform = skr_get_platform_data();
#if defined(SKR_OPENGL) && defined(_WIN32)
	out_graphics->hDC   = (HDC  )platform.gl_hdc;
	out_graphics->hGLRC = (HGLRC)platform.gl_hrc;
#elif defined(SKR_OPENGL) && defined(__ANDROID__)
	out_graphics->egl_display = platform.egl_display;
	out_graphics->egl_surface = platform.egl_surface;
	out_graphics->egl_context = platform.egl_context;
#elif defined(SKR_DIRECT3D11)
	out_graphics->device = (ID3D11Device*)platform.d3d11_device;
#endif

	return true;
}

///////////////////////////////////////////

bool main_init_swapchain(void *user_data, int32_t view_count, int32_t surface_count, void **textures, int32_t width, int32_t height, int64_t fmt) {
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

void main_destroy_swapchain(void *user_data) {
	for (int32_t i = 0; i < app_swapchain.surf_count * app_swapchain.view_count; i++) {
		skr_tex_destroy(&app_swapchain.surfaces[i].render_tex);
		skr_tex_destroy(&app_swapchain.surfaces[i].depth_tex);
	}
	free(app_swapchain.surfaces);
	app_swapchain = {};
}

///////////////////////////////////////////

void main_render(void *user_data, const XrCompositionLayerProjectionView *view, int32_t view_id, int32_t surf_id) {
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
	hmm_mat4 proj_mat;
	openxr_projection(view->fov, 0.01f, 100.0f, &proj_mat.Elements[0][0]);

	app_render(view_mat, proj_mat);
}

///////////////////////////////////////////

