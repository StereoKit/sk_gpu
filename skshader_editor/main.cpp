#include "imgui/imgui.h"
#include "imgui_impl_skg.h"
#include "imgui_impl_win32.h"

#include <windows.h>
#include <tchar.h>
#include <stdio.h>

#include "../sk_gpu.h"
#include "TextEditor.h"

#include "window_preview.h"
#include "app_shader.h"
#include "../skshaderc/sksc.h"

skg_swapchain_t g_pSwapChain = {};

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

int main(int, char**) {
	// Create application window
	//ImGui_ImplWin32_EnableDpiAwareness();
	WNDCLASSEX wc = { sizeof(WNDCLASSEX), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, _T("ImGui Example"), NULL };
	::RegisterClassEx(&wc);
	HWND hwnd = ::CreateWindow(wc.lpszClassName, _T("SK Shader Editor"), WS_OVERLAPPEDWINDOW, 100, 100, 1280, 800, NULL, NULL, wc.hInstance, NULL);

	// Initialize Direct3D
	skg_callback_log([](skg_log_ level, const char *text) { 
		printf("[%d] %s\n", level, text); 
	});
	if (!skg_init("skg_imgui", nullptr)) {
		::UnregisterClass(wc.lpszClassName, wc.hInstance);
		return 1;
	}
	g_pSwapChain = skg_swapchain_create(hwnd, skg_tex_fmt_rgba32_linear, skg_tex_fmt_depth16, 1280, 800);

	// Show the window
	::ShowWindow(hwnd, SW_SHOWDEFAULT);
	::UpdateWindow(hwnd);

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; 
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	ImFontConfig config = ImFontConfig();
	config.OversampleV = 1;
	config.OversampleH = 1;
	ImFont* font = io.Fonts->AddFontFromFileTTF("CascadiaMono.ttf", 18.0f, &config);
	IM_ASSERT(font != NULL);

	// Setup Dear ImGui style
	ImGui::StyleColorsDark();

	// Setup Platform/Renderer backends
	ImGui_ImplWin32_Init(hwnd);
	ImGui_ImplSkg_Init();

	// Our state
	bool   show_demo_window    = false;
	bool   show_another_window = false;
	ImVec4 clear_color         = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

	app_shader_init();

	TextEditor editor;
	TextEditor::LanguageDefinition lang = TextEditor::LanguageDefinition::HLSL();
	editor.SetLanguageDefinition(lang);
	editor.SetImGuiChildIgnored (false);
	editor.SetShowWhitespaces   (false);

	editor.SetText(R"_(//--name = test
//--tex = white 

cbuffer SystemBuffer : register(b1) {
	float4x4 viewproj;
};

struct vsIn {
	float4 pos  : SV_POSITION;
	float3 norm : NORMAL;
	float2 uv   : TEXCOORD0;
	float4 color: COLOR0;
};
struct psIn {
	float4 pos   : SV_POSITION;
	float2 uv    : TEXCOORD0;
	float3 color : COLOR0;
};

Texture2D    tex         : register(t0);
SamplerState tex_sampler : register(s0);

psIn vs(vsIn input) {
	float3 light_dir = normalize(float3(1.5,2,1));

	psIn output;
	output.pos   = mul(input.pos, viewproj);
	output.color = saturate(dot(input.norm, light_dir)).xxx * input.color.rgb;
	output.uv    = input.uv;
	return output;
}
float4 ps(psIn input) : SV_TARGET {
	return float4(input.color, 1) * tex.Sample(tex_sampler, input.uv);
})_");

	// Main loop
	MSG msg;
	ZeroMemory(&msg, sizeof(msg));
	while (msg.message != WM_QUIT) {
		if (::PeekMessage(&msg, NULL, 0U, 0U, PM_REMOVE)) {
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
			continue;
		}

		// Start the Dear ImGui frame
		skg_draw_begin();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		ImGui::DockSpaceOverViewport();
		
		if (show_demo_window)
			ImGui::ShowDemoWindow(&show_demo_window);

		editor.Render("Editor");
		window_preview();
		app_shader_show_log();
		app_shader_show_meta();

		static int64_t timer = 0;
		static bool    dirty = true;
		FILETIME time;
		GetSystemTimeAsFileTime(&time);
		int64_t now = ((LONGLONG)time.dwLowDateTime + ((LONGLONG)(time.dwHighDateTime) << 32LL)) / 10000;
		if (editor.IsTextChanged()) {
			dirty = true;
			timer = now;
		}
		if (dirty && now - timer > 500) {
			dirty = false;
			app_shader_update_hlsl(editor.GetText().c_str());

			TextEditor::ErrorMarkers markers;
			for (size_t i = 0; i < sksc_log_count(); i++) {
				sksc_log_item_t item = sksc_log_get(i);
				if (item.level > 0 && item.line >= 0) {
					markers.emplace(item.line, item.text);
				}
			}
			editor.SetErrorMarkers(markers);
		}

		// Rendering
		ImGui::Render();
		skg_swapchain_bind(&g_pSwapChain, true, (float*)&clear_color);
		ImGui_ImplSkg_RenderDrawData(ImGui::GetDrawData());

		skg_swapchain_present(&g_pSwapChain);
	}

	// Cleanup
	ImGui_ImplSkg_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	skg_swapchain_destroy(&g_pSwapChain);
	skg_shutdown();
	::DestroyWindow(hwnd);
	::UnregisterClass(wc.lpszClassName, wc.hInstance);

	return 0;
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
		return true;

	switch (msg)
	{
	case WM_SIZE:
		if (wParam != SIZE_MINIMIZED)
			skg_swapchain_resize(&g_pSwapChain, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam));
		return 0;
	case WM_SYSCOMMAND:
		if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
			return 0;
		break;
	case WM_DESTROY:
		::PostQuitMessage(0);
		return 0;
	}
	return ::DefWindowProc(hWnd, msg, wParam, lParam);
}
