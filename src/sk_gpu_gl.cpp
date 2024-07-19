#include "sk_gpu_dev.h"
#ifdef SKG_OPENGL
///////////////////////////////////////////
// OpenGL Implementation                 //
///////////////////////////////////////////

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

///////////////////////////////////////////

#if   defined(_SKG_GL_LOAD_EMSCRIPTEN)
	#include <emscripten.h>
	#include <emscripten/html5.h>
	#include <GLES3/gl32.h>
#elif defined(_SKG_GL_LOAD_EGL)
	#include <EGL/egl.h>
	#include <EGL/eglext.h>
	#if defined(SKG_LINUX_EGL)
	#include <fcntl.h>
	#include <gbm.h>
	bool       egl_dri     = false;
	#endif

	EGLDisplay egl_display = EGL_NO_DISPLAY;
	EGLContext egl_context;
	EGLConfig  egl_config;
	EGLSurface egl_temp_surface;
#elif defined(_SKG_GL_LOAD_GLX)
	#include <X11/Xutil.h>
	#include <X11/Xlib.h>
	#include <dlfcn.h>

	Display     *xDisplay;
	XVisualInfo *visualInfo;
#elif defined(_SKG_GL_LOAD_WGL)
	#pragma comment(lib, "opengl32.lib")
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>

	HWND  gl_hwnd;
	HDC   gl_hdc;
	HGLRC gl_hrc;
#endif

///////////////////////////////////////////
// GL loader                             //
///////////////////////////////////////////

#ifdef _SKG_GL_LOAD_WGL
	#define WGL_DRAW_TO_WINDOW_ARB            0x2001
	#define WGL_SUPPORT_OPENGL_ARB            0x2010
	#define WGL_DOUBLE_BUFFER_ARB             0x2011
	#define WGL_ACCELERATION_ARB              0x2003
	#define WGL_FULL_ACCELERATION_ARB         0x2027
	#define WGL_PIXEL_TYPE_ARB                0x2013
	#define WGL_COLOR_BITS_ARB                0x2014
	#define WGL_DEPTH_BITS_ARB                0x2022
	#define WGL_STENCIL_BITS_ARB              0x2023
	#define WGL_TYPE_RGBA_ARB                 0x202B
	#define WGL_SAMPLE_BUFFERS_ARB            0x2041
	#define WGL_SAMPLES_ARB                   0x2042
	#define WGL_CONTEXT_MAJOR_VERSION_ARB     0x2091
	#define WGL_CONTEXT_MINOR_VERSION_ARB     0x2092
	#define WGL_CONTEXT_FLAGS_ARB             0x2094
	#define WGL_CONTEXT_PROFILE_MASK_ARB      0x9126
	#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB  0x00000001
	#define WGL_CONTEXT_DEBUG_BIT_ARB         0x0001

	typedef BOOL  (*wglChoosePixelFormatARB_proc)    (HDC hdc, const int *piAttribIList, const FLOAT *pfAttribFList, UINT nMaxFormats, int *piFormats, UINT *nNumFormats);
	typedef HGLRC (*wglCreateContextAttribsARB_proc) (HDC hDC, HGLRC hShareContext, const int *attribList);
	wglChoosePixelFormatARB_proc    wglChoosePixelFormatARB;
	wglCreateContextAttribsARB_proc wglCreateContextAttribsARB;
#endif

#ifdef _SKG_GL_LOAD_GLX
	#define GLX_RENDER_TYPE                  0x8011
	#define GLX_RGBA_TYPE                    0x8014
	#define GLX_CONTEXT_FLAGS_ARB            0x2094
	#define GLX_CONTEXT_DEBUG_BIT_ARB        0x0001
	#define GLX_CONTEXT_MAJOR_VERSION_ARB    0x2091
	#define GLX_CONTEXT_MINOR_VERSION_ARB    0x2092
	#define GLX_CONTEXT_PROFILE_MASK_ARB     0x9126
	#define GLX_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001

	typedef XID GLXDrawable;
	typedef struct __GLXFBConfig* GLXFBConfig;
	typedef struct __GLXcontext*  GLXContext;
	typedef void (*__GLXextproc)(void);

	typedef GLXContext   (*glXCreateContext_proc)          (Display* dpy, XVisualInfo* vis, GLXContext shareList, Bool direct);
	typedef GLXContext   (*glXCreateContextAttribsARB_proc)(Display* dpy, GLXFBConfig, GLXContext, Bool, const int*);
	typedef void         (*glXDestroyContext_proc)         (Display* dpy, GLXContext);
	typedef Bool         (*glXMakeCurrent_proc)            (Display* dpy, GLXDrawable, GLXContext);
	typedef void         (*glXSwapBuffers_proc)            (Display* dpy, GLXDrawable);
	typedef __GLXextproc (*glXGetProcAddress_proc)         (const char* procName);

	static glXCreateContext_proc           glXCreateContext;
	static glXCreateContextAttribsARB_proc glXCreateContextAttribsARB;
	static glXDestroyContext_proc          glXDestroyContext;
	static glXMakeCurrent_proc             glXMakeCurrent;
	static glXSwapBuffers_proc             glXSwapBuffers;
	static glXGetProcAddress_proc          glXGetProcAddress;

	static GLXFBConfig  glxFBConfig;
	static GLXDrawable  glxDrawable;
	static GLXContext   glxContext;
#endif

#ifdef _SKG_GL_MAKE_FUNCTIONS

#define GL_BLEND 0x0BE2
#define GL_SAMPLE_ALPHA_TO_COVERAGE 0x809E
#define GL_ZERO 0
#define GL_ONE  1
#define GL_SRC_COLOR                0x0300
#define GL_ONE_MINUS_SRC_COLOR      0x0301
#define GL_SRC_ALPHA                0x0302
#define GL_ONE_MINUS_SRC_ALPHA      0x0303
#define GL_DST_ALPHA                0x0304
#define GL_ONE_MINUS_DST_ALPHA      0x0305
#define GL_DST_COLOR                0x0306
#define GL_ONE_MINUS_DST_COLOR      0x0307
#define GL_CONSTANT_COLOR           0x8001
#define GL_ONE_MINUS_CONSTANT_COLOR 0x8002
#define GL_CONSTANT_ALPHA           0x8003
#define GL_ONE_MINUS_CONSTANT_ALPHA 0x8004
#define GL_FUNC_ADD                 0x8006
#define GL_MAX                      0x8008

#define GL_NEVER                    0x0200 
#define GL_LESS                     0x0201
#define GL_EQUAL                    0x0202
#define GL_LEQUAL                   0x0203
#define GL_GREATER                  0x0204
#define GL_NOTEQUAL                 0x0205
#define GL_GEQUAL                   0x0206
#define GL_ALWAYS                   0x0207

#define GL_INVALID_INDEX 0xFFFFFFFFu
#define GL_FRAMEBUFFER_SRGB 0x8DB9
#define GL_VIEWPORT         0x0BA2
#define GL_DEPTH_BUFFER_BIT 0x00000100
#define GL_COLOR_BUFFER_BIT 0x00004000
#define GL_STENCIL_BUFFER_BIT 0x400
#define GL_ARRAY_BUFFER 0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_UNIFORM_BUFFER 0x8A11
#define GL_SHADER_STORAGE_BUFFER 0x90D2
#define GL_STATIC_DRAW 0x88E4
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_READ_ONLY 0x88B8
#define GL_WRITE_ONLY 0x88B9
#define GL_READ_WRITE 0x88BA
#define GL_TRIANGLES 0x0004
#define GL_VERSION 0x1F02
#define GL_RENDERER 0x1F01
#define GL_CULL_FACE 0x0B44
#define GL_BACK 0x0405
#define GL_FRONT 0x0404
#define GL_FRONT_AND_BACK 0x0408
#define GL_LINE 0x1B01
#define GL_FILL 0x1B02
#define GL_DEPTH_TEST 0x0B71
#define GL_SCISSOR_TEST 0x0C11
#define GL_TEXTURE_2D 0x0DE1
#define GL_TEXTURE_2D_ARRAY 0x8C1A
#define GL_TEXTURE_2D_MULTISAMPLE 0x9100
#define GL_TEXTURE_2D_MULTISAMPLE_ARRAY 0x9102
#define GL_TEXTURE_CUBE_MAP 0x8513
#define GL_TEXTURE_CUBE_MAP_SEAMLESS 0x884F
#define GL_TEXTURE_CUBE_MAP_ARRAY 0x9009
#define GL_TEXTURE_BINDING_CUBE_MAP 0x8514
#define GL_TEXTURE_CUBE_MAP_POSITIVE_X 0x8515
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_X 0x8516
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Y 0x8517
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Y 0x8518
#define GL_TEXTURE_CUBE_MAP_POSITIVE_Z 0x8519
#define GL_TEXTURE_CUBE_MAP_NEGATIVE_Z 0x851A
#define GL_NEAREST 0x2600
#define GL_LINEAR 0x2601
#define GL_NEAREST_MIPMAP_NEAREST 0x2700
#define GL_LINEAR_MIPMAP_NEAREST 0x2701
#define GL_NEAREST_MIPMAP_LINEAR 0x2702
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_TEXTURE_MIN_LOD 0x813A
#define GL_TEXTURE_MAX_LOD 0x813B
#define GL_TEXTURE_BASE_LEVEL 0x813C
#define GL_TEXTURE_MAX_LEVEL 0x813D
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_WRAP_S 0x2802
#define GL_TEXTURE_WRAP_T 0x2803
#define GL_TEXTURE_WRAP_R 0x8072
#define GL_TEXTURE_WIDTH 0x1000
#define GL_TEXTURE_HEIGHT 0x1001
#define GL_TEXTURE_INTERNAL_FORMAT 0x1003
#define GL_REPEAT 0x2901
#define GL_CLAMP_TO_EDGE 0x812F
#define GL_MIRRORED_REPEAT 0x8370
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#define GL_TEXTURE0 0x84C0
#define GL_FRAMEBUFFER 0x8D40
#define GL_DRAW_FRAMEBUFFER_BINDING 0x8CA6
#define GL_READ_FRAMEBUFFER 0x8CA8
#define GL_DRAW_FRAMEBUFFER 0x8CA9
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_DEPTH_ATTACHMENT 0x8D00
#define GL_DEPTH_STENCIL_ATTACHMENT 0x821A
#define GL_BUFFER  0x82E0
#define GL_SHADER  0x82E1
#define GL_PROGRAM 0x82E2
#define GL_SAMPLER 0x82E6
#define GL_TEXTURE 0x1702

#define GL_RED 0x1903
#define GL_RGB 0x1907
#define GL_RGBA 0x1908
#define GL_SRGB_ALPHA 0x8C42
#define GL_DEPTH_COMPONENT 0x1902
#define GL_DEPTH_STENCIL 0x84F9
#define GL_R8_SNORM 0x8F94
#define GL_RG8_SNORM 0x8F95
#define GL_RGB8_SNORM 0x8F96
#define GL_RGBA8_SNORM 0x8F97
#define GL_R16_SNORM 0x8F98
#define GL_RG16_SNORM 0x8F99
#define GL_RGB16_SNORM 0x8F9A
#define GL_RGBA16_SNORM 0x8F9B
#define GL_RG 0x8227
#define GL_RG_INTEGER 0x8228
#define GL_R8 0x8229
#define GL_RG8 0x822B
#define GL_RG16 0x822C
#define GL_R16 0x822A
#define GL_R16F 0x822D
#define GL_R32F 0x822E
#define GL_RG16F 0x822F
#define GL_RG32F 0x8230
#define GL_R8I 0x8231
#define GL_R8UI 0x8232
#define GL_R16I 0x8233
#define GL_R16UI 0x8234
#define GL_R32I 0x8235
#define GL_R32UI 0x8236
#define GL_RG8I 0x8237
#define GL_RG8UI 0x8238
#define GL_RG16I 0x8239
#define GL_RG16UI 0x823A
#define GL_RG32I 0x823B
#define GL_RG32UI 0x823C
#define GL_RGBA8 0x8058
#define GL_RGBA16 0x805B
#define GL_BGRA 0x80E1
#define GL_SRGB8_ALPHA8 0x8C43
#define GL_R11F_G11F_B10F 0x8C3A
#define GL_RGB10_A2 0x8059
#define GL_RGBA32F 0x8814
#define GL_RGBA16F 0x881A
#define GL_RGBA16I 0x8D88
#define GL_RGBA16UI 0x8D76
#define GL_COMPRESSED_RGB8_ETC2 0x9274
#define GL_COMPRESSED_SRGB8_ETC2 0x9275
#define GL_COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2 0x9276
#define GL_COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2 0x9277
#define GL_COMPRESSED_RGBA8_ETC2_EAC 0x9278
#define GL_COMPRESSED_SRGB8_ALPHA8_ETC2_EAC 0x9279
#define GL_COMPRESSED_R11_EAC 0x9270
#define GL_COMPRESSED_SIGNED_R11_EAC 0x9271
#define GL_COMPRESSED_RG11_EAC 0x9272
#define GL_COMPRESSED_SIGNED_RG11_EAC 0x9273
#define GL_DEPTH_COMPONENT16 0x81A5
#define GL_DEPTH_COMPONENT32F 0x8CAC
#define GL_DEPTH24_STENCIL8 0x88F0
#define GL_BYTE 0x1400
#define GL_UNSIGNED_BYTE 0x1401
#define GL_SHORT 0x1402
#define GL_UNSIGNED_SHORT 0x1403
#define GL_INT 0x1404
#define GL_UNSIGNED_INT 0x1405
#define GL_UNSIGNED_INT_24_8 0x84FA
#define GL_FLOAT 0x1406
#define GL_HALF_FLOAT 0x140B
#define GL_DOUBLE 0x140A
#define GL_UNSIGNED_INT_8_8_8_8 0x8035
#define GL_UNSIGNED_INT_8_8_8_8_REV 0x8367
#define GL_MAX_SAMPLES 0x8D57
#define GL_PACK_ALIGNMENT 0x0D05
#define GL_UNPACK_ALIGNMENT 0x0CF5

