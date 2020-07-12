//#define SKR_VULKAN
//#define SKR_DIRECT3D12
//#define SKR_DIRECT3D11
//#define SKR_OPENGL

// Also see here for OpenXR GL reference:
// https://github.com/jherico/OpenXR-Samples/blob/master/src/examples/sdl2_gl_single_file_example.cpp

// OpenXR platform defines
/*#define XR_USE_PLATFORM_WIN32

#if defined(SKR_DIRECT3D11)
#define XR_USE_GRAPHICS_API_D3D11
#define XR_GFX_EXTENSION XR_KHR_D3D11_ENABLE_EXTENSION_NAME
#define XrSwapchainImage XrSwapchainImageD3D11KHR
#define XR_TYPE_SWAPCHAIN_IMAGE XR_TYPE_SWAPCHAIN_IMAGE_D3D11_KHR
#define XrGraphicsRequirements XrGraphicsRequirementsD3D11KHR
#define XR_TYPE_GRAPHICS_REQUIREMENTS XR_TYPE_GRAPHICS_REQUIREMENTS_D3D11_KHR
#define PFN_xrGetGraphicsRequirementsKHR PFN_xrGetD3D11GraphicsRequirementsKHR
#define NAME_xrGetGraphicsRequirementsKHR "xrGetD3D11GraphicsRequirementsKHR"
#define XrGraphicsBinding XrGraphicsBindingD3D11KHR
#define XR_TYPE_GRAPHICS_BINDING XR_TYPE_GRAPHICS_BINDING_D3D11_KHR

#elif defined(SKR_OPENGL)
#define XR_USE_GRAPHICS_API_OPENGL
#define XR_GFX_EXTENSION XR_KHR_OPENGL_ENABLE_EXTENSION_NAME
#define XrSwapchainImage XrSwapchainImageOpenGLKHR
#define XR_TYPE_SWAPCHAIN_IMAGE XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR
#define XrGraphicsRequirements XrGraphicsRequirementsOpenGLKHR
#define XR_TYPE_GRAPHICS_REQUIREMENTS XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR
#define PFN_xrGetGraphicsRequirementsKHR PFN_xrGetOpenGLGraphicsRequirementsKHR
#define NAME_xrGetGraphicsRequirementsKHR "xrGetOpenGLGraphicsRequirementsKHR"
#define XrGraphicsBinding XrGraphicsBindingOpenGLWin32KHR
#define XR_TYPE_GRAPHICS_BINDING XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR

#elif defined(SKR_VULKAN)
#define XR_USE_GRAPHICS_API_VULKAN
#define XR_GFX_EXTENSION XR_KHR_VULKAN_ENABLE_EXTENSION_NAME
#define XrSwapchainImage XrSwapchainImageVulkanKHR
#define XR_TYPE_SWAPCHAIN_IMAGE XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR
#define XrGraphicsRequirements XrGraphicsRequirementsVulkanKHR
#define XR_TYPE_GRAPHICS_REQUIREMENTS XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR
#define PFN_xrGetGraphicsRequirementsKHR PFN_xrGetVulkanGraphicsRequirementsKHR
#define NAME_xrGetGraphicsRequirementsKHR "xrGetVulkanGraphicsRequirementsKHR"
#define XrGraphicsBinding XrGraphicsBindingVulkanKHR
#define XR_TYPE_GRAPHICS_BINDING XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR

#elif defined(SKR_OPENGL_ES)
#define XR_USE_GRAPHICS_API_OPENGL_ES
#define XR_GFX_EXTENSION XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME

#endif

#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>

HWND app_hwnd;

#include "sk_gpu.h"
#include "shaders.h"

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

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

struct swapchain_t {
	XrSwapchain handle;
	int32_t     width;
	int32_t     height;
	vector<XrSwapchainImage>     surface_images;
	vector<swapchain_surfdata_t> surface_data;
};

struct input_state_t {
	XrActionSet actionSet;
	XrAction    poseAction;
	XrAction    selectAction;
	XrPath   handSubactionPath[2];
	XrSpace  handSpace[2];
	XrPosef  handPose[2];
	XrBool32 renderHand[2];
	XrBool32 handSelect[2];
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

int         app_width  = 1280;
int         app_height = 720;
bool        app_run    = true;
const char *app_name   = "Cross Platform Renderer";

XrFormFactor            app_config_form = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
XrViewConfigurationType app_config_view = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;

///////////////////////////////////////////

const XrPosef  xr_pose_identity = { {0,0,0,1}, {0,0,0} };
XrInstance     xr_instance      = {};
XrSession      xr_session       = {};
XrSessionState xr_session_state = XR_SESSION_STATE_UNKNOWN;
bool           xr_running       = false;
XrSpace        xr_app_space     = {};
XrSystemId     xr_system_id     = XR_NULL_SYSTEM_ID;
input_state_t  xr_input         = { };
XrEnvironmentBlendMode xr_blend;
skr_tex_fmt_   xr_swapchain_fmt = skr_tex_fmt_rgba32_linear;

vector<XrView>                  xr_views;
vector<XrViewConfigurationView> xr_config_views;
vector<swapchain_t>             xr_swapchains;

PFN_xrGetGraphicsRequirementsKHR ext_xrGetGraphicsRequirementsKHR;

bool openxr_init          (const char *app_name, void *hwnd, int32_t screen_width, int32_t screen_height, int64_t swapchain_format);
void openxr_make_actions  ();
void openxr_shutdown      ();
void openxr_poll_events   (bool &exit);
void openxr_poll_actions  ();
void openxr_poll_predicted(XrTime predicted_time);
void openxr_render_frame  ();
bool openxr_render_layer  (XrTime predictedTime, vector<XrCompositionLayerProjectionView> &projectionViews, XrCompositionLayerProjection &layer);
void openxr_preferred_format(int64_t &out_color, int64_t &out_depth);

///////////////////////////////////////////

bool app_init();
void app_shutdown();
bool app_step();

void app_render_layer(XrCompositionLayerProjectionView &view, swapchain_surfdata_t &surface);
void app_swapchain_destroy(swapchain_t &swapchain);
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
	if (!openxr_init(app_name, app_hwnd, app_width, app_height)) return false;

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

///////////////////////////////////////////
// OpenXR code                           //
///////////////////////////////////////////

bool openxr_init(const char *app_name, void *hwnd, int32_t screen_width, int32_t screen_height) {
	const char          *extensions[] = { XR_GFX_EXTENSION };
	XrInstanceCreateInfo createInfo   = { XR_TYPE_INSTANCE_CREATE_INFO };
	createInfo.enabledExtensionCount      = _countof(extensions);
	createInfo.enabledExtensionNames      = extensions;
	createInfo.applicationInfo.apiVersion = XR_CURRENT_API_VERSION;
	strcpy_s(createInfo.applicationInfo.applicationName, 128, app_name);
	xrCreateInstance(&createInfo, &xr_instance);

	// Check if OpenXR is on this system, if this is null here, the user needs to install an
	// OpenXR runtime and ensure it's active!
	if (xr_instance == nullptr)
		return false;

	// Load extension methods that we'll need for this application! There's a
	// couple ways to do this, and this is a fairly manual one. Chek out this
	// file for another way to do it:
	// https://github.com/maluoi/StereoKit/blob/master/StereoKitC/systems/platform/openxr_extensions.h
	xrGetInstanceProcAddr(
		xr_instance,
		NAME_xrGetGraphicsRequirementsKHR,
		(PFN_xrVoidFunction *)(&ext_xrGetGraphicsRequirementsKHR));

	// Request a form factor from the device (HMD, Handheld, etc.)
	XrSystemGetInfo systemInfo = { XR_TYPE_SYSTEM_GET_INFO };
	systemInfo.formFactor = app_config_form;
	xrGetSystem(xr_instance, &systemInfo, &xr_system_id);

	// Check what blend mode is valid for this device (opaque vs transparent displays)
	// We'll just take the first one available!
	uint32_t blend_count = 0;
	xrEnumerateEnvironmentBlendModes(xr_instance, xr_system_id, app_config_view, 1, &blend_count, &xr_blend);

	// OpenXR wants to ensure apps are using the correct graphics card, so this MUST be called 
	// before xrCreateSession. This is crucial on devices that have multiple graphics cards, 
	// like laptops with integrated graphics chips in addition to dedicated graphics cards.
	XrGraphicsRequirements requirement = { XR_TYPE_GRAPHICS_REQUIREMENTS };
	ext_xrGetGraphicsRequirementsKHR(xr_instance, xr_system_id, &requirement);
	void *luid = nullptr;
#ifdef SKR_DIRECT3D11
	luid = (void *)&requirement.adapterLuid;
#endif
	if (!skr_init(app_name, luid))
		return false;

	// A session represents this application's desire to display things! This is where we hook up our graphics API.
	// This does not start the session, for that, you'll need a call to xrBeginSession, which we do in openxr_poll_events
	XrGraphicsBinding binding = { XR_TYPE_GRAPHICS_BINDING };
#if defined(SKR_OPENGL)
	skr_platform_data_t platform = skr_get_platform_data();
	binding.hDC   = (HDC  )platform.gl_hdc;
	binding.hGLRC = (HGLRC)platform.gl_hrc;
#elif defined(SKR_DIRECT3D11)
	binding.device = (ID3D11Device*)skr_get_platform_data().d3d11_device;
#endif
	XrSessionCreateInfo sessionInfo = { XR_TYPE_SESSION_CREATE_INFO };
	sessionInfo.next     = &binding;
	sessionInfo.systemId = xr_system_id;
	xrCreateSession(xr_instance, &sessionInfo, &xr_session);

	// Unable to start a session, may not have an MR device attached or ready
	if (xr_session == nullptr)
		return false;

	// OpenXR uses a couple different types of reference frames for positioning content, we need to choose one for
	// displaying our content! STAGE would be relative to the center of your guardian system's bounds, and LOCAL
	// would be relative to your device's starting location. HoloLens doesn't have a STAGE, so we'll use LOCAL.
	XrReferenceSpaceCreateInfo ref_space = { XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
	ref_space.poseInReferenceSpace = xr_pose_identity;
	ref_space.referenceSpaceType   = XR_REFERENCE_SPACE_TYPE_LOCAL;
	xrCreateReferenceSpace(xr_session, &ref_space, &xr_app_space);

	// Find OpenXR's format preference
	int64_t native_color_fmt, native_depth_fmt;
	openxr_preferred_format(native_color_fmt, native_depth_fmt);

	// Now we need to find all the viewpoints we need to take care of! For a stereo headset, this should be 2.
	// Similarly, for an AR phone, we'll need 1, and a VR cave could have 6, or even 12!
	uint32_t view_count = 0;
	xrEnumerateViewConfigurationViews(xr_instance, xr_system_id, app_config_view, 0, &view_count, nullptr);
	xr_config_views.resize(view_count, { XR_TYPE_VIEW_CONFIGURATION_VIEW });
	xr_views       .resize(view_count, { XR_TYPE_VIEW });
	xrEnumerateViewConfigurationViews(xr_instance, xr_system_id, app_config_view, view_count, &view_count, xr_config_views.data());
	for (uint32_t i = 0; i < view_count; i++) {
		// Create a swapchain for this viewpoint! A swapchain is a set of texture buffers used for displaying to screen,
		// typically this is a backbuffer and a front buffer, one for rendering data to, and one for displaying on-screen.
		// A note about swapchain image format here! OpenXR doesn't create a concrete image format for the texture, like 
		// DXGI_FORMAT_R8G8B8A8_UNORM. Instead, it switches to the TYPELESS variant of the provided texture format, like 
		// DXGI_FORMAT_R8G8B8A8_TYPELESS. When creating an ID3D11RenderTargetView for the swapchain texture, we must specify
		// a concrete type like DXGI_FORMAT_R8G8B8A8_UNORM, as attempting to create a TYPELESS view will throw errors, so 
		// we do need to store the format separately and remember it later.
		XrViewConfigurationView &view           = xr_config_views[i];
		XrSwapchainCreateInfo    swapchain_info = { XR_TYPE_SWAPCHAIN_CREATE_INFO };
		XrSwapchain              handle;
		swapchain_info.arraySize   = 1;
		swapchain_info.mipCount    = 1;
		swapchain_info.faceCount   = 1;
		swapchain_info.format      = native_color_fmt;
		swapchain_info.width       = view.recommendedImageRectWidth;
		swapchain_info.height      = view.recommendedImageRectHeight;
		swapchain_info.sampleCount = view.recommendedSwapchainSampleCount;
		swapchain_info.usageFlags  = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
		xrCreateSwapchain(xr_session, &swapchain_info, &handle);

		// Find out how many textures were generated for the swapchain
		uint32_t surface_count = 0;
		xrEnumerateSwapchainImages(handle, 0, &surface_count, nullptr);

		// We'll want to track our own information about the swapchain, so we can draw stuff onto it! We'll also create
		// a depth buffer for each generated texture here as well with make_surfacedata.
		swapchain_t swapchain = {};
		swapchain.width  = swapchain_info.width;
		swapchain.height = swapchain_info.height;
		swapchain.handle = handle;
		swapchain.surface_images.resize(surface_count, XrSwapchainImage{ XR_TYPE_SWAPCHAIN_IMAGE } );
		swapchain.surface_data  .resize(surface_count);
		xrEnumerateSwapchainImages(swapchain.handle, surface_count, &surface_count, (XrSwapchainImageBaseHeader*)swapchain.surface_images.data());
		for (uint32_t i = 0; i < surface_count; i++) {
			swapchain_surfdata_t result = {};
#if defined(SKR_DIRECT3D11)
			void *tex = swapchain.surface_images[i].texture;
#elif defined(SKR_OPENGL)
			void *tex = &swapchain.surface_images[i].image;
#endif
			result.render_tex = skr_tex_from_native(tex, skr_tex_type_rendertarget, xr_swapchain_fmt);
			result.depth_tex  = skr_tex_create(skr_tex_type_depth, skr_use_static, skr_tex_fmt_depth32, skr_mip_none);
			skr_tex_set_data(&result.depth_tex, nullptr, 1, swapchain_info.width, swapchain_info.height);
			swapchain.surface_data[i] = result;
		}
		xr_swapchains.push_back(swapchain);
	}

	return true;
}

///////////////////////////////////////////

void openxr_preferred_format(int64_t &out_color, int64_t &out_depth) {
	int64_t pixel_formats[] = {
		skr_tex_fmt_to_native(skr_tex_fmt_rgba32_linear),
		skr_tex_fmt_to_native(skr_tex_fmt_bgra32_linear),
		skr_tex_fmt_to_native(skr_tex_fmt_rgba32),
		skr_tex_fmt_to_native(skr_tex_fmt_bgra32),};
	int64_t depth_formats[] = {
		skr_tex_fmt_to_native(skr_tex_fmt_depth16),
		skr_tex_fmt_to_native(skr_tex_fmt_depth32),
		skr_tex_fmt_to_native(skr_tex_fmt_depthstencil),};

	// Get the list of formats OpenXR would like
	uint32_t count = 0;
	xrEnumerateSwapchainFormats(xr_session, 0, &count, nullptr);
	int64_t *formats = (int64_t *)malloc(sizeof(int64_t) * count);
	xrEnumerateSwapchainFormats(xr_session, count, &count, formats);

	// Check those against our formats
	out_color = 0;
	out_depth = 0;
	for (uint32_t i=0; i<count; i++) {
		for (int32_t f=0; out_color == 0 && f<_countof(pixel_formats); f++) {
			if (formats[i] == pixel_formats[f]) {
				out_color = pixel_formats[f];
				break;
			}
		}
		for (int32_t f=0; out_depth == 0 && f<_countof(depth_formats); f++) {
			if (formats[i] == depth_formats[f]) {
				out_depth = depth_formats[f];
				break;
			}
		}
	}

	// Release memory
	free(formats);
}

///////////////////////////////////////////

void openxr_make_actions() {
	XrActionSetCreateInfo actionset_info = { XR_TYPE_ACTION_SET_CREATE_INFO };
	strcpy_s(actionset_info.actionSetName,          "gameplay");
	strcpy_s(actionset_info.localizedActionSetName, "Gameplay");
	xrCreateActionSet(xr_instance, &actionset_info, &xr_input.actionSet);
	xrStringToPath(xr_instance, "/user/hand/left",  &xr_input.handSubactionPath[0]);
	xrStringToPath(xr_instance, "/user/hand/right", &xr_input.handSubactionPath[1]);

	// Create an action to track the position and orientation of the hands! This is
	// the controller location, or the center of the palms for actual hands.
	XrActionCreateInfo action_info = { XR_TYPE_ACTION_CREATE_INFO };
	action_info.countSubactionPaths = _countof(xr_input.handSubactionPath);
	action_info.subactionPaths      = xr_input.handSubactionPath;
	action_info.actionType          = XR_ACTION_TYPE_POSE_INPUT;
	strcpy_s(action_info.actionName,          "hand_pose");
	strcpy_s(action_info.localizedActionName, "Hand Pose");
	xrCreateAction(xr_input.actionSet, &action_info, &xr_input.poseAction);

	// Create an action for listening to the select action! This is primary trigger
	// on controllers, and an airtap on HoloLens
	action_info.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
	strcpy_s(action_info.actionName,          "select");
	strcpy_s(action_info.localizedActionName, "Select");
	xrCreateAction(xr_input.actionSet, &action_info, &xr_input.selectAction);

	// Bind the actions we just created to specific locations on the Khronos simple_controller
	// definition! These are labeled as 'suggested' because they may be overridden by the runtime
	// preferences. For example, if the runtime allows you to remap buttons, or provides input
	// accessibility settings.
	XrPath profile_path;
	XrPath pose_path  [2];
	XrPath select_path[2];
	xrStringToPath(xr_instance, "/user/hand/left/input/grip/pose",     &pose_path[0]);
	xrStringToPath(xr_instance, "/user/hand/right/input/grip/pose",    &pose_path[1]);
	xrStringToPath(xr_instance, "/user/hand/left/input/select/click",  &select_path[0]);
	xrStringToPath(xr_instance, "/user/hand/right/input/select/click", &select_path[1]);
	xrStringToPath(xr_instance, "/interaction_profiles/khr/simple_controller", &profile_path);
	XrActionSuggestedBinding bindings[] = {
		{ xr_input.poseAction,   pose_path[0]   },
		{ xr_input.poseAction,   pose_path[1]   },
		{ xr_input.selectAction, select_path[0] },
		{ xr_input.selectAction, select_path[1] }, };
	XrInteractionProfileSuggestedBinding suggested_binds = { XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING };
	suggested_binds.interactionProfile     = profile_path;
	suggested_binds.suggestedBindings      = &bindings[0];
	suggested_binds.countSuggestedBindings = _countof(bindings);
	xrSuggestInteractionProfileBindings(xr_instance, &suggested_binds);

	// Create frames of reference for the pose actions
	for (int32_t i = 0; i < 2; i++) {
		XrActionSpaceCreateInfo action_space_info = { XR_TYPE_ACTION_SPACE_CREATE_INFO };
		action_space_info.action            = xr_input.poseAction;
		action_space_info.poseInActionSpace = xr_pose_identity;
		action_space_info.subactionPath     = xr_input.handSubactionPath[i];
		xrCreateActionSpace(xr_session, &action_space_info, &xr_input.handSpace[i]);
	}

	// Attach the action set we just made to the session
	XrSessionActionSetsAttachInfo attach_info = { XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO };
	attach_info.countActionSets = 1;
	attach_info.actionSets      = &xr_input.actionSet;
	xrAttachSessionActionSets(xr_session, &attach_info);
}

///////////////////////////////////////////

void openxr_shutdown() {
	// We used a graphics API to initialize the swapchain data, so we'll
	// give it a chance to release anythig here!
	for (int32_t i = 0; i < xr_swapchains.size(); i++) {
		xrDestroySwapchain(xr_swapchains[i].handle);
		app_swapchain_destroy(xr_swapchains[i]);
	}
	xr_swapchains.clear();

	// Release all the other OpenXR resources that we've created!
	// What gets allocated, must get deallocated!
	if (xr_input.actionSet != XR_NULL_HANDLE) {
		if (xr_input.handSpace[0] != XR_NULL_HANDLE) xrDestroySpace(xr_input.handSpace[0]);
		if (xr_input.handSpace[1] != XR_NULL_HANDLE) xrDestroySpace(xr_input.handSpace[1]);
		xrDestroyActionSet(xr_input.actionSet);
	}
	if (xr_app_space != XR_NULL_HANDLE) xrDestroySpace   (xr_app_space);
	if (xr_session   != XR_NULL_HANDLE) xrDestroySession (xr_session);
	if (xr_instance  != XR_NULL_HANDLE) xrDestroyInstance(xr_instance);
}

///////////////////////////////////////////

void openxr_poll_events(bool &exit) {
	exit = false;

	XrEventDataBuffer event_buffer = { XR_TYPE_EVENT_DATA_BUFFER };

	while (xrPollEvent(xr_instance, &event_buffer) == XR_SUCCESS) {
		switch (event_buffer.type) {
		case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
			XrEventDataSessionStateChanged *changed = (XrEventDataSessionStateChanged*)&event_buffer;
			xr_session_state = changed->state;

			// Session state change is where we can begin and end sessions, as well as find quit messages!
			switch (xr_session_state) {
			case XR_SESSION_STATE_READY: {
				XrSessionBeginInfo begin_info = { XR_TYPE_SESSION_BEGIN_INFO };
				begin_info.primaryViewConfigurationType = app_config_view;
				xrBeginSession(xr_session, &begin_info);
				xr_running = true;
			} break;
			case XR_SESSION_STATE_STOPPING: {
				xr_running = false;
				xrEndSession(xr_session); 
			} break;
			case XR_SESSION_STATE_EXITING:      exit = true;              break;
			case XR_SESSION_STATE_LOSS_PENDING: exit = true;              break;
			}
		} break;
		case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING: exit = true; return;
		}
		event_buffer = { XR_TYPE_EVENT_DATA_BUFFER };
	}
}

///////////////////////////////////////////

void openxr_poll_actions() {
	if (xr_session_state != XR_SESSION_STATE_FOCUSED)
		return;

	// Update our action set with up-to-date input data!
	XrActiveActionSet action_set = { };
	action_set.actionSet     = xr_input.actionSet;
	action_set.subactionPath = XR_NULL_PATH;

	XrActionsSyncInfo sync_info = { XR_TYPE_ACTIONS_SYNC_INFO };
	sync_info.countActiveActionSets = 1;
	sync_info.activeActionSets      = &action_set;

	xrSyncActions(xr_session, &sync_info);

	// Now we'll get the current states of our actions, and store them for later use
	for (uint32_t hand = 0; hand < 2; hand++) {
		XrActionStateGetInfo get_info = { XR_TYPE_ACTION_STATE_GET_INFO };
		get_info.subactionPath = xr_input.handSubactionPath[hand];

		XrActionStatePose pose_state = { XR_TYPE_ACTION_STATE_POSE };
		get_info.action = xr_input.poseAction;
		xrGetActionStatePose(xr_session, &get_info, &pose_state);
		xr_input.renderHand[hand] = pose_state.isActive;

		// Events come with a timestamp
		XrActionStateBoolean select_state = { XR_TYPE_ACTION_STATE_BOOLEAN };
		get_info.action = xr_input.selectAction;
		xrGetActionStateBoolean(xr_session, &get_info, &select_state);
		xr_input.handSelect[hand] = select_state.currentState && select_state.changedSinceLastSync;

		// If we have a select event, update the hand pose to match the event's timestamp
		if (xr_input.handSelect[hand]) {
			XrSpaceLocation space_location = { XR_TYPE_SPACE_LOCATION };
			XrResult        res            = xrLocateSpace(xr_input.handSpace[hand], xr_app_space, select_state.lastChangeTime, &space_location);
			if (XR_UNQUALIFIED_SUCCESS(res) &&
				(space_location.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT   ) != 0 &&
				(space_location.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0) {
				xr_input.handPose[hand] = space_location.pose;
			}
		}
	}
}

///////////////////////////////////////////

void openxr_poll_predicted(XrTime predicted_time) {
	if (xr_session_state != XR_SESSION_STATE_FOCUSED)
		return;

	// Update hand position based on the predicted time of when the frame will be rendered! This 
	// should result in a more accurate location, and reduce perceived lag.
	for (size_t i = 0; i < 2; i++) {
		if (!xr_input.renderHand[i])
			continue;
		XrSpaceLocation spaceRelation = { XR_TYPE_SPACE_LOCATION };
		XrResult        res           = xrLocateSpace(xr_input.handSpace[i], xr_app_space, predicted_time, &spaceRelation);
		if (XR_UNQUALIFIED_SUCCESS(res) &&
			(spaceRelation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT   ) != 0 &&
			(spaceRelation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0) {
			xr_input.handPose[i] = spaceRelation.pose;
		}
	}
}

///////////////////////////////////////////

void openxr_render_frame() {
	// Block until the previous frame is finished displaying, and is ready for another one.
	// Also returns a prediction of when the next frame will be displayed, for use with predicting
	// locations of controllers, viewpoints, etc.
	XrFrameState frame_state = { XR_TYPE_FRAME_STATE };
	xrWaitFrame (xr_session, nullptr, &frame_state);
	// Must be called before any rendering is done! This can return some interesting flags, like 
	// XR_SESSION_VISIBILITY_UNAVAILABLE, which means we could skip rendering this frame and call
	// xrEndFrame right away.
	xrBeginFrame(xr_session, nullptr);

	// Execute any code that's dependant on the predicted time, such as updating the location of
	// controller models.
	openxr_poll_predicted(frame_state.predictedDisplayTime);

	// If the session is active, lets render our layer in the compositor!
	XrCompositionLayerBaseHeader            *layer      = nullptr;
	XrCompositionLayerProjection             layer_proj = { XR_TYPE_COMPOSITION_LAYER_PROJECTION };
	vector<XrCompositionLayerProjectionView> views;
	bool session_active = xr_session_state == XR_SESSION_STATE_VISIBLE || xr_session_state == XR_SESSION_STATE_FOCUSED;
	if (session_active && openxr_render_layer(frame_state.predictedDisplayTime, views, layer_proj)) {
		layer = (XrCompositionLayerBaseHeader*)&layer_proj;
	}

	// We're finished with rendering our layer, so send it off for display!
	XrFrameEndInfo end_info{ XR_TYPE_FRAME_END_INFO };
	end_info.displayTime          = frame_state.predictedDisplayTime;
	end_info.environmentBlendMode = xr_blend;
	end_info.layerCount           = layer == nullptr ? 0 : 1;
	end_info.layers               = &layer;
	xrEndFrame(xr_session, &end_info);
}

///////////////////////////////////////////

bool openxr_render_layer(XrTime predictedTime, vector<XrCompositionLayerProjectionView> &views, XrCompositionLayerProjection &layer) {

	// Find the state and location of each viewpoint at the predicted time
	uint32_t         view_count  = 0;
	XrViewState      view_state  = { XR_TYPE_VIEW_STATE };
	XrViewLocateInfo locate_info = { XR_TYPE_VIEW_LOCATE_INFO };
	locate_info.viewConfigurationType = app_config_view;
	locate_info.displayTime           = predictedTime;
	locate_info.space                 = xr_app_space;
	xrLocateViews(xr_session, &locate_info, &view_state, (uint32_t)xr_views.size(), &view_count, xr_views.data());
	views.resize(view_count);

	skr_draw_begin();
	// And now we'll iterate through each viewpoint, and render it!
	for (uint32_t i = 0; i < view_count; i++) {

		// We need to ask which swapchain image to use for rendering! Which one will we get?
		// Who knows! It's up to the runtime to decide.
		uint32_t                    img_id = 0;
		XrSwapchainImageAcquireInfo acquire_info = { XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
		xrAcquireSwapchainImage(xr_swapchains[i].handle, &acquire_info, &img_id);

		// Wait until the image is available to render to. The compositor could still be
		// reading from it.
		XrSwapchainImageWaitInfo wait_info = { XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
		wait_info.timeout = XR_INFINITE_DURATION;
		xrWaitSwapchainImage(xr_swapchains[i].handle, &wait_info);

		// Set up our rendering information for the viewpoint we're using right now!
		views[i] = { XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW };
		views[i].pose = xr_views[i].pose;
		views[i].fov  = xr_views[i].fov;
		views[i].subImage.swapchain        = xr_swapchains[i].handle;
		views[i].subImage.imageRect.offset = { 0, 0 };
		views[i].subImage.imageRect.extent = { xr_swapchains[i].width, xr_swapchains[i].height };

		// Call the rendering callback with our view and swapchain info
		app_render_layer(views[i], xr_swapchains[i].surface_data[img_id]);

		// And tell OpenXR we're done with rendering to this one!
		XrSwapchainImageReleaseInfo release_info = { XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
		xrReleaseSwapchainImage(xr_swapchains[i].handle, &release_info);
	}

	layer.space     = xr_app_space;
	layer.viewCount = (uint32_t)views.size();
	layer.views     = views.data();
	return true;
}

///////////////////////////////////////////

void app_render_layer(XrCompositionLayerProjectionView &view, swapchain_surfdata_t &surface) {
	float clear_color[4] = { 0,0,0,1 };
	skr_set_render_target(clear_color, &surface.render_tex, &surface.depth_tex);

	hmm_quaternion head_orientation;
	memcpy(&head_orientation, &view.pose.orientation, sizeof(XrQuaternionf));
	hmm_vec3 head_pos;
	memcpy(&head_pos, &view.pose.position, sizeof(XrVector3f));
	
	hmm_mat4 view_mat = 
		HMM_QuaternionToMat4(HMM_InverseQuaternion(head_orientation)) *
		HMM_Translate(HMM_Vec3( -view.pose.position.x, -view.pose.position.y, -view.pose.position.z));
	hmm_mat4 proj_mat  = xr_projection(view.fov, 0.01f, 100.0f);
	hmm_mat4 view_proj = HMM_Transpose( proj_mat * view_mat );

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

	world = HMM_Translate({ 1.5f,0,0 }) * HMM_Rotate(0, { 1,0,0 });
	world = HMM_Transpose(world);
	memcpy(app_shader_data.world, &world, sizeof(float) * 16);
	skr_buffer_update(&app_shader_buffer, &app_shader_data, sizeof(app_shader_data));
	skr_mesh_set(&app_mesh2.mesh);
	skr_shader_program_set(&app_shader);
	skr_draw(0, app_mesh2.ind_count, 1);
}

///////////////////////////////////////////

void app_swapchain_destroy(swapchain_t &swapchain) {
	for (uint32_t i = 0; i < swapchain.surface_data.size(); i++) {
		skr_tex_destroy( &swapchain.surface_data[i].depth_tex  );
		skr_tex_destroy( &swapchain.surface_data[i].render_tex );
	}
}

///////////////////////////////////////////

hmm_mat4 xr_projection(XrFovf fov, float clip_near, float clip_far) {
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
}*/