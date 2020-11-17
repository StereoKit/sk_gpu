//#define XR

#include "../../../src/sk_gpu_dev.h"

#include <malloc.h>

#define XR_APP_IMPL
#include "../../common/xr_app.h"
#include "../../common/app.h"
#define HANDMADE_MATH_IMPLEMENTATION
#include "../../common/HandmadeMath.h"

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "sk_gpu.h", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "sk_gpu.h", __VA_ARGS__))
#define _countof(a) (sizeof(a)/sizeof(*(a)))

///////////////////////////////////////////

struct saved_state {
};

struct swapchain_surfdata_t {
	skg_tex_t depth_tex;
	skg_tex_t render_tex;
};

struct app_swapchain_t {
	int32_t view_count;
	int32_t surf_count;
	swapchain_surfdata_t *surfaces;
};

struct engine {
	struct android_app* app;

	xr_settings_t xr_functions;

	bool swapchain_created;
#ifdef XR
	app_swapchain_t swapchain;
#else
	skg_swapchain_t swapchain;
#endif
	int animating;
	struct saved_state state;
};

///////////////////////////////////////////

// See: http://www.50ply.com/blog/2013/01/19/loading-compressed-android-assets-with-file-pointer/
AAssetManager *android_asset_manager;
bool android_fopen(const char *filename, void **out_data, size_t *out_size) {
	AAsset *asset = AAssetManager_open(android_asset_manager, filename, 0);
	if (!asset) 
		return false;

	FILE *fp = funopen(asset, 
		[](void *cookie, char *buf, int size) {return AAsset_read((AAsset *)cookie, buf, size); }, 
		[](void *cookie, const char *buf, int size) {return EACCES; }, 
		[](void *cookie, fpos_t offset, int whence) {return AAsset_seek((AAsset *)cookie, offset, whence); }, 
		[](void *cookie) { AAsset_close((AAsset *)cookie); return 0; } );

	if (fp == nullptr)
		return false;

	fseek(fp, 0L, SEEK_END);
	*out_size = ftell(fp);
	rewind(fp);

	*out_data = malloc(*out_size);
	if (*out_data == nullptr) { *out_size = 0; fclose(fp); return false; }
	fread (*out_data, *out_size, 1, fp);
	fclose(fp);

	return true;
}

///////////////////////////////////////////

void android_log_callback(skg_log_ level, const char *text) {
	int priority = ANDROID_LOG_INFO;
	switch (level) {
	case skg_log_info:     priority = ANDROID_LOG_INFO;  break;
	case skg_log_warning:  priority = ANDROID_LOG_WARN;  break;
	case skg_log_critical: priority = ANDROID_LOG_ERROR; break;
	}
	__android_log_write(priority, "sk_gpu", text); 
}

///////////////////////////////////////////

#ifndef XR
static int engine_init_display(struct engine* engine) {
	if (engine->swapchain_created)
		skg_swapchain_destroy(&engine->swapchain);

	int32_t width  = ANativeWindow_getWidth (engine->app->window);
	int32_t height = ANativeWindow_getHeight(engine->app->window);
	if (AConfiguration_getOrientation(engine->app->config) == ACONFIGURATION_ORIENTATION_PORT) {
		int32_t tmp = width;
		width  = height;
		height = tmp;
	}

	engine->swapchain_created = true;
	engine->swapchain         = skg_swapchain_create(engine->app->window, skg_tex_fmt_rgba32_linear, skg_tex_fmt_depth32, width, height);
	LOGI("Created swapchain");

	return 1;
}

void engine_resize_display(struct engine* engine) {
	if (engine->swapchain_created) {
		int32_t width  = ANativeWindow_getWidth (engine->app->window);
		int32_t height = ANativeWindow_getHeight(engine->app->window);
		if (AConfiguration_getOrientation(engine->app->config) == ACONFIGURATION_ORIENTATION_PORT) {
			int32_t tmp = width;
			width = height;
			height = tmp;
		}

		LOGI("Got a resize message! %dx%d", width, height);
		skg_swapchain_resize(&engine->swapchain, width, height);
	}
}