#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPUTE_SHADER 0x91B9
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_INFO_LOG_LENGTH 0x8B84
#define GL_NUM_EXTENSIONS 0x821D
#define GL_EXTENSIONS 0x1F03

#define GL_DEBUG_OUTPUT                0x92E0
#define GL_DEBUG_OUTPUT_SYNCHRONOUS    0x8242
#define GL_DEBUG_SEVERITY_NOTIFICATION 0x826B
#define GL_DEBUG_SEVERITY_HIGH         0x9146
#define GL_DEBUG_SEVERITY_MEDIUM       0x9147
#define GL_DEBUG_SEVERITY_LOW          0x9148
#define GL_DEBUG_SOURCE_APPLICATION    0x824A

// Reference from here:
// https://github.com/ApoorvaJ/Papaya/blob/3808e39b0f45d4ca4972621c847586e4060c042a/src/libs/gl_lite.h

#ifdef _WIN32
#define GLDECL WINAPI
#else
#define GLDECL
#endif

typedef void (GLDECL *GLDEBUGPROC)(uint32_t source, uint32_t type, uint32_t id, uint32_t severity, int32_t length, const char* message, const void* userParam);

#define GL_API \
GLE(void,     glLinkProgram,             uint32_t program) \
GLE(void,     glClearColor,              float r, float g, float b, float a) \
GLE(void,     glClear,                   uint32_t mask) \
GLE(void,     glEnable,                  uint32_t cap) \
GLE(void,     glDisable,                 uint32_t cap) \
GLE(void,     glPolygonMode,             uint32_t face, uint32_t mode) \
GLE(void,     glDepthMask,               uint8_t flag) \
GLE(void,     glDepthFunc,               uint32_t func) \
GLE(uint32_t, glGetError,                ) \
GLE(void,     glGetProgramiv,            uint32_t program, uint32_t pname, int32_t *params) \
GLE(uint32_t, glCreateShader,            uint32_t type) \
GLE(void,     glShaderSource,            uint32_t shader, int32_t count, const char* const *string, const int32_t *length) \
GLE(void,     glCompileShader,           uint32_t shader) \
GLE(void,     glGetShaderiv,             uint32_t shader, uint32_t pname, int32_t *params) \
GLE(void,     glGetIntegerv,             uint32_t pname, int32_t *params) \
GLE(void,     glGetShaderInfoLog,        uint32_t shader, int32_t bufSize, int32_t *length, char *infoLog) \
GLE(void,     glGetProgramInfoLog,       uint32_t program, int32_t maxLength, int32_t *length, char *infoLog) \
GLE(void,     glDeleteShader,            uint32_t shader) \
GLE(uint32_t, glCreateProgram,           void) \
GLE(void,     glAttachShader,            uint32_t program, uint32_t shader) \
GLE(void,     glDetachShader,            uint32_t program, uint32_t shader) \
GLE(void,     glUseProgram,              uint32_t program) \
GLE(uint32_t, glGetUniformBlockIndex,    uint32_t program, const char *uniformBlockName) \
GLE(void,     glUniformBlockBinding,     uint32_t program, uint32_t uniformBlockIndex, uint32_t uniformBlockBinding) \
GLE(void,     glDeleteProgram,           uint32_t program) \
GLE(void,     glGenVertexArrays,         int32_t n, uint32_t *arrays) \
GLE(void,     glBindVertexArray,         uint32_t array) \
GLE(void,     glBufferData,              uint32_t target, int32_t size, const void *data, uint32_t usage) \
GLE(void,     glGenBuffers,              int32_t n, uint32_t *buffers) \
GLE(void,     glBindBuffer,              uint32_t target, uint32_t buffer) \
GLE(void,     glBindVertexBuffer,        uint32_t bindingindex, uint32_t buffer, size_t offset, uint32_t stride) \
GLE(void,     glDeleteBuffers,           int32_t n, const uint32_t *buffers) \
GLE(void,     glGenTextures,             int32_t n, uint32_t *textures) \
GLE(void,     glGenFramebuffers,         int32_t n, uint32_t *ids) \
GLE(void,     glDeleteFramebuffers,      int32_t n, uint32_t *ids) \
GLE(void,     glBindFramebuffer,         uint32_t target, uint32_t framebuffer) \
GLE(void,     glFramebufferTexture,      uint32_t target, uint32_t attachment, uint32_t texture, int32_t level) \
GLE(void,     glFramebufferTexture2D,    uint32_t target, uint32_t attachment, uint32_t textarget, uint32_t texture, int32_t level) \
GLE(void,     glFramebufferTextureLayer, uint32_t target, uint32_t attachment, uint32_t texture, int32_t level, int32_t layer) \
GLE(void,     glFramebufferTexture2DMultisampleEXT, uint32_t target, uint32_t attachment, uint32_t textarget, uint32_t texture, int32_t level, int32_t multisample) \
GLE(uint32_t, glCheckFramebufferStatus,  uint32_t target) \
GLE(void,     glBlitFramebuffer,         int32_t srcX0, int32_t srcY0, int32_t srcX1, int32_t srcY1, int32_t dstX0, int32_t dstY0, int32_t dstX1, int32_t dstY1, uint32_t mask, uint32_t filter) \
GLE(void,     glDeleteTextures,          int32_t n, const uint32_t *textures) \
GLE(void,     glBindTexture,             uint32_t target, uint32_t texture) \
GLE(void,     glBindImageTexture,        uint32_t unit, uint32_t texture, int32_t level, unsigned char layered, int32_t layer, uint32_t access, uint32_t format) \
GLE(void,     glTexParameteri,           uint32_t target, uint32_t pname, int32_t param) \
GLE(void,     glGetInternalformativ,     uint32_t target, uint32_t internalformat, uint32_t pname, int32_t bufSize, int32_t *params)\
GLE(void,     glGetTexLevelParameteriv,  uint32_t target, int32_t level, uint32_t pname, int32_t *params) \
GLE(void,     glTexParameterf,           uint32_t target, uint32_t pname, float param) \
GLE(void,     glTexImage2D,              uint32_t target, int32_t level, int32_t internalformat, int32_t width, int32_t height, int32_t border, uint32_t format, uint32_t type, const void *data) \
GLE(void,     glTexStorage2DMultisample, uint32_t target, uint32_t samples, int32_t internalformat, uint32_t width, uint32_t height, uint8_t fixedsamplelocations) \
GLE(void,     glTexStorage3DMultisample, uint32_t target, uint32_t samples, int32_t internalformat, uint32_t width, uint32_t height, uint32_t depth, uint8_t fixedsamplelocations) \
GLE(void,     glTexImage3D,              uint32_t target, int32_t level, int32_t internalformat, uint32_t width, uint32_t height, uint32_t depth, int32_t border, uint32_t format, uint32_t type, const void *data) \
GLE(void,     glCopyTexSubImage2D,       uint32_t target, int32_t level, int32_t xoffset, int32_t yoffset, int32_t x, int32_t y, uint32_t width, uint32_t height) \
GLE(void,     glGetTexImage,             uint32_t target, int32_t level, uint32_t format, uint32_t type, void *img) \
GLE(void,     glReadPixels,              int32_t x, int32_t y, uint32_t width, uint32_t height, uint32_t format, uint32_t type, void *data) \
GLE(void,     glPixelStorei,             uint32_t pname, int32_t param) \
GLE(void,     glActiveTexture,           uint32_t texture) \
GLE(void,     glGenerateMipmap,          uint32_t target) \
GLE(void,     glBindAttribLocation,      uint32_t program, uint32_t index, const char *name) \
GLE(int32_t,  glGetUniformLocation,      uint32_t program, const char *name) \
GLE(void,     glUniform4f,               int32_t location, float v0, float v1, float v2, float v3) \
GLE(void,     glUniform4fv,              int32_t location, int32_t count, const float *value) \
GLE(void,     glDeleteVertexArrays,      int32_t n, const uint32_t *arrays) \
GLE(void,     glEnableVertexAttribArray, uint32_t index) \
GLE(void,     glVertexAttribPointer,     uint32_t index, int32_t size, uint32_t type, uint8_t normalized, int32_t stride, const void *pointer) \
GLE(void,     glUniform1i,               int32_t location, int32_t v0) \
GLE(void,     glDrawElementsInstanced,   uint32_t mode, int32_t count, uint32_t type, const void *indices, int32_t primcount) \
GLE(void,     glDrawElementsInstancedBaseVertex,   uint32_t mode, int32_t count, uint32_t type, const void *indices, int32_t instancecount, int32_t basevertex) \
GLE(void,     glDrawElements,            uint32_t mode, int32_t count, uint32_t type, const void *indices) \
GLE(void,     glDebugMessageCallback,    GLDEBUGPROC callback, const void *userParam) \
GLE(void,     glBindBufferBase,          uint32_t target, uint32_t index, uint32_t buffer) \
GLE(void,     glBufferSubData,           uint32_t target, int64_t offset, int32_t size, const void *data) \
GLE(void,     glViewport,                int32_t x, int32_t y, uint32_t width, uint32_t height) \
GLE(void,     glScissor,                 int32_t x, int32_t y, uint32_t width, uint32_t height) \
GLE(void,     glCullFace,                uint32_t mode) \
GLE(void,     glBlendFunc,               uint32_t sfactor, uint32_t dfactor) \
GLE(void,     glBlendFuncSeparate,       uint32_t srcRGB, uint32_t dstRGB, uint32_t srcAlpha, uint32_t dstAlpha) \
GLE(void,     glBlendEquationSeparate,   uint32_t modeRGB, uint32_t modeAlpha) \
GLE(void,     glDispatchCompute,         uint32_t num_groups_x, uint32_t num_groups_y, uint32_t num_groups_z) \
GLE(void,     glObjectLabel,             uint32_t identifier, uint32_t name, uint32_t length, const char* label) \
GLE(void,     glPushDebugGroupKHR,       uint32_t source, uint32_t id, uint32_t length, const char* message) \
GLE(void,     glPopDebugGroupKHR,        void) \
GLE(const char *, glGetString,           uint32_t name) \
GLE(const char *, glGetStringi,          uint32_t name, uint32_t index)

#define GLE(ret, name, ...) typedef ret GLDECL name##_proc(__VA_ARGS__); static name##_proc * name;
GL_API
#undef GLE

#if defined(_SKG_GL_LOAD_WGL)
	// from https://www.khronos.org/opengl/wiki/Load_OpenGL_Functions
	// Some GL functions can only be loaded with wglGetProcAddress, and others
	// can only be loaded by GetProcAddress.
	void *gl_get_function(const char *name) {
		static HMODULE dll = LoadLibraryA("opengl32.dll");
		void *f = (void *)wglGetProcAddress(name);
		if (f == 0 || (f == (void*)0x1) || (f == (void*)0x2) || (f == (void*)0x3) || (f == (void*)-1) ) {
			f = (void *)GetProcAddress(dll, name);
		}
		return f;
	}
#elif defined(_SKG_GL_LOAD_EGL)
	#define gl_get_function(x) eglGetProcAddress(x)
#elif defined(_SKG_GL_LOAD_GLX)
	#define gl_get_function(x) glXGetProcAddress(x)
#endif

static void gl_load_extensions( ) {
#define GLE(ret, name, ...) name = (name##_proc *) gl_get_function(#name); if (name == nullptr) skg_log(skg_log_info, "Couldn't load gl function " #name);
	GL_API
#undef GLE
}

#endif // _SKG_GL_MAKE_FUNCTIONS

///////////////////////////////////////////

int32_t     gl_active_width        = 0;
int32_t     gl_active_height       = 0;
skg_tex_t  *gl_active_rendertarget = nullptr;
uint32_t    gl_current_framebuffer = 0;
char*       gl_adapter_name        = nullptr;

bool gl_caps[skg_cap_max] = {};

///////////////////////////////////////////

uint32_t skg_buffer_type_to_gl   (skg_buffer_type_ type);
uint32_t skg_tex_fmt_to_gl_type  (skg_tex_fmt_ format);
uint32_t skg_tex_fmt_to_gl_layout(skg_tex_fmt_ format);

///////////////////////////////////////////

