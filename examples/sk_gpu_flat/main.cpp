
#ifndef __EMSCRIPTEN__
#define VC_EXTRALEAN
#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>

HWND app_hwnd;
#else
#include <emscripten.h>
#include <emscripten/html5.h>
#define _countof(a) (sizeof(a)/sizeof(*(a)))
#endif

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

skg_swapchain_t app_swapchain = {};
int             app_width     = 1280;
int             app_height    = 720;
bool            app_run       = true;
const char     *app_name      = "sk_gpu.h";

///////////////////////////////////////////

bool main_init    ();
void main_shutdown();
int  main_step    (double t, void *);

///////////////////////////////////////////

int main() {
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

bool main_init() {
	skg_callback_log([](skg_log_ level, const char *text) { printf("[%d] %s\n", level, text); });
#ifdef __EMSCRIPTEN__
	if (!skg_init(app_name, nullptr, nullptr)) return false;
#else
	WNDCLASS wc = {}; 
	wc.lpfnWndProc = [](HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
		switch(message) {
		case WM_CLOSE: app_run = false; PostQuitMessage(0); break;
		case WM_SIZE: {
			app_width  = (UINT)LOWORD(lParam);
			app_height = (UINT)HIWORD(lParam);
			skg_swapchain_resize(&app_swapchain, app_width, app_height);
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
	if (!skg_init(app_name, app_hwnd, nullptr)) return false;
#endif
	app_swapchain = skg_swapchain_create(skg_tex_fmt_rgba32_linear, skg_tex_fmt_depth32, app_width, app_height);

	return app_init();
}

///////////////////////////////////////////

void main_shutdown() {
	app_shutdown();
	skg_shutdown();
}

///////////////////////////////////////////

int main_step(double t, void *) {
#ifndef __EMSCRIPTEN__
	MSG msg = {};
	if (PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
#endif

	skg_draw_begin();
	float clear_color[4] = { 0,0,0,1 };
	skg_tex_t *target = skg_swapchain_get_next(&app_swapchain);
	skg_tex_target_bind(target, true, clear_color);

	hmm_mat4 view = HMM_LookAt(
		HMM_Vec3(sinf(t*0.001) * 5, 3, cosf(t*0.001) * 5),
		HMM_Vec3(0, 0, 0),
		HMM_Vec3(0, 1, 0));
	hmm_mat4 proj = HMM_Perspective(90, app_swapchain.width / (float)app_swapchain.height, 0.01f, 1000);

	app_render(t, view, proj);

	skg_swapchain_present(&app_swapchain);
	return 1;
}

///////////////////////////////////////////