static void engine_draw_frame(struct engine* engine) {
	if (!engine->swapchain_created)
		return;

	skg_draw_begin();
	float clear_color[4] = { 0,0,0,1 };
	skg_swapchain_bind(&engine->swapchain, true, clear_color);

	static int32_t frame = 0;
	frame++;

	hmm_mat4 view = HMM_LookAt(
		HMM_Vec3(sinf(frame / 30.0f) * 5, 3, cosf(frame / 30.0f) * 5),
		HMM_Vec3(0, 0, 0),
		HMM_Vec3(0, 1, 0));
	hmm_mat4 proj = HMM_Perspective(90, engine->swapchain.width / (float)engine->swapchain.height, 0.01f, 1000);

	app_render(frame * 0.016f, view, proj);

	skg_swapchain_present(&engine->swapchain);
}

static void engine_term_display(struct engine* engine) {
	if (engine->swapchain_created)
		skg_swapchain_destroy(&engine->swapchain);
	engine->swapchain_created = false;

	LOGI("Destroying swapchain");
}

#else

bool main_init_gfx(void *user_data, const XrGraphicsRequirements *requirements, XrGraphicsBinding *out_graphics) {
	engine *eng = (engine*)user_data;
	
	skg_platform_data_t platform = skg_get_platform_data();
#if defined(SKG_OPENGL) && defined(_WIN32)
	out_graphics->hDC     = (HDC  )platform._gl_hdc;
	out_graphics->hGLRC   = (HGLRC)platform._gl_hrc;
#elif defined(SKG_OPENGL) && defined(__ANDROID__)
	out_graphics->display = (EGLDisplay)platform._egl_display;
	out_graphics->config  = (EGLConfig )platform._egl_config;
	out_graphics->context = (EGLContext)platform._egl_context;
#elif defined(SKG_DIRECT3D11)
	out_graphics->device  = (ID3D11Device*)platform._d3d11_device;
#endif

	return true;
}

///////////////////////////////////////////

bool main_init_swapchain(void *user_data, int32_t view_count, int32_t surface_count, void **textures, int32_t width, int32_t height, int64_t fmt) {
	engine *eng = (engine*)user_data;
	eng->swapchain.view_count = view_count;
	eng->swapchain.surf_count = surface_count;
	eng->swapchain.surfaces   = (swapchain_surfdata_t*)malloc(sizeof(swapchain_surfdata_t) * view_count * surface_count);
	skg_tex_fmt_ skg_format = skg_tex_fmt_from_native(fmt);

	for (int32_t i = 0; i < view_count*surface_count; i++) {
		eng->swapchain.surfaces[i].render_tex = skg_tex_create_from_existing(textures[i], skg_tex_type_rendertarget, skg_format, width, height, surface_count);
		eng->swapchain.surfaces[i].depth_tex  = skg_tex_create(skg_tex_type_depth, skg_use_static, skg_tex_fmt_depth32, skg_mip_none);
		skg_tex_set_contents(&eng->swapchain.surfaces[i].depth_tex, nullptr, 1, width, height);
		skg_tex_attach_depth(&eng->swapchain.surfaces[i].render_tex, &eng->swapchain.surfaces[i].depth_tex);
	}
	return true;
}

///////////////////////////////////////////

void main_destroy_swapchain(void *user_data) {
	engine *eng = (engine*)user_data;
	for (int32_t i = 0; i < eng->swapchain.surf_count * eng->swapchain.view_count; i++) {
		skg_tex_destroy(&eng->swapchain.surfaces[i].render_tex);
		skg_tex_destroy(&eng->swapchain.surfaces[i].depth_tex);
	}
	free(eng->swapchain.surfaces);
	eng->swapchain = {};
}

///////////////////////////////////////////