// Some nice reference: https://gist.github.com/nickrolfe/1127313ed1dbf80254b614a721b3ee9c
int32_t gl_init_wgl() {
#ifdef _SKG_GL_LOAD_WGL
	///////////////////////////////////////////
	// Dummy initialization for pixel format //
	///////////////////////////////////////////

	WNDCLASSA dummy_class = { 0 };
	dummy_class.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	dummy_class.lpfnWndProc   = DefWindowProcA;
	dummy_class.hInstance     = GetModuleHandle(0);
	dummy_class.lpszClassName = "DummyGLWindow";
	if (!RegisterClassA(&dummy_class))
		return false;

	HWND dummy_window = CreateWindowExA(0, dummy_class.lpszClassName, "Dummy GL Window", 0, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, dummy_class.hInstance, 0);
	if (!dummy_window)
		return false;
	HDC dummy_dc = GetDC(dummy_window);

	PIXELFORMATDESCRIPTOR format_desc = { sizeof(PIXELFORMATDESCRIPTOR) };
	format_desc.nVersion     = 1;
	format_desc.iPixelType   = PFD_TYPE_RGBA;
	format_desc.dwFlags      = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	format_desc.cColorBits   = 32;
	format_desc.cAlphaBits   = 8;
	format_desc.iLayerType   = PFD_MAIN_PLANE;
	format_desc.cDepthBits   = 0;
	format_desc.cStencilBits = 0;

	int pixel_format = ChoosePixelFormat(dummy_dc, &format_desc);
	if (!pixel_format) {
		skg_log(skg_log_critical, "Failed to find a suitable pixel format.");
		return false;
	}
	if (!SetPixelFormat(dummy_dc, pixel_format, &format_desc)) {
		skg_log(skg_log_critical, "Failed to set the pixel format.");
		return false;
	}
	HGLRC dummy_context = wglCreateContext(dummy_dc);
	if (!dummy_context) {
		skg_log(skg_log_critical, "Failed to create a dummy OpenGL rendering context.");
		return false;
	}
	if (!wglMakeCurrent(dummy_dc, dummy_context)) {
		skg_log(skg_log_critical, "Failed to activate dummy OpenGL rendering context.");
		return false;
	}

	// Load the function pointers we need to actually initialize OpenGL
	// Function pointers we need to actually initialize OpenGL
	wglChoosePixelFormatARB    = (wglChoosePixelFormatARB_proc   )wglGetProcAddress("wglChoosePixelFormatARB");
	wglCreateContextAttribsARB = (wglCreateContextAttribsARB_proc)wglGetProcAddress("wglCreateContextAttribsARB");

	// Shut down the dummy so we can set up OpenGL for real
	wglMakeCurrent  (dummy_dc, 0);
	wglDeleteContext(dummy_context);
	ReleaseDC       (dummy_window, dummy_dc);
	DestroyWindow   (dummy_window);

	///////////////////////////////////////////
	// Real OpenGL initialization            //
	///////////////////////////////////////////

	WNDCLASSA win_class = { 0 };
	win_class.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	win_class.lpfnWndProc   = DefWindowProcA;
	win_class.hInstance     = GetModuleHandle(0);
	win_class.lpszClassName = "SKGPUWindow";
	if (!RegisterClassA(&win_class))
		return false;

	void *app_hwnd = CreateWindowExA(0, win_class.lpszClassName, "sk_gpu Window", 0, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, win_class.hInstance, 0);
	if (!app_hwnd)
		return false;

	gl_hwnd = (HWND)app_hwnd;
	gl_hdc  = GetDC(gl_hwnd);

	// Find a pixel format
	const int format_attribs[] = {
		WGL_DRAW_TO_WINDOW_ARB, true,
		WGL_SUPPORT_OPENGL_ARB, true,
		WGL_DOUBLE_BUFFER_ARB,  true,
		WGL_ACCELERATION_ARB,   WGL_FULL_ACCELERATION_ARB,
		WGL_PIXEL_TYPE_ARB,     WGL_TYPE_RGBA_ARB,
		WGL_COLOR_BITS_ARB,     32,
		WGL_DEPTH_BITS_ARB,     0,
		WGL_STENCIL_BITS_ARB,   0,
		WGL_SAMPLE_BUFFERS_ARB, 0,
		WGL_SAMPLES_ARB,        0,
		0 };

	pixel_format = 0;
	UINT num_formats = 0;
	if (!wglChoosePixelFormatARB(gl_hdc, format_attribs, nullptr, 1, &pixel_format, &num_formats)) {
		skg_log(skg_log_critical, "Couldn't find pixel format!");
		return false;
	}

	memset(&format_desc, 0, sizeof(format_desc));
	DescribePixelFormat(gl_hdc, pixel_format, sizeof(format_desc), &format_desc);
	if (!SetPixelFormat(gl_hdc, pixel_format, &format_desc)) {
		skg_log(skg_log_critical, "Couldn't set pixel format!");
		return false;
	}

	// Create an OpenGL context
	int attributes[] = {
		WGL_CONTEXT_MAJOR_VERSION_ARB, 3, 
		WGL_CONTEXT_MINOR_VERSION_ARB, 3,
#if !defined(NDEBUG)
		WGL_CONTEXT_FLAGS_ARB, WGL_CONTEXT_DEBUG_BIT_ARB,
#endif
		WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
		0 };
	gl_hrc = wglCreateContextAttribsARB( gl_hdc, 0, attributes );
	if (!gl_hrc) {
		skg_log(skg_log_critical, "Couldn't create GL context!");
		return false;
	}
	if (!wglMakeCurrent(gl_hdc, gl_hrc)) {
		skg_log(skg_log_critical, "Couldn't activate GL context!");
		return false;
	}
#endif // _SKG_GL_LOAD_WGL
	return 1;
}

///////////////////////////////////////////

int32_t gl_init_emscripten() {
	// Some reference code:
	// https://github.com/emscripten-core/emscripten/blob/master/tests/glbook/Common/esUtil.c
	// https://github.com/emscripten-core/emscripten/tree/master/tests/minimal_webgl
#ifdef _SKG_GL_LOAD_EMSCRIPTEN
	EmscriptenWebGLContextAttributes attrs;
	emscripten_webgl_init_context_attributes(&attrs);
	attrs.alpha                     = false;
	attrs.depth                     = true;
	attrs.enableExtensionsByDefault = true;
	attrs.majorVersion              = 2;
	EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx = emscripten_webgl_create_context("canvas", &attrs);
	emscripten_webgl_make_context_current(ctx);
#endif // _SKG_GL_LOAD_EMSCRIPTEN
	return 1;
}

///////////////////////////////////////////

int32_t gl_init_egl() {
#ifdef _SKG_GL_LOAD_EGL
	const EGLint attribs[] = {
		EGL_SURFACE_TYPE, EGL_DONT_CARE,
		EGL_CONFORMANT,   EGL_OPENGL_ES3_BIT_KHR,
		EGL_BLUE_SIZE,  8,
		EGL_GREEN_SIZE, 8,
		EGL_RED_SIZE,   8,
		EGL_ALPHA_SIZE, 8,
		EGL_DEPTH_SIZE, 0,
		EGL_NONE
	};
	EGLint context_attribs[] = { 
		EGL_CONTEXT_CLIENT_VERSION, 3,
		EGL_NONE };
	EGLint format;
	EGLint numConfigs;

	// No display means no overrides
	if (egl_display == EGL_NO_DISPLAY) {
		egl_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
		if (eglGetError() != EGL_SUCCESS) { skg_log(skg_log_critical, "Err eglGetDisplay"); return 0; }
	}

	int32_t major=0, minor=0;
	eglInitialize(egl_display, &major, &minor);
	
	#if defined(SKG_LINUX_EGL)
	if (egl_display == EGL_NO_DISPLAY || eglGetError() != EGL_SUCCESS) {
		skg_log(skg_log_info, "Trying EGL direct rendering from /dev/dri/renderD128");
		int32_t fd = open ("/dev/dri/renderD128", O_RDWR | O_CLOEXEC);
		if (fd <= 0) {
			skg_log(skg_log_critical, "Could not find direct rendering interface at /dev/dri/renderD128");
			return 0;
		}

		struct gbm_device *gbm = gbm_create_device (fd);
		if (gbm == NULL) {
			skg_log(skg_log_critical, "Could not create a GBM device");
			return 0;
		}

		egl_display = eglGetPlatformDisplay (EGL_PLATFORM_GBM_MESA, gbm, NULL);
		if (eglGetError() != EGL_SUCCESS) {
			skg_log(skg_log_critical, "Could not get a platform display");
			return 0;
		}
		egl_dri = true;
		eglInitialize(egl_display, &major, &minor);
	}
	#endif

	if (eglGetError() != EGL_SUCCESS) { skg_log(skg_log_critical, "Err eglInitialize"); return 0; }
	char version[128];
	snprintf(version, sizeof(version), "EGL version %d.%d", major, minor);
	skg_log(skg_log_info, version);

	eglChooseConfig   (egl_display, attribs, &egl_config, 1, &numConfigs);
	if (eglGetError() != EGL_SUCCESS) { skg_log(skg_log_critical, "Err eglChooseConfig"   ); return 0; }
	eglGetConfigAttrib(egl_display, egl_config, EGL_NATIVE_VISUAL_ID, &format);
	if (eglGetError() != EGL_SUCCESS) { skg_log(skg_log_critical, "Err eglGetConfigAttrib"); return 0; }

	egl_context = eglCreateContext      (egl_display, egl_config, nullptr, context_attribs);
	if (eglGetError() != EGL_SUCCESS) { skg_log(skg_log_critical, "Err eglCreateContext"  ); return 0; }

	const char* egl_extensions       = eglQueryString(egl_display, EGL_EXTENSIONS);
	bool        supports_surfaceless = egl_extensions != nullptr && strstr(egl_extensions, "EGL_KHR_surfaceless_context") != nullptr;

	egl_temp_surface = nullptr;
	if (supports_surfaceless == false) {
		EGLint temp_buffer_attr[] = {
			EGL_WIDTH,  1,
			EGL_HEIGHT, 1,
			EGL_NONE };
		egl_temp_surface = eglCreatePbufferSurface(egl_display, egl_config, temp_buffer_attr);
		if (egl_temp_surface == EGL_NO_SURFACE) {
			skg_log(skg_log_critical, "Unable to create temporary EGL surface");
			return -1;
		}
	}

	if (eglMakeCurrent(egl_display, egl_temp_surface, egl_temp_surface, egl_context) == EGL_FALSE) {
		skg_log(skg_log_critical, "Unable to eglMakeCurrent");
		return -1;
	}
#endif // _SKG_GL_LOAD_EGL
	return 1;
}

///////////////////////////////////////////

int32_t gl_init_glx() {
#ifdef _SKG_GL_LOAD_GLX
	// Load the OpenGL library
	void* lib = dlopen("libGL.so.1", RTLD_LAZY|RTLD_GLOBAL);
	if (lib == nullptr) lib = dlopen("libGL.so", RTLD_LAZY|RTLD_GLOBAL);
	if (lib == nullptr) {
		skg_log(skg_log_critical, "Unable to load GL!");
		return -1;
	}
	glXCreateContext           = (glXCreateContext_proc)          dlsym(lib, "glXCreateContext");
	glXCreateContextAttribsARB = (glXCreateContextAttribsARB_proc)dlsym(lib, "glXCreateContextAttribsARB");
	glXDestroyContext          = (glXDestroyContext_proc)         dlsym(lib, "glXDestroyContext");
	glXMakeCurrent             = (glXMakeCurrent_proc)            dlsym(lib, "glXMakeCurrent");
	glXSwapBuffers             = (glXSwapBuffers_proc)            dlsym(lib, "glXSwapBuffers");
	glXGetProcAddress          = (glXGetProcAddress_proc)         dlsym(lib, "glXGetProcAddress");

	GLXContext old_ctx = glXCreateContext(xDisplay, visualInfo, NULL, true);
	glXMakeCurrent(xDisplay, glxDrawable, old_ctx);

	int ctx_attribute_list[] = {
		GLX_RENDER_TYPE,               GLX_RGBA_TYPE,
		GLX_CONTEXT_MAJOR_VERSION_ARB, 4,
		GLX_CONTEXT_MINOR_VERSION_ARB, 5,
#if !defined(NDEBUG)
		GLX_CONTEXT_FLAGS_ARB,         GLX_CONTEXT_DEBUG_BIT_ARB,
#endif
		GLX_CONTEXT_PROFILE_MASK_ARB,  GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
		0
	};
	glxContext = glXCreateContextAttribsARB(xDisplay, glxFBConfig, NULL, true, ctx_attribute_list);
	glXDestroyContext(xDisplay, old_ctx);
	glXMakeCurrent(xDisplay, glxDrawable, glxContext);

#endif // _SKG_GL_LOAD_GLX
	return 1;
}

///////////////////////////////////////////

void skg_setup_xlib(void *dpy, void *vi, void *fbconfig, void *drawable) {
#ifdef _SKG_GL_LOAD_GLX
	xDisplay    =  (Display    *) dpy;
	visualInfo  =  (XVisualInfo*) vi;
	glxFBConfig = *(GLXFBConfig*) fbconfig;
	glxDrawable = *(Drawable   *) drawable;
#endif
}

///////////////////////////////////////////

void gl_check_exts() {
	int32_t ct;
	glGetIntegerv(GL_NUM_EXTENSIONS, &ct);
	for (int32_t i = 0; i < ct; i++) {
		const char* ext = (const char *)glGetStringi(GL_EXTENSIONS, i);
		if (strcmp(ext, "GL_AMD_vertex_shader_layer"            ) == 0) gl_caps[skg_cap_tex_layer_select] = true;
		if (strcmp(ext, "GL_EXT_multisampled_render_to_texture2") == 0) gl_caps[skg_cap_tiled_multisample] = true;
	}
	
#ifndef _SKG_GL_WEB
	// On some platforms, glPolygonMode is a function and not a function 
	// pointer, so glPolygonMode != nullptr is trivially true, and Clang wants
	// to warn us about that. This isn't an actual problem, so let's suppress
	// that warning.
#ifdef __clang__
	#pragma clang diagnostic push
	#pragma clang diagnostic ignored "-Wtautological-pointer-compare"
#endif
	gl_caps[skg_cap_wireframe] = glPolygonMode != nullptr;
#ifdef __clang__
	#pragma clang diagnostic pop
#endif
	#endif
};

