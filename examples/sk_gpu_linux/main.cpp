// Install pre-requisites:
//---------------------------
// sudo apt install clang
// sudo apt install libx11-dev
// sudo apt install libgles2-mesa-dev
// 
// Build the executable:
//---------------------------
// clang main.cpp -lX11 -lEGL -o sk_gpu.exe

#define SKG_IMPL
#include "../../sk_gpu.h"

#include <stdio.h>
#include <X11/X.h>
#include <X11/Xlib.h>

Display        *display;
Window          window;
skg_swapchain_t swapchain = {};

int main() {
    display = XOpenDisplay(nullptr);
    int32_t screen = DefaultScreen(display);
    Window  root   = RootWindow   (display, screen);
    window = XCreateSimpleWindow(display, root, 0, 0, 640, 400, 0, 0, 0);
    
    skg_callback_log([](skg_log_ level, const char *text) {
        printf("[%d] %s", level, text);
    });
    skg_init("Test!", nullptr);

    swapchain = skg_swapchain_create(&window, skg_tex_fmt_rgba32_linear, skg_tex_fmt_depth16, 640, 400);

    while(true) {
        skg_draw_begin();
        
        float clear_color[4] = {1, 0, 0, 1};
        skg_swapchain_bind(&swapchain, true, clear_color);

        skg_swapchain_present(&swapchain);
    }

    skg_shutdown();

    XCloseDisplay(display);
    return 0;
}