void main_render(void *user_data, const XrCompositionLayerProjectionView *view, int32_t view_id, int32_t surf_id) {
	engine *eng = (engine*)user_data;
	float clear_color[4] = { 0,0,0,1 };
	skg_tex_t *target = &eng->swapchain.surfaces[view_id * eng->swapchain.surf_count + surf_id].render_tex;
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

static int engine_init_display(struct engine* engine) {
	engine->xr_functions.user_data = engine;

	if (engine->swapchain_created)
		return 1;

	engine->xr_functions.android_activity = engine->app->activity->clazz;
	engine->xr_functions.android_vm       = engine->app->activity->vm;
	engine->xr_functions.init_gfx           = main_init_gfx;
	engine->xr_functions.init_swapchain     = main_init_swapchain;
	engine->xr_functions.destroy_swapchain  = main_destroy_swapchain;
	engine->xr_functions.draw               = main_render;
	engine->xr_functions.pixel_formats      = (int64_t*)malloc(sizeof(int64_t) * 4);
	engine->xr_functions.pixel_format_count = 4;
	engine->xr_functions.depth_formats      = (int64_t*)malloc(sizeof(int64_t) * 3);
	engine->xr_functions.depth_format_count = 3;
	engine->xr_functions.pixel_formats[0]   = skg_tex_fmt_to_native(skg_tex_fmt_rgba32_linear);
	engine->xr_functions.pixel_formats[1]   = skg_tex_fmt_to_native(skg_tex_fmt_bgra32_linear);
	engine->xr_functions.pixel_formats[2]   = skg_tex_fmt_to_native(skg_tex_fmt_rgba32);
	engine->xr_functions.pixel_formats[3]   = skg_tex_fmt_to_native(skg_tex_fmt_bgra32);
	engine->xr_functions.depth_formats[0]   = skg_tex_fmt_to_native(skg_tex_fmt_depth16);
	engine->xr_functions.depth_formats[1]   = skg_tex_fmt_to_native(skg_tex_fmt_depth32);
	engine->xr_functions.depth_formats[2]   = skg_tex_fmt_to_native(skg_tex_fmt_depthstencil);

	LOGI("Initializing OpenXR");
	openxr_log_callback([](const char *text) { __android_log_write(ANDROID_LOG_INFO, "sk_gpu", text); });
	if (openxr_init("sk_gpu.h", &engine->xr_functions)) {
		LOGI("...success!");
	} else {
		LOGI("...failed");
		return false;
	}

	engine->swapchain_created = true;

	return 1;
}

void engine_resize_display(struct engine* engine) {
}

static void engine_draw_frame(struct engine* engine) {
	if (!engine->swapchain_created)
		return;

	openxr_step();
}

static void engine_term_display(struct engine* engine) {
	if (engine->swapchain_created)
		openxr_shutdown();
	engine->swapchain_created = false;

	LOGI("Destroying swapchain");
}

#endif

static int32_t engine_handle_input(struct android_app* app, AInputEvent* event) {
	return 0;
}

static void engine_handle_cmd(struct android_app* app, int32_t cmd) {
	struct engine* engine = (struct engine*)app->userData;
	switch (cmd) {
	case APP_CMD_SAVE_STATE:
		// The system has asked us to save our current state.  Do so.
		engine->app->savedState = malloc(sizeof(struct saved_state));
		*((struct saved_state*)engine->app->savedState) = engine->state;
		engine->app->savedStateSize = sizeof(struct saved_state);
		break;
	case APP_CMD_INIT_WINDOW:
		LOGI("cmd: init");
		// The window is being shown, get it ready.
		if (engine->app->window != NULL) {
			if (engine_init_display(engine))
				engine_draw_frame(engine);
			else
				app->destroyRequested = 1;
		}
		break;
	case APP_CMD_TERM_WINDOW:  LOGI("cmd: term"); engine_term_display(engine); break;
	case APP_CMD_WINDOW_RESIZED: engine_resize_display(engine); break;
	case APP_CMD_CONFIG_CHANGED: engine_resize_display(engine); break;
	case APP_CMD_GAINED_FOCUS: LOGI("cmd: focused"); break;
	case APP_CMD_LOST_FOCUS:   LOGI("cmd: unfocused"); break;
	}
}

void android_main(struct android_app* state) {
	struct engine engine;

	memset(&engine, 0, sizeof(engine));
	state->userData = &engine;
	state->onAppCmd = engine_handle_cmd;
	state->onInputEvent = engine_handle_input;
	engine.app = state;

	android_asset_manager = state->activity->assetManager;

	if (state->savedState != NULL) {
		// We are starting with a previous saved state; restore from it.
		engine.state = *(struct saved_state*)state->savedState;
	}

	skg_callback_file_read(android_fopen);
	skg_callback_log      (android_log_callback);
	int result = skg_init("skg_gpu.h", nullptr);
	if (!result) return;
	if (!app_init()) return;

	engine.animating = 1;
	bool run = true;
	while (run) {
		// Read all pending events.
		int events;
		struct android_poll_source* source;
		while (ALooper_pollAll(engine.animating ? 0 : -1, NULL, &events, (void**)&source) >= 0) {
			if (source                  != NULL) source->process(state, source);
			if (state->destroyRequested != 0   ) run = false;
		}

		if (engine.animating) {
			engine_draw_frame(&engine);
		}
	}

	engine_term_display(&engine);

	app_shutdown();
	skg_shutdown();
}

///////////////////////////////////////////