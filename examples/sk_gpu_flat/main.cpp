#if defined(_WIN32)
#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__linux__)
#include <X11/Xlib.h>
#include <GL/glx.h>
Display *x_dpy;
#elif defined(__EMSCRIPTEN__)
#include <emscripten.h>
#include <emscripten/html5.h>
#define _countof(a) (sizeof(a)/sizeof(*(a)))
#endif
#include "../common/webxr.h"

// When using single file header like normal, do this
//#define SKG_OPENGL
//#define SKG_IMPL
//#include "../sk_gpu.h"

// For easier development
#include "../../src/sk_gpu_dev.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../common/app.h"
#ifndef __EMSCRIPTEN__
#define HANDMADE_MATH_NO_SSE
#endif
#define HANDMADE_MATH_IMPLEMENTATION
#include "../common/HandmadeMath.h"

///////////////////////////////////////////

void           *app_hwnd      = nullptr;
skg_swapchain_t app_swapchain = {};
bool            app_resize    = false;
int             app_width     = 0;
int             app_height    = 0;
int             app_resize_width  = 1280;
int             app_resize_height = 720;
const int       app_multisample = 8;
bool            app_run       = true;
bool            app_stereo    = false;
const char     *app_name      = "sk_gpu.h";

///////////////////////////////////////////

bool main_init    ();
void main_shutdown();
int  main_step    (double t, void *);
void main_step_stereo(void *userData, int, float[16], WebXRView *views);

///////////////////////////////////////////

int main() {
#if __EMSCRIPTEN__
	webxr_init(WEBXR_SESSION_MODE_IMMERSIVE_VR,
		main_step_stereo,
		[](void *user_data) { app_stereo = true; },
		[](void *user_data) { app_stereo = false; emscripten_request_animation_frame_loop(&main_step, 0); },
		[](void *user_data, int err) {printf("WebXR err: %d\n", err);},
		nullptr);
#endif
	if (!main_init())
		return -1;

#if __EMSCRIPTEN__
	emscripten_request_animation_frame_loop(&main_step, 0);
#else
	double t = 0;
	while (app_run) { main_step(t, nullptr); t += 16; }

	main_shutdown();
#endif

	return 1;
}

///////////////////////////////////////////

void resize_swapchain(int width, int height) {
	if (width == app_width && height == app_height)
		return;
	
	app_width  = width;
	app_height = height;

	skg_swapchain_resize(&app_swapchain, app_width, app_height);
}

///////////////////////////////////////////