///////////////////////////////////////////

int32_t skg_init(const char *app_name, void *adapter_id) {
#if   defined(_SKG_GL_LOAD_WGL)
	int32_t result = gl_init_wgl();
#elif defined(_SKG_GL_LOAD_EGL)
	int32_t result = gl_init_egl();
#elif defined(_SKG_GL_LOAD_GLX)
	int32_t result = gl_init_glx();
#elif defined(_SKG_GL_LOAD_EMSCRIPTEN)
	int32_t result = gl_init_emscripten();
#endif
	if (!result)
		return result;

	// Load OpenGL function pointers
#ifdef _SKG_GL_MAKE_FUNCTIONS
	gl_load_extensions();
#endif

	const char* name     = (const char *)glGetString(GL_RENDERER);
	size_t      name_len = strlen(name);
	gl_adapter_name = (char*)malloc(name_len+1);
	memcpy(gl_adapter_name, name, name_len);
	gl_adapter_name[name_len] = '\0';

	skg_logf(skg_log_info, "Using OpenGL: %s", glGetString(GL_VERSION));
	skg_logf(skg_log_info, "Device: %s", gl_adapter_name);

#if !defined(NDEBUG) && !defined(_SKG_GL_WEB)
	skg_log(skg_log_info, "Debug info enabled.");
	// Set up debug info for development
	glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageCallback([](uint32_t source, uint32_t type, uint32_t id, uint32_t severity, int32_t length, const char *message, const void *userParam) {
		if (id == 0x7fffffff) return;

		const char *src = "OTHER";
		switch (source) {
		case 0x8246: src = "API"; break;
		case 0x8247: src = "WINDOW SYSTEM"; break;
		case 0x8248: src = "SHADER COMPILER"; break;
		case 0x8249: src = "THIRD PARTY"; break;
		case 0x824A: src = "APPLICATION"; break;
		case 0x824B: src = "OTHER"; break;
		}

		const char *type_str = "OTHER";
		switch (type) {
		case 0x824C: type_str = "ERROR"; break;
		case 0x824D: type_str = "DEPRECATED_BEHAVIOR"; break;
		case 0x824E: type_str = "UNDEFINED_BEHAVIOR"; break;
		case 0x824F: type_str = "PORTABILITY"; break;
		case 0x8250: type_str = "PERFORMANCE"; break;
		case 0x8268: type_str = "MARKER"; break;
		case 0x8251: type_str = "OTHER"; break;
		}

		char msg[1024];
		snprintf(msg, sizeof(msg), "%s/%s - 0x%x - %s", src, type_str, id, message);

		switch (severity) {
		case GL_DEBUG_SEVERITY_NOTIFICATION: break;
		case GL_DEBUG_SEVERITY_LOW:    skg_log(skg_log_info,     msg); break;
		case GL_DEBUG_SEVERITY_MEDIUM: skg_log(skg_log_warning,  msg); break;
		case GL_DEBUG_SEVERITY_HIGH:   skg_log(skg_log_critical, msg); break;
		}
	}, nullptr);
#endif // !defined(NDEBUG) && !defined(_SKG_GL_WEB)

	gl_check_exts();
	
	// Some default behavior
	glEnable   (GL_DEPTH_TEST);  
	glEnable   (GL_CULL_FACE);
	glCullFace (GL_BACK);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); 
#ifdef _SKG_GL_DESKTOP
	glEnable   (GL_TEXTURE_CUBE_MAP_SEAMLESS);
#endif
	
	return 1;
}

///////////////////////////////////////////

const char* skg_adapter_name() {
	return gl_adapter_name;
}

///////////////////////////////////////////

void skg_shutdown() {
	free(gl_adapter_name); gl_adapter_name = nullptr;

#if defined(_SKG_GL_LOAD_WGL)
	wglMakeCurrent(NULL, NULL);
	ReleaseDC(gl_hwnd, gl_hdc);
	wglDeleteContext(gl_hrc);
#elif defined(_SKG_GL_LOAD_EGL)

	if (egl_temp_surface) {
		eglDestroySurface(egl_display, egl_temp_surface);
		egl_temp_surface = nullptr;
	}

	if (egl_display != EGL_NO_DISPLAY) {
		eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		if (egl_context != EGL_NO_CONTEXT) eglDestroyContext(egl_display, egl_context);
		eglTerminate(egl_display);
	}
	egl_display = EGL_NO_DISPLAY;
	egl_context = EGL_NO_CONTEXT;
#endif
}

///////////////////////////////////////////

void skg_draw_begin() {
}

///////////////////////////////////////////

void skg_tex_target_bind(skg_tex_t *render_target, int32_t layer_idx, int32_t mip_level) {
	gl_active_rendertarget = render_target;
	gl_current_framebuffer = render_target == nullptr 
		? 0 
		: layer_idx >= 0 && render_target->array_count > 1
			? render_target->_framebuffer_layers[layer_idx]
			: render_target->_framebuffer;

	glBindFramebuffer(GL_FRAMEBUFFER, gl_current_framebuffer);
	if (render_target) {
		glViewport(0, 0, render_target->width, render_target->height);
	} else {
		glViewport(0, 0, gl_active_width, gl_active_height);
	}

#ifndef _SKG_GL_WEB
	if (render_target == nullptr || render_target->format == skg_tex_fmt_rgba32 || render_target->format == skg_tex_fmt_bgra32) {
		glEnable(GL_FRAMEBUFFER_SRGB);
	} else {
		glDisable(GL_FRAMEBUFFER_SRGB);
	}
#endif
}

///////////////////////////////////////////

void skg_target_clear(bool depth, const float *clear_color_4) {
	uint32_t clear_mask = 0;
	if (depth) {
		clear_mask = GL_DEPTH_BUFFER_BIT;
		// If DepthMask is false, glClear won't clear depth
		glDepthMask(true);
	}
	if (clear_color_4) {
		clear_mask = clear_mask | GL_COLOR_BUFFER_BIT;
		glClearColor(clear_color_4[0], clear_color_4[1], clear_color_4[2], clear_color_4[3]);
	}

	glClear(clear_mask);
}

///////////////////////////////////////////

skg_tex_t *skg_tex_target_get() {
	return gl_active_rendertarget;
}

///////////////////////////////////////////

skg_platform_data_t skg_get_platform_data() {
	skg_platform_data_t result = {};
#if   defined(_SKG_GL_LOAD_WGL)
	result._gl_hdc = gl_hdc;
	result._gl_hrc = gl_hrc;
#elif defined(_SKG_GL_LOAD_EGL)
	result._egl_display = egl_display;
	result._egl_config  = egl_config;
	result._egl_context = egl_context;
#elif defined(_SKG_GL_LOAD_GLX)
	result._x_display     = xDisplay;
	result._visual_id     = &visualInfo->visualid;
	result._glx_fb_config = glxFBConfig;
	result._glx_drawable  = &glxDrawable;
	result._glx_context   = glxContext;
#endif
	return result;
}

///////////////////////////////////////////

bool skg_capability(skg_cap_ capability) {
	return gl_caps[capability];
}

///////////////////////////////////////////

void skg_event_begin (const char *name) {
#if defined(_DEBUG) && !defined(_SKG_GL_WEB)
	if (glPushDebugGroupKHR)
		glPushDebugGroupKHR(GL_DEBUG_SOURCE_APPLICATION, 0, -1, name);
#endif
}

///////////////////////////////////////////

void skg_event_end () {
#if defined(_DEBUG) && !defined(_SKG_GL_WEB)
	if (glPopDebugGroupKHR)
		glPopDebugGroupKHR();
#endif
}

///////////////////////////////////////////

void skg_draw(int32_t index_start, int32_t index_base, int32_t index_count, int32_t instance_count) {
#ifdef _SKG_GL_WEB
	glDrawElementsInstanced(GL_TRIANGLES, index_count, GL_UNSIGNED_INT, (void*)(index_start*sizeof(uint32_t)), instance_count);
#else
	glDrawElementsInstancedBaseVertex(GL_TRIANGLES, index_count, GL_UNSIGNED_INT, (void*)(index_start*sizeof(uint32_t)), instance_count, index_base);
#endif
}

///////////////////////////////////////////

void skg_compute(uint32_t thread_count_x, uint32_t thread_count_y, uint32_t thread_count_z) {
	glDispatchCompute(thread_count_x, thread_count_y, thread_count_z);
}

///////////////////////////////////////////

void skg_viewport(const int32_t *xywh) {
	glViewport(xywh[0], xywh[1], xywh[2], xywh[3]);
}

///////////////////////////////////////////

void skg_viewport_get(int32_t *out_xywh) {
	glGetIntegerv(GL_VIEWPORT, out_xywh);
}

///////////////////////////////////////////

void skg_scissor(const int32_t *xywh) {
	int32_t viewport[4];
	skg_viewport_get(viewport);
	glScissor(xywh[0], (viewport[3]-xywh[1])-xywh[3], xywh[2], xywh[3]);
}

///////////////////////////////////////////

skg_buffer_t skg_buffer_create(const void *data, uint32_t size_count, uint32_t size_stride, skg_buffer_type_ type, skg_use_ use) {
	skg_buffer_t result = {};
	result.use     = use;
	result.type    = type;
	result.stride  = size_stride;
	result._target = skg_buffer_type_to_gl(type);

	glGenBuffers(1, &result._buffer);
	glBindBuffer(result._target, result._buffer);
	glBufferData(result._target, size_count * size_stride, data, use == skg_use_static ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW);

	return result;
}

///////////////////////////////////////////

void skg_buffer_name(skg_buffer_t *buffer, const char* name) {
	if (buffer->_buffer != 0)
		glObjectLabel(GL_BUFFER, buffer->_buffer, (uint32_t)strlen(name), name);
}

///////////////////////////////////////////

bool skg_buffer_is_valid(const skg_buffer_t *buffer) {
	return buffer->_buffer != 0;
}

///////////////////////////////////////////

void skg_buffer_set_contents(skg_buffer_t *buffer, const void *data, uint32_t size_bytes) {
	if (buffer->use != skg_use_dynamic) {
		skg_log(skg_log_warning, "Attempting to dynamically set contents of a static buffer!");
		return;
	}

	glBindBuffer   (buffer->_target, buffer->_buffer);
	glBufferSubData(buffer->_target, 0, size_bytes, data);
}

///////////////////////////////////////////

void skg_buffer_bind(const skg_buffer_t *buffer, skg_bind_t bind, uint32_t offset) {
	if (buffer->type == skg_buffer_type_constant || buffer->type == skg_buffer_type_compute)
		glBindBufferBase(buffer->_target, bind.slot, buffer->_buffer);
	else if (buffer->type == skg_buffer_type_vertex) {
#ifdef _SKG_GL_WEB
		glBindBuffer(buffer->_target, buffer->_buffer);
#else
		glBindVertexBuffer(bind.slot, buffer->_buffer, offset, buffer->stride);
#endif
	} else
		glBindBuffer(buffer->_target, buffer->_buffer);
}

///////////////////////////////////////////

void skg_buffer_clear(skg_bind_t bind) {
	if (bind.stage_bits == skg_stage_compute) {
		if (bind.register_type == skg_register_constant)
			glBindBufferBase(GL_UNIFORM_BUFFER, bind.slot, 0);
		if (bind.register_type == skg_register_readwrite)
			glBindBufferBase(GL_SHADER_STORAGE_BUFFER, bind.slot, 0);
	}
}

///////////////////////////////////////////

void skg_buffer_destroy(skg_buffer_t *buffer) {
	uint32_t buffer_list[] = { buffer->_buffer };
	glDeleteBuffers(1, buffer_list);
	*buffer = {};
}

///////////////////////////////////////////

skg_mesh_t skg_mesh_create(const skg_buffer_t *vert_buffer, const skg_buffer_t *ind_buffer) {
	skg_mesh_t result = {};
	skg_mesh_set_verts(&result, vert_buffer);
	skg_mesh_set_inds (&result, ind_buffer);

	return result;
}

///////////////////////////////////////////

void skg_mesh_name(skg_mesh_t *mesh, const char* name) {
	char postfix_name[256];
	if (mesh->_vert_buffer != 0) {
		snprintf(postfix_name, sizeof(postfix_name), "%s_verts", name);
		glObjectLabel(GL_BUFFER, mesh->_vert_buffer,  (uint32_t)strlen(postfix_name), postfix_name);
	}
	if (mesh->_ind_buffer != 0) {
		snprintf(postfix_name, sizeof(postfix_name), "%s_inds", name);
		glObjectLabel(GL_BUFFER, mesh->_ind_buffer,  (uint32_t)strlen(postfix_name), postfix_name);
	}
}

///////////////////////////////////////////

void skg_mesh_set_verts(skg_mesh_t *mesh, const skg_buffer_t *vert_buffer) {
	mesh->_vert_buffer = vert_buffer ? vert_buffer->_buffer : 0;
	if (mesh->_vert_buffer != 0) {
		if (mesh->_layout != 0) {
			glDeleteVertexArrays(1, &mesh->_layout);
			mesh->_layout = 0;
		}

		glBindBuffer(GL_ARRAY_BUFFER, mesh->_vert_buffer);

		// Create a vertex layout
		glGenVertexArrays(1, &mesh->_layout);
		glBindVertexArray(mesh->_layout);
		// enable the vertex data for the shader
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glEnableVertexAttribArray(2);
		glEnableVertexAttribArray(3);
		// tell the shader how our vertex data binds to the shader inputs
		glVertexAttribPointer(0, 3, GL_FLOAT,         0, sizeof(skg_vert_t), nullptr);
		glVertexAttribPointer(1, 3, GL_FLOAT,         0, sizeof(skg_vert_t), (void*)(sizeof(float) * 3));
		glVertexAttribPointer(2, 2, GL_FLOAT,         0, sizeof(skg_vert_t), (void*)(sizeof(float) * 6));
		glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, 1, sizeof(skg_vert_t), (void*)(sizeof(float) * 8));
	}
}

