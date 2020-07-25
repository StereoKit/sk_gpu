#include "../../src/sk_gpu_dev.h"

#include <malloc.h>

#include "../../app.h"
#define HANDMADE_MATH_IMPLEMENTATION
#include "../../HandmadeMath.h"

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "AndroidProject1.NativeActivity", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "AndroidProject1.NativeActivity", __VA_ARGS__))
#define _countof(a) (sizeof(a)/sizeof(*(a)))

///////////////////////////////////////////

struct saved_state {
};

struct engine {
	struct android_app* app;

	bool initialized;
	skr_swapchain_t swapchain;
	int animating;
	struct saved_state state;
};

static int engine_init_display(struct engine* engine) {
	if (engine->initialized)
		return 0;
	skr_log_callback([](const char *text) { __android_log_print(ANDROID_LOG_INFO, "sk_gpu", text); });
	int result = skr_init("skr_gpu.h", engine->app->window, nullptr);
	if (!result)
	return result;

	engine->initialized = true;
	engine->swapchain   = skr_swapchain_create(skr_tex_fmt_rgba32_linear, skr_tex_fmt_depth32, 1280, 720);

	return app_init() ? 0 : -1;
}

static void engine_draw_frame(struct engine* engine) {
	if (!engine->initialized)
		return;

	skr_draw_begin();
	float clear_color[4] = { 0,1,0,1 };
	const skr_tex_t *target, *depth;
	skr_swapchain_get_next(&engine->swapchain, &target, &depth);
	skr_set_render_target(clear_color, target, depth);

	static int32_t frame = 0;
	frame++;

	hmm_mat4 view = HMM_LookAt(
		HMM_Vec3(sinf(frame / 30.0f) * 5, 3, cosf(frame / 30.0f) * 5),
		HMM_Vec3(0, 0, 0),
		HMM_Vec3(0, 1, 0));
	hmm_mat4 proj = HMM_Perspective(90, 720 / (float)1280.0f, 0.01f, 1000);

	app_render(view, proj);

	skr_swapchain_present(&engine->swapchain);
}

static void engine_term_display(struct engine* engine) {
	//skr_shutdown();
}

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
		// The window is being shown, get it ready.
		if (engine->app->window != NULL) {
			engine_init_display(engine);
			engine_draw_frame(engine);
		}
		break;
	case APP_CMD_TERM_WINDOW: engine_term_display(engine); break;
	case APP_CMD_GAINED_FOCUS: break;
	case APP_CMD_LOST_FOCUS:
		// Also stop animating.
		engine->animating = 0;
		engine_draw_frame(engine);
		break;
	}
}

void android_main(struct android_app* state) {
	struct engine engine;

	memset(&engine, 0, sizeof(engine));
	state->userData = &engine;
	state->onAppCmd = engine_handle_cmd;
	state->onInputEvent = engine_handle_input;
	engine.app = state;

	if (state->savedState != NULL) {
		// We are starting with a previous saved state; restore from it.
		engine.state = *(struct saved_state*)state->savedState;
	}

	engine.animating = 1;
	while (1) {
		// Read all pending events.
		int ident;
		int events;
		struct android_poll_source* source;

		while (ALooper_pollAll(engine.animating ? 0 : -1, NULL, &events, (void**)&source) >= 0) {

			if (source != NULL) source->process(state, source);
			
			if (state->destroyRequested != 0) {
				engine_term_display(&engine);
				return;
			}
		}

		if (engine.animating) {
			engine_draw_frame(&engine);
		}
	}
}

///////////////////////////////////////////