bool main_init() {
#if defined(_WIN32)
	WNDCLASS wc = {}; 
	wc.lpfnWndProc = [](HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
		switch(message) {
		case WM_CLOSE: app_run = false; PostQuitMessage(0); break;
		case WM_SIZE: {
			app_resize_width  = (UINT)LOWORD(lParam);
			app_resize_height = (UINT)HIWORD(lParam);
			if (app_resize || wParam == SIZE_MAXIMIZED) {
				app_resize = false;
				resize_swapchain(app_resize_width, app_resize_height);
			}
		} return DefWindowProc(hWnd, message, wParam, lParam);
		case WM_SYSCOMMAND: {
			if (GET_SC_WPARAM(wParam) == SC_RESTORE)
				app_resize = true;
		} return DefWindowProc(hWnd, message, wParam, lParam); 
		case WM_EXITSIZEMOVE: {
			resize_swapchain(app_resize_width, app_resize_height);
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
		0, 0, app_resize_width, app_resize_height, 
		nullptr, nullptr, wc.hInstance, nullptr);

	if( !app_hwnd ) return false;

	RECT bounds;
	GetClientRect((HWND)app_hwnd, &bounds);
	app_resize_width  = bounds.right  - bounds.left;
	app_resize_height = bounds.bottom - bounds.top;
#elif defined(__linux__)
	x_dpy = XOpenDisplay(0);
	if (x_dpy == nullptr) return false;

	GLint fb_att[] = {
		GLX_DOUBLEBUFFER,  true,
		GLX_RED_SIZE,      8,
		GLX_GREEN_SIZE,    8,
		GLX_BLUE_SIZE,     8,
		GLX_ALPHA_SIZE,    8,
		GLX_DEPTH_SIZE,    16,
		GLX_RENDER_TYPE,   GLX_RGBA_BIT,
		GLX_DRAWABLE_TYPE, GLX_PBUFFER_BIT,
		GLX_X_RENDERABLE,  true,
		None
	};

	Window       x_root         = DefaultRootWindow(x_dpy);
	int          fbConfigNumber = 0;
	GLXFBConfig *x_fb_config    = glXChooseFBConfig       (x_dpy, 0, fb_att, &fbConfigNumber);
	XVisualInfo *x_vi           = glXGetVisualFromFBConfig(x_dpy, *x_fb_config);
	if (x_vi == nullptr) return false;

	Colormap             x_cmap = XCreateColormap(x_dpy, x_root, x_vi->visual, AllocNone);
	XSetWindowAttributes x_swa  = {};
	x_swa.colormap              = x_cmap;
	x_swa.event_mask            = ExposureMask | KeyPressMask;

	Window x_win = XCreateWindow(x_dpy, x_root, 0, 0, 1280, 720, 0, x_vi->depth, InputOutput, x_vi->visual, CWColormap | CWEventMask, &x_swa);

	XSizeHints *hints = XAllocSizeHints();
	if (hints != nullptr) {
		hints->flags      = PMinSize;
		hints->min_width  = 100;
		hints->min_height = 100;
		XSetWMNormalHints(x_dpy, x_win, hints);
		XSetWMSizeHints  (x_dpy, x_win, hints, PMinSize);
	}

	XMapWindow(x_dpy, x_win);
	XStoreName(x_dpy, x_win, app_name);

	skg_setup_xlib(x_dpy, x_vi, x_fb_config, &x_win);
	app_hwnd = (void *)x_win;
#endif

	skg_callback_log([](skg_log_ level, const char *text) { 
		if (level == 2) {
			printf("[%d] %s\n", level, text);
		} else {
			printf("[%d] %s\n", level, text);
		}
	});
	if (skg_init(app_name, nullptr) <= 0)
		return false;
	app_swapchain = skg_swapchain_create(app_hwnd, skg_tex_fmt_rgba32_linear, skg_tex_fmt_depth32, app_resize_width, app_resize_height);
	resize_swapchain(app_resize_width, app_resize_height);

	return app_init();
}

///////////////////////////////////////////

void main_shutdown() {
	app_shutdown();
	skg_shutdown();
}

///////////////////////////////////////////

int main_step(double t, void *) {
	if (app_stereo)
		return 0; // Cancel the emscripten animation loop

#if defined(_WIN32)
	MSG msg = {};
	while (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
#elif defined(__linux__)
	XEvent event;

	while (XPending(x_dpy)) {
		XNextEvent(x_dpy, &event);
	}
#endif

	skg_draw_begin();
	float clear_color[4] = { 0,0,0,1 };
	skg_swapchain_bind(&app_swapchain);
	skg_target_clear(true, clear_color);

	hmm_mat4 view = HMM_LookAt(
		HMM_Vec3(sinf((float)t*0.001f) * 5, 2, cosf((float)t*0.001f) * 5),
		HMM_Vec3(0, 0, 0),
		HMM_Vec3(0, 1, 0));
	hmm_mat4 proj = HMM_Perspective(90, app_swapchain.width / (float)app_swapchain.height, 0.01f, 1000);

	app_render((float)t, view, proj);

	skg_swapchain_present(&app_swapchain);
	return 1;
}

///////////////////////////////////////////

void main_step_stereo(void* userData, int, float[16], WebXRView* views) {
	int32_t old_viewport[4];
	skg_viewport_get(old_viewport);

	skg_draw_begin();

	static double frame = 0;
	frame += 0.016;
	for (size_t i = 0; i < 2; i++) {
		skg_viewport(views[i].viewport);
		
		hmm_mat4 view, proj;
		memcpy(&view, views[i].viewMatrix,       sizeof(hmm_mat4));
		memcpy(&proj, views[i].projectionMatrix, sizeof(hmm_mat4));
		skg_event_begin("app_render");
		app_render((float)frame, view, proj);
		skg_event_end();
	}

	skg_viewport(old_viewport);
}

///////////////////////////////////////////

#ifdef __EMSCRIPTEN__
EMSCRIPTEN_KEEPALIVE
extern "C" void start_xr() {
	webxr_request_session();
}
EMSCRIPTEN_KEEPALIVE
extern "C" void web_canvas_resize(int32_t width, int32_t height) {
	skg_swapchain_resize(&app_swapchain, width, height);
}
#endif