///////////////////////////////////////////

void skg_mesh_set_inds(skg_mesh_t *mesh, const skg_buffer_t *ind_buffer) {
	mesh->_ind_buffer = ind_buffer ? ind_buffer->_buffer : 0;
}

///////////////////////////////////////////

void skg_mesh_bind(const skg_mesh_t *mesh) {
	glBindVertexArray(mesh->_layout);
	glBindBuffer(GL_ARRAY_BUFFER,         mesh->_vert_buffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh->_ind_buffer );
}

///////////////////////////////////////////

void skg_mesh_destroy(skg_mesh_t *mesh) {
	uint32_t vao_list[] = {mesh->_layout};
	glDeleteVertexArrays(1, vao_list);
	*mesh = {};
}

///////////////////////////////////////////
// skg_shader_t                          //
///////////////////////////////////////////

skg_shader_stage_t skg_shader_stage_create(const void *file_data, size_t shader_size, skg_stage_ type) {
	const char *file_chars = (const char *)file_data;

	skg_shader_stage_t result = {}; 
	result.type = type;

	// Include terminating character
	if (shader_size > 0 && file_chars[shader_size-1] != '\0')
		shader_size += 1;

	uint32_t gl_type = 0;
	switch (type) {
	case skg_stage_pixel:   gl_type = GL_FRAGMENT_SHADER; break;
	case skg_stage_vertex:  gl_type = GL_VERTEX_SHADER;   break;
	case skg_stage_compute: gl_type = GL_COMPUTE_SHADER;  break;
	}

	// Convert the prefix if it doesn't match the GL version we're using
#if   defined(_SKG_GL_ES)
	const char   *prefix_gl      = "#version 310 es";
#elif defined(_SKG_GL_DESKTOP)
	const char   *prefix_gl      = "#version 450";
#elif defined(_SKG_GL_WEB)
	const char   *prefix_gl      = "#version 300 es";
#endif
	const size_t  prefix_gl_size = strlen(prefix_gl);
	char         *final_data = (char*)file_chars;
	bool          needs_free = false;

	if (shader_size >= prefix_gl_size && memcmp(prefix_gl, file_chars, prefix_gl_size) != 0) {
		const char *end = file_chars;
		while (*end != '\n' && *end != '\r' && *end != '\0') end++;
		size_t version_size = end - file_chars;

		final_data = (char*)malloc(sizeof(char) * ((shader_size-version_size)+prefix_gl_size));
		memcpy(final_data, prefix_gl, prefix_gl_size);
		memcpy(&final_data[prefix_gl_size], &file_chars[version_size], shader_size - version_size);
		needs_free = true;
	}

	// create and compile the vertex shader
	result._shader = glCreateShader(gl_type);
	try {
		glShaderSource (result._shader, 1, &final_data, NULL);
		glCompileShader(result._shader);
	} catch (...) {
		// Some GL drivers have a habit of crashing during shader compile.
		const char *stage_name = "";
		switch (type) {
			case skg_stage_pixel:   stage_name = "Pixel";   break;
			case skg_stage_vertex:  stage_name = "Vertex";  break;
			case skg_stage_compute: stage_name = "Compute"; break; }
		skg_logf(skg_log_warning, "%s shader compile exception", stage_name);
		glDeleteShader(result._shader);
		result._shader = 0;
		if (needs_free)
			free(final_data);
		return result;
	}

	// check for errors?
	int32_t err, length;
	glGetShaderiv(result._shader, GL_COMPILE_STATUS, &err);
	if (err == 0) {
		char *log;

		glGetShaderiv(result._shader, GL_INFO_LOG_LENGTH, &length);
		log = (char*)malloc(length);
		glGetShaderInfoLog(result._shader, length, &err, log);

		// Trim trailing newlines, we've already got that covered
		size_t len = strlen(log);
		while(len > 0 && log[len-1] == '\n') { log[len-1] = '\0'; len -= 1; }

		skg_logf(skg_log_warning, "Unable to compile shader (%d):\n%s", err, log);
		free(log);

		glDeleteShader(result._shader);
		result._shader = 0;
	}
	if (needs_free)
		free(final_data);

	return result;
}

///////////////////////////////////////////

void skg_shader_stage_destroy(skg_shader_stage_t *shader) {
	//glDeleteShader(shader->shader);
	*shader = {};
}

///////////////////////////////////////////

skg_shader_t skg_shader_create_manual(skg_shader_meta_t *meta, skg_shader_stage_t v_shader, skg_shader_stage_t p_shader, skg_shader_stage_t c_shader) {
	if (v_shader._shader == 0 && p_shader._shader == 0 && c_shader._shader == 0) {
#if   defined(_SKG_GL_ES)
		const char   *gl_name      = "GLES";
#elif defined(_SKG_GL_DESKTOP)
		const char   *gl_name      = "OpenGL";
#elif defined(_SKG_GL_WEB)
		const char   *gl_name      = "WebGL";
#endif
		skg_logf(skg_log_warning, "Shader '%s' has no valid stages for %s!", meta->name, gl_name);
		return {};
	}

	skg_shader_t result = {};
	result.meta     = meta;
	result._vertex  = v_shader._shader;
	result._pixel   = p_shader._shader;
	result._compute = c_shader._shader;
	skg_shader_meta_reference(result.meta);

	result._program = glCreateProgram();
	if (result._vertex)  glAttachShader(result._program, result._vertex);
	if (result._pixel)   glAttachShader(result._program, result._pixel);
	if (result._compute) glAttachShader(result._program, result._compute);
	try {
		glLinkProgram(result._program);
	} catch (...) {
		// Some GL drivers have a habit of crashing during shader compile.
		skg_logf(skg_log_warning, "Shader link exception in %s:", meta->name);
		glDeleteProgram(result._program);
		result._program = 0;
		return result;
	}

	// check for errors?
	int32_t err, length;
	glGetProgramiv(result._program, GL_LINK_STATUS, &err);
	if (err == 0) {
		char *log;

		glGetProgramiv(result._program, GL_INFO_LOG_LENGTH, &length);
		log = (char*)malloc(length);
		glGetProgramInfoLog(result._program, length, &err, log);

		skg_logf(skg_log_warning, "Unable to link %s:", meta->name);
		skg_log(skg_log_warning, log);
		free(log);

		glDeleteProgram(result._program);
		result._program = 0;

		return result;
	}

	// Set buffer binds
	char t_name[64];
	for (uint32_t i = 0; i < meta->buffer_count; i++) {
		snprintf(t_name, 64, "%s", meta->buffers[i].name);
		// $Global is a near universal buffer name, we need to scrape the
		// '$' character out.
		char *pr = t_name;
		while (*pr) {
			if (*pr == '$')
				*pr = '_';
			pr++;
		}

		uint32_t slot = glGetUniformBlockIndex(result._program, t_name);
		if (slot != GL_INVALID_INDEX) {
			glUniformBlockBinding(result._program, slot, meta->buffers[i].bind.slot);
		} else {
			skg_logf(skg_log_warning, "Couldn't find uniform block index for: %s", meta->buffers[i].name);
		}
	}

	// Set sampler uniforms
	glUseProgram(result._program);
	for (uint32_t i = 0; i < meta->resource_count; i++) {
		int32_t loc = glGetUniformLocation(result._program, meta->resources[i].name);
		if (loc == -1) {
			// Sometimes the shader compiler will prefix the variable with an
			// _, particularly if it overlaps with a keyword of some sort.
			snprintf(t_name, 64, "_%s", meta->resources[i].name);
			loc = glGetUniformLocation(result._program, t_name);
		}

		if (loc != -1) {
			glUniform1i(loc, meta->resources[i].bind.slot);
		} else {
			// This may not be much of an issue under current usage patterns,
			// but could cause problems for compute shaders later on.
			skg_logf(skg_log_warning, "Couldn't find uniform location for: %s (is it a compute shader buffer?)", meta->resources[i].name);
		}
	}

	return result;
}

///////////////////////////////////////////

void skg_shader_name(skg_shader_t *shader, const char* name) {
	char postfix_name[256];
	if (shader->_program != 0) {
		snprintf(postfix_name, sizeof(postfix_name), "%s_program", name);
		glObjectLabel(GL_PROGRAM, shader->_program, (uint32_t)strlen(postfix_name), postfix_name);
	}
	if (shader->_compute != 0) {
		snprintf(postfix_name, sizeof(postfix_name), "%s_cs", name);
		glObjectLabel(GL_SHADER, shader->_compute, (uint32_t)strlen(postfix_name), postfix_name);
	}
	if (shader->_pixel != 0) {
		snprintf(postfix_name, sizeof(postfix_name), "%s_ps", name);
		glObjectLabel(GL_SHADER, shader->_pixel, (uint32_t)strlen(postfix_name), postfix_name);
	}
	if (shader->_vertex != 0) {
		snprintf(postfix_name, sizeof(postfix_name), "%s_vs", name);
		glObjectLabel(GL_SHADER, shader->_vertex, (uint32_t)strlen(postfix_name), postfix_name);
	}
}

///////////////////////////////////////////

void skg_shader_compute_bind(const skg_shader_t *shader) {
	if (shader) glUseProgram(shader->_program);
	else        glUseProgram(0);
}

///////////////////////////////////////////

bool skg_shader_is_valid(const skg_shader_t *shader) {
	return shader->meta
		&& shader->_program;
}

///////////////////////////////////////////

void skg_shader_destroy(skg_shader_t *shader) {
	skg_shader_meta_release(shader->meta);
	glDeleteProgram(shader->_program);
	glDeleteShader (shader->_vertex);
	glDeleteShader (shader->_pixel);
	*shader = {};
}

///////////////////////////////////////////
// skg_pipeline                          //
///////////////////////////////////////////

skg_pipeline_t skg_pipeline_create(skg_shader_t *shader) {
	skg_pipeline_t result = {};
	result.transparency = skg_transparency_none;
	result.cull         = skg_cull_back;
	result.wireframe    = false;
	result.depth_test   = skg_depth_test_less;
	result.depth_write  = true;
	result.meta         = shader->meta;
	result._shader      = *shader;
	skg_shader_meta_reference(result._shader.meta);

	return result;
}

///////////////////////////////////////////

void skg_pipeline_name(skg_pipeline_t *pipeline, const char* name) {
}

///////////////////////////////////////////

void skg_pipeline_bind(const skg_pipeline_t *pipeline) {
	glUseProgram(pipeline->_shader._program);
	
	switch (pipeline->transparency) {
	case skg_transparency_alpha_to_coverage:
		glDisable(GL_BLEND);
		glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);
		break;
	case skg_transparency_blend:
		glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glBlendEquationSeparate(GL_FUNC_ADD, GL_MAX);
		break;
	case skg_transparency_add:
		glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE);
		glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
		break;
	case skg_transparency_none:
		glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
		glDisable(GL_BLEND);
		break;
	}

	switch (pipeline->cull) {
	case skg_cull_back: {
		glEnable  (GL_CULL_FACE);
		glCullFace(GL_BACK);
	} break;
	case skg_cull_front: {
		glEnable  (GL_CULL_FACE);
		glCullFace(GL_FRONT);
	} break;
	case skg_cull_none: {
		glDisable(GL_CULL_FACE);
	} break;
	}

	if (pipeline->depth_test != skg_depth_test_always)
		 glEnable (GL_DEPTH_TEST);
	else glDisable(GL_DEPTH_TEST);

	if (pipeline->scissor) glEnable (GL_SCISSOR_TEST);
	else                   glDisable(GL_SCISSOR_TEST);

	glDepthMask(pipeline->depth_write);
	switch (pipeline->depth_test) {
	case skg_depth_test_always:        glDepthFunc(GL_ALWAYS);   break;
	case skg_depth_test_equal:         glDepthFunc(GL_EQUAL);    break;
	case skg_depth_test_greater:       glDepthFunc(GL_GREATER);  break;
	case skg_depth_test_greater_or_eq: glDepthFunc(GL_GEQUAL);   break;
	case skg_depth_test_less:          glDepthFunc(GL_LESS);     break;
	case skg_depth_test_less_or_eq:    glDepthFunc(GL_LEQUAL);   break;
	case skg_depth_test_never:         glDepthFunc(GL_NEVER);    break;
	case skg_depth_test_not_equal:     glDepthFunc(GL_NOTEQUAL); break; }
	
#ifdef _SKG_GL_DESKTOP
	if (pipeline->wireframe) {
		glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
	} else {
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	}
#endif
}

///////////////////////////////////////////

void skg_pipeline_set_transparency(skg_pipeline_t *pipeline, skg_transparency_ transparency) {
	pipeline->transparency = transparency;
}

///////////////////////////////////////////

void skg_pipeline_set_cull(skg_pipeline_t *pipeline, skg_cull_ cull) {
	pipeline->cull = cull;
}

///////////////////////////////////////////

void skg_pipeline_set_wireframe(skg_pipeline_t *pipeline, bool wireframe) {
	pipeline->wireframe = wireframe;
}

///////////////////////////////////////////

