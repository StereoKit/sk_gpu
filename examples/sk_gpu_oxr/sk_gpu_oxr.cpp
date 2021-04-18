// When using single file header like normal, do this
#define SKG_IMPL
#include "../../sk_gpu.h"

// Also see here for OpenXR GL reference:
// https://github.com/jherico/OpenXR-Samples/blob/master/src/examples/sdl2_gl_single_file_example.cpp

#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>

#define XR_APP_IMPL
#include "../common/xr_app.h"
#include "../common/app.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define HANDMADE_MATH_IMPLEMENTATION
#include "../common/HandmadeMath.h"

///////////////////////////////////////////

struct swapchain_surfdata_t {
	skg_tex_t depth_tex;
	skg_tex_t render_tex;
};

struct app_swapchain_t {
	int32_t view_count;
	int32_t surf_count;
	swapchain_surfdata_t *surfaces;
};

///////////////////////////////////////////

app_swapchain_t app_swapchain = {};
const char     *app_name      = "sk_gpu.h";
xr_settings_t   xr_functions  = {};

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
	xr_functions.pixel_formats[0]   = skg_tex_fmt_to_native(skg_tex_fmt_rgba32_linear);
	xr_functions.pixel_formats[1]   = skg_tex_fmt_to_native(skg_tex_fmt_bgra32_linear);
	xr_functions.pixel_formats[2]   = skg_tex_fmt_to_native(skg_tex_fmt_rgba32);
	xr_functions.pixel_formats[3]   = skg_tex_fmt_to_native(skg_tex_fmt_bgra32);
	xr_functions.depth_formats[0]   = skg_tex_fmt_to_native(skg_tex_fmt_depth16);
	xr_functions.depth_formats[1]   = skg_tex_fmt_to_native(skg_tex_fmt_depth32);
	xr_functions.depth_formats[2]   = skg_tex_fmt_to_native(skg_tex_fmt_depthstencil);
	if (!openxr_init(app_name, &xr_functions)) return false;

	app_init();
	return true;
}

///////////////////////////////////////////

void main_shutdown() {
	app_shutdown();
	skg_shutdown();
}

///////////////////////////////////////////

bool main_step() {
	return openxr_step();
}

///////////////////////////////////////////

bool main_init_gfx(void *user_data, const XrGraphicsRequirements *requirements, XrGraphicsBinding *out_graphics) {
	void *luid = nullptr;
#ifdef SKG_DIRECT3D11
	luid = (void *)&requirements->adapterLuid;
#endif
	skg_callback_log([](skg_log_ level, const char *text) { printf("[%d] %s\n", level, text); });
	if (!skg_init(app_name, luid))
		return false;

	skg_platform_data_t platform = skg_get_platform_data();
#if defined(SKG_OPENGL) && defined(_WIN32)
	out_graphics->hDC   = (HDC  )platform_gl_hdc;
	out_graphics->hGLRC = (HGLRC)platform._gl_hrc;
#elif defined(SKG_OPENGL) && defined(__ANDROID__)
	out_graphics->egl_display = platform._egl_display;
	out_graphics->egl_surface = platform._egl_surface;
	out_graphics->egl_context = platform._egl_context;
#elif defined(SKG_DIRECT3D11)
	out_graphics->device = (ID3D11Device*)platform._d3d11_device;
#endif

	return true;
}

///////////////////////////////////////////

bool main_init_swapchain(void *user_data, int32_t view_count, int32_t surface_count, void **textures, int32_t width, int32_t height, int64_t fmt) {
	app_swapchain.view_count = view_count;
	app_swapchain.surf_count = surface_count;
	app_swapchain.surfaces   = (swapchain_surfdata_t*)malloc(sizeof(swapchain_surfdata_t) * view_count * surface_count);
	skg_tex_fmt_ skg_format = skg_tex_fmt_from_native(fmt);

	for (int32_t i = 0; i < view_count*surface_count; i++) {
		app_swapchain.surfaces[i].render_tex = skg_tex_create_from_existing(textures[i], skg_tex_type_rendertarget, skg_format, width, height, surface_count);
		app_swapchain.surfaces[i].depth_tex  = skg_tex_create(skg_tex_type_depth, skg_use_static, skg_tex_fmt_depth32, skg_mip_none);
		skg_tex_set_contents (&app_swapchain.surfaces[i].depth_tex, nullptr, 1, width, height);
		skg_tex_attach_depth(&app_swapchain.surfaces[i].render_tex, &app_swapchain.surfaces[i].depth_tex);
	}
	return true;
}

///////////////////////////////////////////

void main_destroy_swapchain(void *user_data) {
	for (int32_t i = 0; i < app_swapchain.surf_count * app_swapchain.view_count; i++) {
		skg_tex_destroy(&app_swapchain.surfaces[i].render_tex);
		skg_tex_destroy(&app_swapchain.surfaces[i].depth_tex);
	}
	free(app_swapchain.surfaces);
	app_swapchain = {};
}

///////////////////////////////////////////

void main_render(void *user_data, const XrCompositionLayerProjectionView *view, int32_t view_id, int32_t surf_id) {
	float clear_color[4] = { 0,0,0,1 };
	skg_tex_t *target = &app_swapchain.surfaces[view_id * app_swapchain.surf_count + surf_id].render_tex;
	skg_tex_target_bind(target, true, clear_color);

	hmm_quaternion head_orientation;
	memcpy(&head_orientation, &view->pose.orientation, sizeof(XrQuaternionf));
	hmm_vec3 head_pos;
	memcpy(&head_pos, &view->pose.position, sizeof(XrVector3f));
	
	hmm_mat4 view_mat = 
		HMM_QuaternionToMat4(HMM_InverseQuaternion(head_orientation)) *
		HMM_Translate(HMM_Vec3( -view->pose.position.x, -view->pose.position.y, -view->pose.position.z));
	hmm_mat4 proj_mat;
	openxr_projection(view->fov, 0.01f, 100.0f, &proj_mat.Elements[0][0]);

	app_render(0, view_mat, proj_mat);
}

///////////////////////////////////////////