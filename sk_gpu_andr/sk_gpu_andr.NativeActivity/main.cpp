#include "../../src/sk_gpu_dev.h"

#include <malloc.h>
//#include <math.h>

#include "../../shaders.h"
/*#define sinf(x) ((float)sin(x))
#define cosf(x) ((float)cos(x))
#define sqrtf(x) ((float)sqrt(x))
#define acosf(x) ((float)acos(x))
#define tanf(x) ((float)tan(x))*/
#define HANDMADE_MATH_IMPLEMENTATION
#include "../../HandmadeMath.h"

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "AndroidProject1.NativeActivity", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "AndroidProject1.NativeActivity", __VA_ARGS__))
#define _countof(a) (sizeof(a)/sizeof(*(a)))

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

app_shader_data_t    app_shader_data = {};
app_mesh_t           app_mesh1  = {};
app_mesh_t           app_mesh2  = {};
skr_shader_stage_t   app_ps     = {};
skr_shader_stage_t   app_vs     = {};
skr_shader_t         app_shader = {};
skr_buffer_t         app_shader_buffer = {};
skr_tex_t            app_tex = {};

struct saved_state {
	float angle;
	int32_t x;
	int32_t y;
};
struct engine {
	struct android_app* app;

	ASensorManager* sensorManager;
	const ASensor* accelerometerSensor;
	ASensorEventQueue* sensorEventQueue;

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
	engine->swapchain = skr_swapchain_create(skr_tex_fmt_rgba32_linear, skr_tex_fmt_depth32, 1280, 720);

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
	
	return 0;
}

/**
* Just the current frame in the display.
*/
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
	hmm_mat4 view_proj = HMM_Transpose( proj * view );

	hmm_mat4 world = HMM_Translate({ -1.5f,0,0 });
	world = HMM_Transpose(world);
	memcpy(app_shader_data.world,     &world,     sizeof(float) * 16);
	memcpy(app_shader_data.view_proj, &view_proj, sizeof(float) * 16);
	skr_buffer_update(&app_shader_buffer, &app_shader_data, sizeof(app_shader_data));

	skr_shader_set(&app_shader);
	skr_buffer_set(&app_shader_buffer, 0, sizeof(app_shader_data_t), 0);
	skr_mesh_set(&app_mesh1.mesh);
	skr_tex_set_active(&app_tex, 0);
	skr_draw(0, app_mesh1.ind_count, 1);

	world = HMM_Translate({ 1.5f,0,0 }) * HMM_Rotate(frame, { 1,0,0 });
	world = HMM_Transpose(world);
	memcpy(app_shader_data.world, &world, sizeof(float) * 16);
	skr_buffer_update(&app_shader_buffer, &app_shader_data, sizeof(app_shader_data));
	skr_mesh_set(&app_mesh2.mesh);
	skr_draw(0, app_mesh2.ind_count, 1);

	skr_swapchain_present(&engine->swapchain);
}

/**
* Tear down the EGL context currently associated with the display.
*/
static void engine_term_display(struct engine* engine) {
	//skr_shutdown();
}

/**
* Process the next input event.
*/
static int32_t engine_handle_input(struct android_app* app, AInputEvent* event) {
	struct engine* engine = (struct engine*)app->userData;
	if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
		engine->state.x = AMotionEvent_getX(event, 0);
		engine->state.y = AMotionEvent_getY(event, 0);
		return 1;
	}
	return 0;
}

/**
* Process the next main command.
*/
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
	case APP_CMD_TERM_WINDOW:
		// The window is being hidden or closed, clean it up.
		engine_term_display(engine);
		break;
	case APP_CMD_GAINED_FOCUS:
		// When our app gains focus, we start monitoring the accelerometer.
		if (engine->accelerometerSensor != NULL) {
			ASensorEventQueue_enableSensor(engine->sensorEventQueue,
				engine->accelerometerSensor);
			// We'd like to get 60 events per second (in microseconds).
			ASensorEventQueue_setEventRate(engine->sensorEventQueue,
				engine->accelerometerSensor, (1000L / 60) * 1000);
		}
		break;
	case APP_CMD_LOST_FOCUS:
		// When our app loses focus, we stop monitoring the accelerometer.
		// This is to avoid consuming battery while not being used.
		if (engine->accelerometerSensor != NULL) {
			ASensorEventQueue_disableSensor(engine->sensorEventQueue,
				engine->accelerometerSensor);
		}
		// Also stop animating.
		engine->animating = 0;
		engine_draw_frame(engine);
		break;
	}
}

/**
* This is the main entry point of a native application that is using
* android_native_app_glue.  It runs in its own thread, with its own
* event loop for receiving input events and doing other things.
*/
void android_main(struct android_app* state) {
	struct engine engine;

	memset(&engine, 0, sizeof(engine));
	state->userData = &engine;
	state->onAppCmd = engine_handle_cmd;
	state->onInputEvent = engine_handle_input;
	engine.app = state;

	// Prepare to monitor accelerometer
	engine.sensorManager = ASensorManager_getInstance();
	engine.accelerometerSensor = ASensorManager_getDefaultSensor(engine.sensorManager,
		ASENSOR_TYPE_ACCELEROMETER);
	engine.sensorEventQueue = ASensorManager_createEventQueue(engine.sensorManager,
		state->looper, LOOPER_ID_USER, NULL, NULL);

	if (state->savedState != NULL) {
		// We are starting with a previous saved state; restore from it.
		engine.state = *(struct saved_state*)state->savedState;
	}

	engine.animating = 1;

	// loop waiting for stuff to do.

	while (1) {
		// Read all pending events.
		int ident;
		int events;
		struct android_poll_source* source;

		// If not animating, we will block forever waiting for events.
		// If animating, we loop until all events are read, then continue
		// to draw the next frame of animation.
		while ((ident = ALooper_pollAll(engine.animating ? 0 : -1, NULL, &events,
			(void**)&source)) >= 0) {

			// Process this event.
			if (source != NULL) {
				source->process(state, source);
			}

			// If a sensor has data, process it now.
			if (ident == LOOPER_ID_USER) {
				if (engine.accelerometerSensor != NULL) {
					ASensorEvent event;
					while (ASensorEventQueue_getEvents(engine.sensorEventQueue,
						&event, 1) > 0) {
						/*LOGI("accelerometer: x=%f y=%f z=%f",
							event.acceleration.x, event.acceleration.y,
							event.acceleration.z);*/
					}
				}
			}

			// Check if we are exiting.
			if (state->destroyRequested != 0) {
				engine_term_display(&engine);
				return;
			}
		}

		if (engine.animating) {
			// Done with events; draw next animation frame.
			engine.state.angle += .01f;
			if (engine.state.angle > 1) {
				engine.state.angle = 0;
			}

			// Drawing is throttled to the screen update rate, so there
			// is no need to do timing here.
			engine_draw_frame(&engine);
		}
	}
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