void skg_pipeline_set_depth_write(skg_pipeline_t *pipeline, bool write) {
	pipeline->depth_write = write;
}

///////////////////////////////////////////

void skg_pipeline_set_depth_test (skg_pipeline_t *pipeline, skg_depth_test_ test) {
	pipeline->depth_test = test;
}

///////////////////////////////////////////

void skg_pipeline_set_scissor(skg_pipeline_t *pipeline, bool enable) {
	pipeline->scissor = enable;

}

///////////////////////////////////////////

skg_transparency_ skg_pipeline_get_transparency(const skg_pipeline_t *pipeline) {
	return pipeline->transparency;
}

///////////////////////////////////////////

skg_cull_ skg_pipeline_get_cull(const skg_pipeline_t *pipeline) {
	return pipeline->cull;
}

///////////////////////////////////////////

bool skg_pipeline_get_wireframe(const skg_pipeline_t *pipeline) {
	return pipeline->wireframe;
}

///////////////////////////////////////////

bool skg_pipeline_get_depth_write(const skg_pipeline_t *pipeline) {
	return pipeline->depth_write;
}

///////////////////////////////////////////

skg_depth_test_ skg_pipeline_get_depth_test(const skg_pipeline_t *pipeline) {
	return pipeline->depth_test;
}

///////////////////////////////////////////

bool skg_pipeline_get_scissor(const skg_pipeline_t *pipeline) {
	return pipeline->scissor;
}

///////////////////////////////////////////

void skg_pipeline_destroy(skg_pipeline_t *pipeline) {
	skg_shader_meta_release(pipeline->_shader.meta);
	*pipeline = {};
}

///////////////////////////////////////////

skg_swapchain_t skg_swapchain_create(void *hwnd, skg_tex_fmt_ format, skg_tex_fmt_ depth_format, int32_t requested_width, int32_t requested_height) {
	skg_swapchain_t result = {};

#if defined(_SKG_GL_LOAD_WGL)
	result._hwnd  = hwnd;
	result._hdc   = GetDC((HWND)hwnd);
	result.width  = requested_width;
	result.height = requested_height;

	// Find a pixel format
	const int format_attribs[] = {
		WGL_DRAW_TO_WINDOW_ARB, true,
		WGL_SUPPORT_OPENGL_ARB, true,
		WGL_DOUBLE_BUFFER_ARB,  true,
		WGL_ACCELERATION_ARB,   WGL_FULL_ACCELERATION_ARB,
		WGL_PIXEL_TYPE_ARB,     WGL_TYPE_RGBA_ARB,
		WGL_COLOR_BITS_ARB,     32,
		WGL_DEPTH_BITS_ARB,     0,
		WGL_STENCIL_BITS_ARB,   0,
		WGL_SAMPLE_BUFFERS_ARB, 0,
		WGL_SAMPLES_ARB,        0,
		0 };

	int  pixel_format = 0;
	UINT num_formats  = 0;
	if (!wglChoosePixelFormatARB((HDC)result._hdc, format_attribs, nullptr, 1, &pixel_format, &num_formats)) {
		skg_log(skg_log_critical, "Couldn't find pixel format!");
		result = {};
		return result;
	}

	PIXELFORMATDESCRIPTOR format_desc = { sizeof(PIXELFORMATDESCRIPTOR) };
	DescribePixelFormat((HDC)result._hdc, pixel_format, sizeof(format_desc), &format_desc);
	if (!SetPixelFormat((HDC)result._hdc, pixel_format, &format_desc)) {
		skg_log(skg_log_critical, "Couldn't set pixel format!");
		result = {};
		return result;
	}
#elif defined(_SKG_GL_LOAD_EGL)
	EGLint attribs[] = { 
		EGL_GL_COLORSPACE_KHR, EGL_GL_COLORSPACE_SRGB_KHR,
		EGL_NONE };
	result._egl_surface = eglCreateWindowSurface(egl_display, egl_config, (EGLNativeWindowType)hwnd, attribs);
	if (eglGetError() != EGL_SUCCESS) skg_log(skg_log_critical, "Err eglCreateWindowSurface");
	
	if (eglMakeCurrent(egl_display, result._egl_surface, result._egl_surface, egl_context) == EGL_FALSE)
		skg_log(skg_log_critical, "Unable to eglMakeCurrent for swapchain");

	eglQuerySurface(egl_display, result._egl_surface, EGL_WIDTH,  &result.width );
	eglQuerySurface(egl_display, result._egl_surface, EGL_HEIGHT, &result.height);
#elif defined(_SKG_GL_LOAD_GLX)
	result._x_window = hwnd;
	result.width  = requested_width;
	result.height = requested_height;
#else
	int32_t viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);
	result.width  = viewport[2];
	result.height = viewport[3];
#endif

#if defined(_SKG_GL_WEB) && defined(SKG_MANUAL_SRGB)
	const char *vs = R"_(#version 300 es
layout(location = 0) in vec4 in_var_SV_POSITION;
layout(location = 1) in vec3 in_var_NORMAL;
layout(location = 2) in vec2 in_var_TEXCOORD0;
layout(location = 3) in vec4 in_var_COLOR;

out vec2 fs_var_TEXCOORD0;

void main() {
    gl_Position = in_var_SV_POSITION;
    fs_var_TEXCOORD0 = in_var_TEXCOORD0;
})_";
	const char *ps = R"_(#version 300 es
precision mediump float;
precision highp int;

uniform highp sampler2D tex;

in highp vec2 fs_var_TEXCOORD0;
layout(location = 0) out highp vec4 out_var_SV_TARGET;

void main() {
	vec4 color = texture(tex, fs_var_TEXCOORD0);
    out_var_SV_TARGET = vec4(pow(color.xyz, vec3(1.0 / 2.2)), color.w);
})_";

	skg_shader_meta_t *meta = (skg_shader_meta_t *)malloc(sizeof(skg_shader_meta_t));
	*meta = {};
	meta->resource_count = 1;
	meta->resources = (skg_shader_resource_t*)malloc(sizeof(skg_shader_resource_t));
	meta->resources[0].bind = { 0, skg_stage_pixel };
	strcpy(meta->resources[0].name, "tex");
	meta->resources[0].name_hash = skg_hash(meta->resources[0].name);

	skg_shader_stage_t v_stage = skg_shader_stage_create(vs, strlen(vs), skg_stage_vertex);
	skg_shader_stage_t p_stage = skg_shader_stage_create(ps, strlen(ps), skg_stage_pixel);
	result._convert_shader = skg_shader_create_manual(meta, v_stage, p_stage, {});
	result._convert_pipe   = skg_pipeline_create(&result._convert_shader);

	result._surface = skg_tex_create(skg_tex_type_rendertarget, skg_use_static, skg_tex_fmt_rgba32_linear, skg_mip_none);
	skg_tex_set_contents(&result._surface, nullptr, result.width, result.height);

	result._surface_depth = skg_tex_create(skg_tex_type_depth, skg_use_static, depth_format, skg_mip_none);
	skg_tex_set_contents(&result._surface_depth, nullptr, result.width, result.height);
	skg_tex_attach_depth(&result._surface, &result._surface_depth);

	skg_vert_t quad_verts[] = { 
		{ {-1, 1,0}, {0,0,1}, {0,1}, {255,255,255,255} },
		{ { 1, 1,0}, {0,0,1}, {1,1}, {255,255,255,255} },
		{ { 1,-1,0}, {0,0,1}, {1,0}, {255,255,255,255} },
		{ {-1,-1,0}, {0,0,1}, {0,0}, {255,255,255,255} } };
	uint32_t quad_inds[] = { 2,1,0, 3,2,0 };
	result._quad_vbuff = skg_buffer_create(quad_verts, 4, sizeof(skg_vert_t), skg_buffer_type_vertex, skg_use_static);
	result._quad_ibuff = skg_buffer_create(quad_inds,  6, sizeof(uint32_t  ), skg_buffer_type_index,  skg_use_static);
	result._quad_mesh  = skg_mesh_create(&result._quad_vbuff, &result._quad_ibuff);
#endif
	return result;
}

///////////////////////////////////////////

void skg_swapchain_resize(skg_swapchain_t *swapchain, int32_t width, int32_t height) {
	if (width == swapchain->width && height == swapchain->height)
		return;

	swapchain->width  = width;
	swapchain->height = height;

#ifdef _SKG_GL_WEB
	skg_tex_fmt_ color_fmt = swapchain->_surface.format;
	skg_tex_fmt_ depth_fmt = swapchain->_surface_depth.format;

	skg_tex_destroy(&swapchain->_surface);
	skg_tex_destroy(&swapchain->_surface_depth);

	swapchain->_surface = skg_tex_create(skg_tex_type_rendertarget, skg_use_static, color_fmt, skg_mip_none);
	skg_tex_set_contents(&swapchain->_surface, nullptr, swapchain->width, swapchain->height);

	swapchain->_surface_depth = skg_tex_create(skg_tex_type_depth, skg_use_static, depth_fmt, skg_mip_none);
	skg_tex_set_contents(&swapchain->_surface_depth, nullptr, swapchain->width, swapchain->height);
	skg_tex_attach_depth(&swapchain->_surface, &swapchain->_surface_depth);
#endif
}

///////////////////////////////////////////

void skg_swapchain_present(skg_swapchain_t *swapchain) {
#if   defined(_SKG_GL_LOAD_WGL)
	SwapBuffers((HDC)swapchain->_hdc);
#elif defined(_SKG_GL_LOAD_EGL)
	eglSwapBuffers(egl_display, swapchain->_egl_surface);
#elif defined(_SKG_GL_LOAD_GLX)
	glXSwapBuffers(xDisplay, (Drawable) swapchain->_x_window);
#elif defined(_SKG_GL_LOAD_EMSCRIPTEN) && defined(SKG_MANUAL_SRGB)
	float clear[4] = { 0,0,0,1 };
	skg_tex_target_bind(nullptr, -1, 0);
	skg_target_clear   (true, clear);
	skg_tex_bind      (&swapchain->_surface, {0, skg_stage_pixel});
	skg_mesh_bind     (&swapchain->_quad_mesh);
	skg_pipeline_bind (&swapchain->_convert_pipe);
	skg_draw          (0, 0, 6, 1);
#endif
}

///////////////////////////////////////////

void skg_swapchain_bind(skg_swapchain_t *swapchain) {
	gl_active_width  = swapchain->width;
	gl_active_height = swapchain->height;
#if   defined(_SKG_GL_LOAD_EMSCRIPTEN) && defined(SKG_MANUAL_SRGB)
	skg_tex_target_bind(&swapchain->_surface, -1, 0);
#elif defined(_SKG_GL_LOAD_WGL)
	wglMakeCurrent((HDC)swapchain->_hdc, gl_hrc);
	skg_tex_target_bind(nullptr, -1, 0);
#elif defined(_SKG_GL_LOAD_EGL)
	eglMakeCurrent(egl_display, swapchain->_egl_surface, swapchain->_egl_surface, egl_context);
	skg_tex_target_bind(nullptr, -1, 0);
#elif defined(_SKG_GL_LOAD_GLX)
	glXMakeCurrent(xDisplay, (Drawable)swapchain->_x_window, glxContext);
	skg_tex_target_bind(nullptr, -1, 0);
#endif
}

///////////////////////////////////////////

void skg_swapchain_destroy(skg_swapchain_t *swapchain) {
#if defined(_SKG_GL_LOAD_WGL)
	if (swapchain->_hdc != nullptr) {
		wglMakeCurrent(nullptr, nullptr);
		ReleaseDC((HWND)swapchain->_hwnd, (HDC)swapchain->_hdc);
		swapchain->_hwnd = nullptr;
		swapchain->_hdc  = nullptr;
	}
#elif defined(_SKG_GL_LOAD_EGL)
	eglMakeCurrent(egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	if (swapchain->_egl_surface != EGL_NO_SURFACE) eglDestroySurface(egl_display, swapchain->_egl_surface);
	swapchain->_egl_surface = EGL_NO_SURFACE;
#endif
}

///////////////////////////////////////////

uint32_t gl_tex_target(skg_tex_type_ type, int32_t array_count, int32_t multisample) {
	if (multisample > 1) {
		return array_count == 1
			? GL_TEXTURE_2D_MULTISAMPLE
			: GL_TEXTURE_2D_MULTISAMPLE_ARRAY;
	} else {
		return array_count > 1
			? (type == skg_tex_type_cubemap ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D_ARRAY)
			: GL_TEXTURE_2D;
	}
}

///////////////////////////////////////////

void gl_framebuffer_attach(uint32_t texture, uint32_t target, skg_tex_fmt_ format, int32_t physical_multisample, int32_t multisample, int32_t array_count, uint32_t layer, uint32_t mip_level) {
	uint32_t attach = GL_COLOR_ATTACHMENT0;
	if      (format == skg_tex_fmt_depthstencil)                             attach = GL_DEPTH_STENCIL_ATTACHMENT;
	else if (format == skg_tex_fmt_depth16 || format == skg_tex_fmt_depth32) attach = GL_DEPTH_ATTACHMENT;

	bool is_framebuffer_msaa = multisample > physical_multisample;
	if      (array_count > 1)                                                   glFramebufferTextureLayer           (GL_FRAMEBUFFER, attach,         texture, mip_level, layer);
	else if (gl_caps[skg_cap_tiled_multisample] == true && is_framebuffer_msaa) glFramebufferTexture2DMultisampleEXT(GL_FRAMEBUFFER, attach, target, texture, mip_level, multisample);
	else                                                                        glFramebufferTexture                (GL_FRAMEBUFFER, attach,         texture, mip_level);
	
	uint32_t status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE) {
		skg_logf(skg_log_critical, "Framebuffer incomplete: %x\n", status);
	}
}

///////////////////////////////////////////

skg_tex_t skg_tex_create_from_existing(void *native_tex, skg_tex_type_ type, skg_tex_fmt_ format, int32_t width, int32_t height, int32_t array_count, int32_t physical_multisample, int32_t framebuffer_multisample) {
	skg_tex_t result = {};
	result.type        = type;
	result.use         = skg_use_static;
	result.mips        = skg_mip_none;
	result.format      = format;
	result.width       = width;
	result.height      = height;
	result.array_count = array_count;
	result.multisample = framebuffer_multisample > physical_multisample ? framebuffer_multisample : physical_multisample;
	result._physical_multisample = physical_multisample;
	result._texture    = (uint32_t)(uint64_t)native_tex;
	result._format     = (uint32_t)skg_tex_fmt_to_native(result.format);
	result._target     = gl_tex_target(type, array_count, physical_multisample);

	if (type == skg_tex_type_rendertarget) {
		glGenFramebuffers(1, &result._framebuffer);

		glBindFramebuffer(GL_FRAMEBUFFER, result._framebuffer);
		gl_framebuffer_attach(result._texture, result._target, result.format, result._physical_multisample, result.multisample, result.array_count, 0, 0);

		// Add framebuffers for individual layers of any array surfaces
		if (result.array_count > 1) {
			result._framebuffer_layers = (uint32_t*)malloc(sizeof(uint32_t) * result.array_count);
			glGenFramebuffers(result.array_count, result._framebuffer_layers);

			for (int32_t i = 0; i < result.array_count; i++) {
				glBindFramebuffer(GL_FRAMEBUFFER, result._framebuffer_layers[i]);
				gl_framebuffer_attach(result._texture, result._target, result.format, result._physical_multisample, result.multisample, result.array_count, i, 0);
			}
		}

		glBindFramebuffer(GL_FRAMEBUFFER, gl_current_framebuffer);
	}
	
	return result;
}

///////////////////////////////////////////

skg_tex_t skg_tex_create_from_layer(void *native_tex, skg_tex_type_ type, skg_tex_fmt_ format, int32_t width, int32_t height, int32_t array_layer) {
	skg_tex_t result = {};
	result.type        = type;
	result.use         = skg_use_static;
	result.mips        = skg_mip_none;
	result.format      = format;
	result.width       = width;
	result.height      = height;
	result.array_count = 1;
	result.array_start = array_layer;
	result.multisample = 1;
	result._physical_multisample = 1;
	result._texture    = (uint32_t)(uint64_t)native_tex;
	result._format     = (uint32_t)skg_tex_fmt_to_native(result.format);
	result._target     = gl_tex_target(type, 2, result._physical_multisample);

	if (type == skg_tex_type_rendertarget) {
		glGenFramebuffers(1, &result._framebuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, result._framebuffer);
		gl_framebuffer_attach(result._texture, result._target, result.format, result._physical_multisample, result.multisample, 2, array_layer, 0);

		glBindFramebuffer(GL_FRAMEBUFFER, gl_current_framebuffer);
	}

	return result;
}

///////////////////////////////////////////

skg_tex_t skg_tex_create(skg_tex_type_ type, skg_use_ use, skg_tex_fmt_ format, skg_mip_ mip_maps) {
	skg_tex_t result = {};
	result.type    = type;
	result.use     = use;
	result.format  = format;
	result.mips    = mip_maps;
	result._format = (uint32_t)skg_tex_fmt_to_native(result.format);

	if      (use & skg_use_compute_read && use & skg_use_compute_write) result._access = GL_READ_WRITE;
	else if (use & skg_use_compute_read)                                result._access = GL_READ_ONLY;
	else if (use & skg_use_compute_write)                               result._access = GL_WRITE_ONLY;
	result._format = (uint32_t)skg_tex_fmt_to_native(result.format);

	glGenTextures(1, &result._texture);
	skg_tex_settings(&result, type == skg_tex_type_cubemap ? skg_tex_address_clamp : skg_tex_address_repeat, skg_tex_sample_linear, 1);

	if (type == skg_tex_type_rendertarget) {
		glGenFramebuffers(1, &result._framebuffer);
	}
	
	return result;
}

///////////////////////////////////////////

void skg_tex_name(skg_tex_t *tex, const char* name) {
	if (tex->_texture != 0)
		glObjectLabel(GL_TEXTURE, tex->_texture, (uint32_t)strlen(name), name);

	char postfix_name[256];
	if (tex->_framebuffer != 0) {
		snprintf(postfix_name, sizeof(postfix_name), "%s_framebuffer", name);
		// If the framebuffer hasn't been created, labeling it can error out,
		// binding it can force creation and fix that!
		glBindFramebuffer(GL_FRAMEBUFFER, tex->_framebuffer);
		glObjectLabel    (GL_FRAMEBUFFER, tex->_framebuffer, (uint32_t)strlen(postfix_name), postfix_name);
	}
	if (tex->_framebuffer_layers) {
		for (int32_t i = 0; i<tex->array_count; i+=1) {
			snprintf(postfix_name, sizeof(postfix_name), "%s_framebuffer_layer_%d", name, i);
			glBindFramebuffer(GL_FRAMEBUFFER, tex->_framebuffer_layers[i]);
			glObjectLabel    (GL_FRAMEBUFFER, tex->_framebuffer_layers[i], (uint32_t)strlen(postfix_name), postfix_name);
		}
	}
}

///////////////////////////////////////////

bool skg_tex_is_valid(const skg_tex_t *tex) {
	return tex->_target != 0;
}

///////////////////////////////////////////

void skg_tex_copy_to(const skg_tex_t *tex, int32_t tex_surface, skg_tex_t *destination, int32_t dest_surface) {
	if (destination->width != tex->width || destination->height != tex->height) {
		skg_tex_set_contents_arr(destination, nullptr, tex->array_count, tex->width, tex->height, tex->multisample);
	}

	uint32_t err = glGetError();
	while(err) {
		skg_logf(skg_log_warning, "err: %x", err);
		err = glGetError();
	}

	if (tex_surface == -1 && dest_surface == -1 && destination->array_count > 1) {
		if (tex->array_count != destination->array_count) skg_log(skg_log_critical, "Mismatching texture array count.");

		for (int32_t i=0; i<destination->array_count; i+=1) {
			glBindFramebuffer(GL_READ_FRAMEBUFFER, tex        ->_framebuffer_layers[i]);
			glBindFramebuffer(GL_DRAW_FRAMEBUFFER, destination->_framebuffer_layers[i]);

			glBlitFramebuffer(0, 0, tex->width, tex->height, 0, 0, tex->width, tex->height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		}
	} else {
		glBindFramebuffer(GL_READ_FRAMEBUFFER, tex_surface >= 0 
			? tex->_framebuffer_layers[tex_surface] 
			: tex->_framebuffer);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dest_surface >= 0 
			? destination->_framebuffer_layers[dest_surface]
			: destination->_framebuffer);

		glBlitFramebuffer(0, 0, tex->width, tex->height, 0, 0, tex->width, tex->height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	}

	err = glGetError();
	while(err) {
		skg_logf(skg_log_warning, "blit err: %x", err);
		err = glGetError();
	}
}

///////////////////////////////////////////

void skg_tex_copy_to_swapchain(const skg_tex_t *tex, skg_swapchain_t *destination) {
	skg_swapchain_bind(destination);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, tex->_framebuffer);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
	glBlitFramebuffer(0,0,tex->width,tex->height,0,0,tex->width,tex->height,  GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT, GL_NEAREST);
}

///////////////////////////////////////////

void skg_tex_attach_depth(skg_tex_t *tex, skg_tex_t *depth) {
	if (tex->type != skg_tex_type_rendertarget) {
		skg_log(skg_log_warning, "Can't bind a depth texture to a non-rendertarget");
		return;
	}
	if (tex->array_count != depth->array_count) {
		skg_log(skg_log_warning, "Mismatching array count for depth texture");
		return;
	}
	
	glBindFramebuffer(GL_FRAMEBUFFER, tex->_framebuffer);
	gl_framebuffer_attach(depth->_texture, depth->_target, depth->format, depth->_physical_multisample, depth->multisample, depth->array_count, 0, 0);

	// Attach depth to the per-layer framebuffers
	if (depth->array_count > 1) {
		for (int32_t i = 0; i < depth->array_count; i++) {
			glBindFramebuffer(GL_FRAMEBUFFER, depth->_framebuffer_layers[i]);
			gl_framebuffer_attach(depth->_texture, depth->_target, depth->format, depth->_physical_multisample, depth->multisample, depth->array_count, i, 0);
		}
	}
}

///////////////////////////////////////////

void skg_tex_settings(skg_tex_t *tex, skg_tex_address_ address, skg_tex_sample_ sample, int32_t anisotropy) {
	tex->_address    = address;
	tex->_sample     = sample;
	tex->_anisotropy = anisotropy;

	uint32_t mode;
	switch (address) {
	case skg_tex_address_clamp:  mode = GL_CLAMP_TO_EDGE;   break;
	case skg_tex_address_repeat: mode = GL_REPEAT;          break;
	case skg_tex_address_mirror: mode = GL_MIRRORED_REPEAT; break;
	default: mode = GL_REPEAT;
	}

	uint32_t filter, min_filter;
	switch (sample) {
	case skg_tex_sample_linear:     filter = GL_LINEAR;  min_filter = tex->mips == skg_mip_generate ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR; break; // Technically trilinear
	case skg_tex_sample_point:      filter = GL_NEAREST; min_filter = GL_NEAREST;                                                          break;
	case skg_tex_sample_anisotropic:filter = GL_LINEAR;  min_filter = tex->mips == skg_mip_generate ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR; break;
	default: filter = GL_LINEAR; min_filter = GL_LINEAR;
	}

	if (!skg_tex_is_valid(tex)) return;
	// Multisample textures throw errors if you try to set sampler states.
	if (tex->_target == GL_TEXTURE_2D_MULTISAMPLE || tex->_target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY) return;

	glBindTexture(tex->_target, tex->_texture);

	glTexParameteri(tex->_target, GL_TEXTURE_WRAP_S, mode);
	glTexParameteri(tex->_target, GL_TEXTURE_WRAP_T, mode);
	if (tex->type == skg_tex_type_cubemap) {
		glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, mode);
	}
	glTexParameteri(tex->_target, GL_TEXTURE_MIN_FILTER, min_filter);
	glTexParameteri(tex->_target, GL_TEXTURE_MAG_FILTER, filter    );
#ifdef _SKG_GL_DESKTOP
	glTexParameterf(tex->_target, GL_TEXTURE_MAX_ANISOTROPY_EXT, sample == skg_tex_sample_anisotropic ? anisotropy : 1.0f);
#endif
}

///////////////////////////////////////////

void skg_tex_set_contents(skg_tex_t *tex, const void *data, int32_t width, int32_t height) {
	const void *data_arr[1] = { data };
	return skg_tex_set_contents_arr(tex, data_arr, 1, width, height, 1);
}

///////////////////////////////////////////

void skg_tex_set_contents_arr(skg_tex_t *tex, const void **data_frames, int32_t data_frame_count, int32_t width, int32_t height, int32_t multisample) {
	if (multisample > 1) {
		int32_t max_samples = 0;
		glGetIntegerv(GL_MAX_SAMPLES, &max_samples);
		if (multisample > max_samples)
			multisample = max_samples;
	}
#ifdef _SKG_GL_WEB
	multisample = 1;
#endif

	tex->width                 = width;
	tex->height                = height;
	tex->array_count           = data_frame_count;
	tex->multisample           = multisample; // multisample render to texture is technically an MSAA surface, but functions like a normal single sample texture.
	tex->_physical_multisample = gl_caps[skg_cap_tiled_multisample] ? 1 : multisample;
	tex->_target               = gl_tex_target(tex->type, tex->array_count, tex->_physical_multisample);

	glBindTexture(tex->_target, tex->_texture);

	if (tex->format == skg_tex_fmt_r8)
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	else if (tex->format == skg_tex_fmt_r16u || tex->format == skg_tex_fmt_r16s || tex->format == skg_tex_fmt_r16f || tex->format == skg_tex_fmt_r8g8 || tex->format == skg_tex_fmt_depth16)
		glPixelStorei(GL_UNPACK_ALIGNMENT, 2);

	tex->_format    = (uint32_t)skg_tex_fmt_to_native   (tex->format);
	uint32_t layout =           skg_tex_fmt_to_gl_layout(tex->format);
	uint32_t type   =           skg_tex_fmt_to_gl_type  (tex->format);
	if (tex->type == skg_tex_type_cubemap) {
		if (data_frame_count != 6) {
			skg_log(skg_log_warning, "Cubemaps need 6 data frames");
			return;
		}
		for (int32_t f = 0; f < 6; f++)
			glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+f , 0, tex->_format, width, height, 0, layout, type, data_frames[f]);
	} else {
#ifndef _SKG_GL_WEB
		if      (tex->_target == GL_TEXTURE_2D_MULTISAMPLE)       { glTexStorage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE,       tex->multisample, tex->_format, width, height, true); }
		else if (tex->_target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY) { glTexStorage3DMultisample(GL_TEXTURE_2D_MULTISAMPLE_ARRAY, tex->multisample, tex->_format, width, height, data_frame_count, true); }
		else if (tex->_target == GL_TEXTURE_2D_ARRAY)             { glTexImage3D             (GL_TEXTURE_2D_ARRAY, 0, tex->_format, width, height, data_frame_count, 0, layout, type, data_frames == nullptr ? nullptr : data_frames[0]); }
		else                                                      { glTexImage2D             (GL_TEXTURE_2D,       0, tex->_format, width, height, 0, layout, type, data_frames == nullptr ? nullptr : data_frames[0]); }
#else
		glTexImage2D(GL_TEXTURE_2D, 0, tex->_format, width, height, 0, layout, type, data_frames == nullptr ? nullptr : data_frames[0]);
#endif
	}

	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

	if (tex->mips == skg_mip_generate)
		glGenerateMipmap(tex->_target);

	if (tex->type == skg_tex_type_rendertarget) {
		glBindFramebuffer(GL_FRAMEBUFFER, tex->_framebuffer);
		gl_framebuffer_attach(tex->_texture, tex->_target, tex->format, tex->_physical_multisample, tex->multisample, tex->array_count, 0, 0);

		// Add framebuffers for individual layers of any array surfaces
		if (tex->array_count > 1) {
			tex->_framebuffer_layers = (uint32_t*)malloc(sizeof(uint32_t) * tex->array_count);
			glGenFramebuffers(tex->array_count, tex->_framebuffer_layers);

			for (int32_t i = 0; i < tex->array_count; i++) {
				glBindFramebuffer(GL_FRAMEBUFFER, tex->_framebuffer_layers[i]);
				gl_framebuffer_attach(tex->_texture, tex->_target, tex->format, tex->_physical_multisample, tex->multisample, tex->array_count, i, 0);
			}
		}

		glBindFramebuffer(GL_FRAMEBUFFER, gl_current_framebuffer);
	}

	skg_tex_settings(tex, tex->_address, tex->_sample, tex->_anisotropy);
}

///////////////////////////////////////////

bool skg_tex_get_contents(skg_tex_t *tex, void *ref_data, size_t data_size) {
	return skg_tex_get_mip_contents_arr(tex, 0, 0, ref_data, data_size);
}

///////////////////////////////////////////

bool skg_tex_get_mip_contents(skg_tex_t *tex, int32_t mip_level, void *ref_data, size_t data_size) {
	return skg_tex_get_mip_contents_arr(tex, mip_level, 0, ref_data, data_size);
}

///////////////////////////////////////////

bool skg_tex_get_mip_contents_arr(skg_tex_t *tex, int32_t mip_level, int32_t arr_index, void *ref_data, size_t data_size) {
	uint32_t result = glGetError();
	while (result != 0) {
		char text[128];
		snprintf(text, 128, "skg_tex_get_mip_contents_arr: eating a gl error from somewhere else: %d", result);
		skg_log(skg_log_warning, text);
		result = glGetError();
	}
	
	// Double check on mips first
	int32_t mip_levels = tex->mips == skg_mip_generate ? (int32_t)skg_mip_count(tex->width, tex->height) : 1;
	if (mip_level != 0) {
		if (tex->mips != skg_mip_generate) {
			skg_log(skg_log_critical, "Can't get mip data from a texture with no mips!");
			return false;
		}
		if (mip_level >= mip_levels) {
			skg_log(skg_log_critical, "This texture doesn't have quite as many mip levels as you think.");
			return false;
		}
	}

	// Make sure we've been provided enough memory to hold this texture
	int32_t width       = 0;
	int32_t height      = 0;
	size_t  format_size = skg_tex_fmt_size(tex->format);
	skg_mip_dimensions(tex->width, tex->height, mip_level, &width, &height);

	if (data_size != (size_t)width * (size_t)height * format_size) {
		skg_log(skg_log_critical, "Insufficient buffer size for skg_tex_get_mip_contents_arr");
		return false;
	}

	int64_t layout = skg_tex_fmt_to_gl_layout(tex->format);
	glBindTexture (tex->_target, tex->_texture);

#if defined(_SKG_GL_WEB) || defined(_SKG_GL_ES)
	// Referenced from here:
	// https://stackoverflow.com/questions/53993820/opengl-es-2-0-android-c-glgetteximage-alternative
	uint32_t fbo = 0;
	glGenFramebuffers     (1, &fbo); 
	glBindFramebuffer     (GL_FRAMEBUFFER, fbo);
	if (tex->_target == GL_TEXTURE_CUBE_MAP) {
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_CUBE_MAP_POSITIVE_X+arr_index, tex->_texture, mip_level);
	} else {
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex->_target, tex->_texture, mip_level);
	}

	glReadPixels(0, 0, width, height, layout, skg_tex_fmt_to_gl_type(tex->format), ref_data);

	glBindFramebuffer   (GL_FRAMEBUFFER, 0);
	glDeleteFramebuffers(1, &fbo);
#else
	glBindTexture(tex->_target, tex->_texture);

	if (tex->_target == GL_TEXTURE_CUBE_MAP) {
		glGetTexImage(GL_TEXTURE_CUBE_MAP_POSITIVE_X+arr_index, mip_level, (uint32_t)layout, skg_tex_fmt_to_gl_type(tex->format), ref_data);
	} else {
		glGetTexImage(tex->_target, mip_level, (uint32_t)layout, skg_tex_fmt_to_gl_type(tex->format), ref_data);
	}
#endif

	result = glGetError();
	if (result != 0) {
		char text[128];
		snprintf(text, 128, "skg_tex_get_mip_contents_arr error: %d", result);
		skg_log(skg_log_critical, text);
	}

	return result == 0;
}

///////////////////////////////////////////

void* skg_tex_get_native(const skg_tex_t* tex) {
	return (void*)((uint64_t)tex->_texture);
}

///////////////////////////////////////////

void skg_tex_bind(const skg_tex_t *texture, skg_bind_t bind) {
	if (bind.stage_bits & skg_stage_compute) {
#if !defined(_SKG_GL_WEB)
		glBindImageTexture(bind.slot, texture->_texture, 0, false, 0, texture->_access, (uint32_t)skg_tex_fmt_to_native( texture->format ));
#endif
	} else {
		glActiveTexture(GL_TEXTURE0 + bind.slot);
		glBindTexture(texture->_target, texture->_texture);
	}
}

///////////////////////////////////////////

void skg_tex_clear(skg_bind_t bind) {
}

///////////////////////////////////////////

void skg_tex_destroy(skg_tex_t *tex) {
	uint32_t tex_list[] = { tex->_texture     };
	uint32_t fb_list [] = { tex->_framebuffer };
	if (tex->_texture    ) glDeleteTextures    (1, tex_list);
	if (tex->_framebuffer) glDeleteFramebuffers(1, fb_list );  
	if (tex->_framebuffer_layers) {
		glDeleteFramebuffers(tex->array_count, tex->_framebuffer_layers);  
		free(tex->_framebuffer_layers);
	}
	*tex = {};
}

///////////////////////////////////////////

uint32_t skg_buffer_type_to_gl(skg_buffer_type_ type) {
	switch (type) {
	case skg_buffer_type_vertex:   return GL_ARRAY_BUFFER;
	case skg_buffer_type_index:    return GL_ELEMENT_ARRAY_BUFFER;
	case skg_buffer_type_constant: return GL_UNIFORM_BUFFER;
	case skg_buffer_type_compute:  return GL_SHADER_STORAGE_BUFFER;
	default: return 0;
	}
}

///////////////////////////////////////////

int64_t skg_tex_fmt_to_native(skg_tex_fmt_ format) {
	switch (format) {
	case skg_tex_fmt_rgba32:        return GL_SRGB8_ALPHA8;
	case skg_tex_fmt_rgba32_linear: return GL_RGBA8;
	case skg_tex_fmt_bgra32:        return GL_RGBA8;
	case skg_tex_fmt_bgra32_linear: return GL_RGBA8;
	case skg_tex_fmt_rg11b10:       return GL_R11F_G11F_B10F;
	case skg_tex_fmt_rgb10a2:       return GL_RGB10_A2;
	case skg_tex_fmt_rgba64u:       return GL_RGBA16;
	case skg_tex_fmt_rgba64s:       return GL_RGBA16_SNORM;
	case skg_tex_fmt_rgba64f:       return GL_RGBA16F;
	case skg_tex_fmt_rgba128:       return GL_RGBA32F;
	case skg_tex_fmt_depth16:       return GL_DEPTH_COMPONENT16;
	case skg_tex_fmt_depth32:       return GL_DEPTH_COMPONENT32F;
	case skg_tex_fmt_depthstencil:  return GL_DEPTH24_STENCIL8;
	case skg_tex_fmt_r8:            return GL_R8;
	case skg_tex_fmt_r16u:          return GL_R16;
	case skg_tex_fmt_r16s:          return GL_R16_SNORM;
	case skg_tex_fmt_r16f:          return GL_R16F;
	case skg_tex_fmt_r32:           return GL_R32F;
	case skg_tex_fmt_r8g8:          return GL_RG8;
	default: return 0;
	}
}

///////////////////////////////////////////

skg_tex_fmt_ skg_tex_fmt_from_native(int64_t format) {
	switch (format) {
	case GL_SRGB8_ALPHA8:       return skg_tex_fmt_rgba32;
	case GL_RGBA8:              return skg_tex_fmt_rgba32_linear;
	case GL_R11F_G11F_B10F:     return skg_tex_fmt_rg11b10;
	case GL_RGB10_A2:           return skg_tex_fmt_rgb10a2;
	case GL_RGBA16:             return skg_tex_fmt_rgba64u;
	case GL_RGBA16_SNORM:       return skg_tex_fmt_rgba64s;
	case GL_RGBA16F:            return skg_tex_fmt_rgba64f;
	case GL_RGBA32F:            return skg_tex_fmt_rgba128;
	case GL_DEPTH_COMPONENT16:  return skg_tex_fmt_depth16;
	case GL_DEPTH_COMPONENT32F: return skg_tex_fmt_depth32;
	case GL_DEPTH24_STENCIL8:   return skg_tex_fmt_depthstencil;
	case GL_R8:                 return skg_tex_fmt_r8;
	case GL_R16:                return skg_tex_fmt_r16u;
	case GL_R16F:               return skg_tex_fmt_r16f;
	case GL_R16_SNORM:          return skg_tex_fmt_r16s;
	case GL_R32F:               return skg_tex_fmt_r32;
	case GL_RG8:                return skg_tex_fmt_r8g8;
	default: return skg_tex_fmt_none;
	}
}

///////////////////////////////////////////

uint32_t skg_tex_fmt_to_gl_layout(skg_tex_fmt_ format) {
	switch (format) {
	case skg_tex_fmt_rgba32:
	case skg_tex_fmt_rgba32_linear:
	case skg_tex_fmt_rgb10a2:
	case skg_tex_fmt_rgba64u:
	case skg_tex_fmt_rgba64s:
	case skg_tex_fmt_rgba64f:
	case skg_tex_fmt_rgba128:       return GL_RGBA;
	case skg_tex_fmt_rg11b10:       return GL_RGB;
	case skg_tex_fmt_bgra32:
	case skg_tex_fmt_bgra32_linear:
		#ifdef _SKG_GL_WEB // WebGL has no GL_BGRA?
		return GL_RGBA;
		#else
		return GL_BGRA;
		#endif
	case skg_tex_fmt_depth16:
	case skg_tex_fmt_depth32:       return GL_DEPTH_COMPONENT;
	case skg_tex_fmt_depthstencil:  return GL_DEPTH_STENCIL;
	case skg_tex_fmt_r8:
	case skg_tex_fmt_r16u:
	case skg_tex_fmt_r16s:
	case skg_tex_fmt_r16f:
	case skg_tex_fmt_r32:           return GL_RED;
	case skg_tex_fmt_r8g8:          return GL_RG;
	default: return 0;
	}
}

///////////////////////////////////////////

uint32_t skg_tex_fmt_to_gl_type(skg_tex_fmt_ format) {
	switch (format) {
	case skg_tex_fmt_rgba32:        return GL_UNSIGNED_BYTE;
	case skg_tex_fmt_rgba32_linear: return GL_UNSIGNED_BYTE;
	case skg_tex_fmt_bgra32:        return GL_UNSIGNED_BYTE;
	case skg_tex_fmt_bgra32_linear: return GL_UNSIGNED_BYTE;
	case skg_tex_fmt_rgb10a2:       return GL_FLOAT;
	case skg_tex_fmt_rg11b10:       return GL_FLOAT;
	case skg_tex_fmt_rgba64u:       return GL_UNSIGNED_SHORT;
	case skg_tex_fmt_rgba64s:       return GL_SHORT;
	case skg_tex_fmt_rgba64f:       return GL_HALF_FLOAT;
	case skg_tex_fmt_rgba128:       return GL_FLOAT;
	case skg_tex_fmt_depth16:       return GL_UNSIGNED_SHORT;
	case skg_tex_fmt_depth32:       return GL_FLOAT;
	case skg_tex_fmt_depthstencil:  return GL_UNSIGNED_INT_24_8;
	case skg_tex_fmt_r8:            return GL_UNSIGNED_BYTE;
	case skg_tex_fmt_r16u:          return GL_UNSIGNED_SHORT;
	case skg_tex_fmt_r16s:          return GL_SHORT;
	case skg_tex_fmt_r16f:          return GL_HALF_FLOAT;
	case skg_tex_fmt_r32:           return GL_FLOAT;
	case skg_tex_fmt_r8g8:          return GL_UNSIGNED_BYTE;
	default: return 0;
	}
}

#endif