// https://simoncoenen.com/blog/programming/graphics/DxcRevised.html

#define _CRT_SECURE_NO_WARNINGS

#include <sys/stat.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#if defined(_WIN32)
#include <windows.h>
#include <direct.h>
#elif defined(__linux__) || defined(__APPLE__)
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <libgen.h>
#endif

#define SKG_IMPL
#define SKG_FORCE_NULL 
#include "../sk_gpu.h"

#define SKSC_IMPL
#include "sksc.h"

#include "miniz.h"

///////////////////////////////////////////

typedef struct compiler_settings_t {
	bool replace_ext;
	bool output_header;
	bool output_zipped;
	bool output_raw_shaders;
	bool only_if_changed;
	char *out_folder;

	sksc_settings_t shaderc;
} compiler_settings_t;

///////////////////////////////////////////

uint64_t exe_file_time = 0;
const int32_t path_size = 2048;

///////////////////////////////////////////

bool                read_file     (const char *filename, char **out_text, size_t *out_size);
bool                write_file    (const char *filename, void *file_data, size_t file_size);
bool                write_file_txt(const char *filename, void *file_data, size_t file_size);
bool                write_header  (const char *filename, void *file_data, size_t file_size, bool zipped);
void                write_stages  (const skg_shader_file_t *file, const char *folder, bool trailing_slash, const char *name_ext);
void                compile_file  (const char *filename, compiler_settings_t *settings);
void                iterate_dir   (const char *directory_path, void *callback_data, void (*on_item)(void *callback_data, const char *name, bool file));
compiler_settings_t check_settings(int32_t argc, char **argv, bool *exit); 
void                show_usage    ();
uint64_t            file_time     (const char *file);
void                file_name     (const char *file, char *out_name, size_t name_size);
void                file_name_ext (const char *file, char *out_name, size_t name_size);
void                file_dir      (const char *file, char *out_path, size_t path_size);
bool                file_exists   (const char *path);
bool                path_is_file  (const char *path);
bool                path_is_wild  (const char *path);
char               *path_absolute (const char *relative_dir);
bool                recurse_mkdir (const char *dirname);

///////////////////////////////////////////

int main(int argc, char **argv) {
	bool                exit     = false;
	compiler_settings_t settings = check_settings(argc, argv, &exit);
	if (exit) return 0;

	exe_file_time = file_time(argv[0]);

	sksc_init();

#if defined(_WIN32)
	const char *path = argv[argc - 1];
	if (path_is_file(path) && !path_is_wild(path)) {
		compile_file(path, &settings);
	} else {
		iterate_dir(path, &settings, [](void *callback_data, const char *src_filename, bool file) {
			if (!file) return;
			compile_file(src_filename, (compiler_settings_t*)callback_data);
		});
	}
#else
	for (size_t i = 1; i < argc; i++) {
		if (file_exists(argv[i])) {
			compile_file(argv[i], &settings);
		}
	}
#endif

	sksc_shutdown();

	return 0;
}

///////////////////////////////////////////

compiler_settings_t check_settings(int32_t argc, char **argv, bool *exit) {
	if (argc <= 1) {
		*exit = true;
		show_usage();
		return {};
	}

	compiler_settings_t result = {};
	result.replace_ext     = false;
	result.output_header   = false;
	result.only_if_changed = true;
	result.shaderc.debug         = false;
	result.shaderc.optimize      = 3;
	result.shaderc.row_major     = false;
	result.shaderc.silent_err    = false;
	result.shaderc.silent_info   = false;
	result.shaderc.silent_warn   = false;

	// Get the inlcude folder
	file_dir(argv[argc-1], result.shaderc.folder, sizeof(result.shaderc.folder));

	bool set_targets = false;
	for (int32_t i=1; i<argc-1; i++) {
		if      (strcmp(argv[i], "-h" ) == 0) result.output_header         = true;
		else if (strcmp(argv[i], "-z" ) == 0) result.output_zipped         = true;
		else if (strcmp(argv[i], "-raw")== 0) result.output_raw_shaders    = true;
		else if (strcmp(argv[i], "-e" ) == 0) result.replace_ext           = false;
		else if (strcmp(argv[i], "-f" ) == 0) result.only_if_changed       = false;
		else if (strcmp(argv[i], "-r" ) == 0) result.shaderc.row_major     = true;
		else if (strcmp(argv[i], "-d" ) == 0) result.shaderc.debug         = true;
		else if (strcmp(argv[i], "-si") == 0) result.shaderc.silent_info   = true;
		else if (strcmp(argv[i], "-sw") == 0) { result.shaderc.silent_info = true; result.shaderc.silent_warn = true; }
		else if (strcmp(argv[i], "-s" ) == 0) { result.shaderc.silent_err  = true; result.shaderc.silent_info = true; result.shaderc.silent_warn = true;}
		else if (strcmp(argv[i], "-o0") == 0 ||
		         strcmp(argv[i], "-O0") == 0) result.shaderc.optimize = 0;
		else if (strcmp(argv[i], "-o1") == 0 ||
		         strcmp(argv[i], "-O1") == 0) result.shaderc.optimize = 1;
		else if (strcmp(argv[i], "-o2") == 0 ||
		         strcmp(argv[i], "-O2") == 0) result.shaderc.optimize = 2;
		else if (strcmp(argv[i], "-o3") == 0 ||
		         strcmp(argv[i], "-O3") == 0) result.shaderc.optimize = 3;
		else if (strcmp(argv[i], "-help" ) == 0 ||
		         strcmp(argv[i], "-?" ) == 0 ||
		         strcmp(argv[i], "--help" ) == 0 ||
		         strcmp(argv[i], "/?" ) == 0 ) *exit = true;
		else if (strcmp(argv[i], "-cs") == 0 && i<argc-1) { result.shaderc.cs_entry_require = true; strncpy(result.shaderc.cs_entrypoint, argv[i+1], sizeof(result.shaderc.cs_entrypoint)); i++; }
		else if (strcmp(argv[i], "-vs") == 0 && i<argc-1) { result.shaderc.vs_entry_require = true; strncpy(result.shaderc.vs_entrypoint, argv[i+1], sizeof(result.shaderc.vs_entrypoint)); i++; }
		else if (strcmp(argv[i], "-ps") == 0 && i<argc-1) { result.shaderc.ps_entry_require = true; strncpy(result.shaderc.ps_entrypoint, argv[i+1], sizeof(result.shaderc.ps_entrypoint)); i++; }
		else if (strcmp(argv[i], "-gl") == 0 && i<argc-1) { result.shaderc.gl_version = atoi(argv[i+1]); i++; }
		else if (strcmp(argv[i], "-m" ) == 0 && i<argc-1) { strncpy(result.shaderc.shader_model,  argv[i+1], sizeof(result.shaderc.shader_model )); i++; }
		else if (strcmp(argv[i], "-i" ) == 0 && i<argc-1) {
			size_t len = strlen(argv[i + 1]) + 1;
			result.shaderc.include_folder_ct += 1;
			result.shaderc.include_folders    = (char**)realloc(result.shaderc.include_folders, sizeof(result.shaderc.include_folder_ct * sizeof(char *)));
			result.shaderc.include_folders[result.shaderc.include_folder_ct-1] = (char*)malloc(len);
			strncpy(result.shaderc.include_folders[result.shaderc.include_folder_ct-1], argv[i+1], len); 
			i++; }
		else if (strcmp(argv[i], "-o" ) == 0) { 
			size_t len = strlen(argv[i + 1]) + 1;
			result.out_folder = (char*)malloc(len);
			strncpy(result.out_folder, argv[i+1], len); 
			i++; }
		else if (strcmp(argv[i], "-t" ) == 0 && i<argc-1) {
			set_targets = true;
			const char *targets = argv[i + 1];
			size_t      len     = strlen(targets);
			for (size_t i = 0; i < len; i++) {
				if      (targets[i] == 'x') result.shaderc.target_langs[skg_shader_lang_hlsl]     = true;
				else if (targets[i] == 's') result.shaderc.target_langs[skg_shader_lang_spirv]    = true;
				else if (targets[i] == 'g') result.shaderc.target_langs[skg_shader_lang_glsl]     = true;
				else if (targets[i] == 'e') result.shaderc.target_langs[skg_shader_lang_glsl_es]  = true;
				else if (targets[i] == 'w') result.shaderc.target_langs[skg_shader_lang_glsl_web] = true;
				else { printf("Unrecognized shader language target '%c'\n", targets[i]); *exit = true; }
			}
			i++;
		}
		else if (file_exists(argv[i])) {}
		else { printf("Unrecognized option '%s'\n", argv[i]); *exit = true; }
	}

	// Default language targets, all of them
	if (!set_targets) {
		for (size_t i = 0; i < sizeof(result.shaderc.target_langs)/sizeof(result.shaderc.target_langs[0]); i++) {
			result.shaderc.target_langs[i] = true;
		}
	}

	// Default shader model
	if (result.shaderc.shader_model[0] == 0)
		strncpy(result.shaderc.shader_model, "5_0", sizeof(result.shaderc.shader_model));

	// default desktop glsl version
	if (result.shaderc.gl_version == 0)
		result.shaderc.gl_version = 430;

	// If no entrypoints were provided, then these are the defaults!
	if (result.shaderc.ps_entrypoint[0] == 0 && result.shaderc.vs_entrypoint[0] == 0 && result.shaderc.cs_entrypoint[0] == 0) {
		strncpy(result.shaderc.ps_entrypoint, "ps", sizeof(result.shaderc.ps_entrypoint));
		strncpy(result.shaderc.vs_entrypoint, "vs", sizeof(result.shaderc.vs_entrypoint));
		strncpy(result.shaderc.cs_entrypoint, "cs", sizeof(result.shaderc.cs_entrypoint));
	}

	if (*exit) {
		show_usage();
	}
	return result;
}

///////////////////////////////////////////

void show_usage() {
	printf(R"_(
Usage: skshaderc [options] target_file

Options:
	-r		Specify row-major matrices, column-major is default.
	-h		Output a C header file with a byte array instead of a binary file.
	-z		Zips and compresses output data with miniz
	-raw		Outputs the raw shader stage data as additional files in the same
			directory. Useful for debugging shader transpilation.
	-e		Appends the sks extension to the resulting file instead of 
			replacing the extension with sks. Default will replace the 
			extension.
	-s		Silent, no errors, warnings or info are printed when compiling 
			shaders.
	-sw		No info or warnings are printed when compiling shaders.
	-si		No info is printed when compiling shaders.
	-f		Force the shader to recompile, even if the timestamp on the 
			matching .sks file is newer.
	
	-d		Compile shaders with debug info embedded. Enabling this will
			disable shader optimizations.
	-o0		Optimization level 0. Default is 3.
	-o1		Optimization level 1. Default is 3.
	-o2		Optimization level 2. Default is 3.
	-o3		Optimization level 3. Default is 3.

	-cs name	Compiles a compute shader stage from this file, using an entry
			function of [name]. Specifying this removes the default entry 
			names from vertex and pixel shader stages. Default is 'cs'.
	-ps name	Compiles a pixel shader stage from this file, using an entry
			function of [name]. Specifying this removes default entry
			names from vertex and pixel shader stages. Default is 'ps'.
	-vs name	Compiles a vertex shader stage from this file, using an entry
			function of [name]. Specifying this removes default entry
			names from vertex and pixel shader stages. Default is 'vs'.
	-m		Lets you set the shader model used for compiling, default is
			5_0. This may not be implemented yet.

	-i folder	Adds a folder to the include path when searching for #include
			files.
	-o folder	Sets the output folder for compiled shaders. Default will 
			leave them in the same folder as the original file.
	-gl version	Sets the target GLSL version for generated desktop OpenGL 
			shaders. By default this is '430'.
	-t targets	Sets a list of shader language targets to generate. This is a
			string of characters, where each character represents a language.
			'x' is d3d11 dxil, 's' is d3d12 and vulkan spir-v, 'g' is desktop
			GLSL, 'w' is web GLSL, and 'e' is GLES GLSL. Default value is 
			'xsgwe'.

	target_file	This can be any filename, and can use the wildcard '*' to 
			compile multiple files in the same call.
)_");
}

///////////////////////////////////////////

void compile_file(const char *src_filename, compiler_settings_t *settings) {
	char dir     [path_size];
	char name    [path_size];
	char name_ext[path_size];
	file_dir     (src_filename, dir,      sizeof(dir));
	file_name    (src_filename, name,     sizeof(name));
	file_name_ext(src_filename, name_ext, sizeof(name_ext));

	char        new_filename[path_size];
	const char *dest_folder    = settings->out_folder ? settings->out_folder : dir;
	char        trailing_char  = dest_folder[strlen(dest_folder) - 1];
	bool        trailing_slash = trailing_char == '/' || trailing_char == '\\';
	if (settings->replace_ext) {
		snprintf(new_filename, sizeof(new_filename), "%s%s%s.%s", dest_folder, trailing_slash ? "":"/", name,     settings->output_header?"h":"sks");
	} else {
		snprintf(new_filename, sizeof(new_filename), "%s%s%s.%s", dest_folder, trailing_slash ? "":"/", name_ext, settings->output_header?"h":"sks");
	}

	// Skip this file if it hasn't changed 
	uint64_t src_file_time      = file_time(src_filename);
	uint64_t compiled_file_time = file_time(new_filename);
	if (settings->only_if_changed && src_file_time < compiled_file_time && exe_file_time < compiled_file_time) {
		if (!settings->shaderc.silent_info) {
			printf("File '%s' is already up-to-date, skipping...\n", src_filename);
		}
		return;
	}

	char  *file_text;
	size_t file_size;
	if (read_file(src_filename, &file_text, &file_size)) {
		skg_shader_file_t file;
		sksc_log(log_level_info, "Compiling %s..", src_filename);
		if (sksc_compile(src_filename, file_text, &settings->shaderc, &file)) {
			
			// Turn the shader data into a binary file
			void  *sks_data;
			size_t sks_size;
			sksc_build_file(&file, &sks_data, &sks_size);
			
			// Zip data
			bool err = false;
			if (settings->output_zipped) {
				mz_ulong sks_size_z = mz_compressBound((mz_ulong)sks_size);
				void*    sks_data_z = malloc(sks_size_z);
				
				int status = mz_compress2((unsigned char*)sks_data_z, &sks_size_z, (unsigned char*)sks_data, (mz_ulong)sks_size, MZ_BEST_COMPRESSION);
				if (status != MZ_OK) {
					sksc_log(log_level_err, "Failed to compress data! %d\n", status);
					err = true;
				}
				
				free(sks_data);
				sks_data = sks_data_z;
				sks_size = sks_size_z;
			}

			// Write to file
			if (!err) {
				// Make sure the folder exists
				char folder[path_size];
				file_dir(new_filename, folder, sizeof(folder));
				recurse_mkdir(folder);

				char* abs_file = path_absolute(new_filename);
				bool written = settings->output_header
					? write_header(abs_file, sks_data, sks_size, settings->output_zipped)
					: write_file  (abs_file, sks_data, sks_size);

				if (settings->output_raw_shaders) {
					write_stages(&file, dest_folder, trailing_slash, name_ext);
				}

				if (written) sksc_log(log_level_info, "Compiled successfully to %s", abs_file);
				else         sksc_log(log_level_err,  "Failed to write file! %s", abs_file);
			}
			free(sks_data);

			skg_shader_file_destroy(&file);
		}
		
		char* abs_src_file = path_absolute(src_filename);
		sksc_log_print(abs_src_file, &settings->shaderc);
		sksc_log_clear();
		free(file_text);
	} else {
		printf("Couldn't read file '%s'!\n", src_filename);
	}
}

///////////////////////////////////////////

void write_stages(const skg_shader_file_t *file, const char *folder, bool trailing_slash, const char *name_ext) {
	for (uint32_t i = 0; i < file->stage_count; i++) {
		skg_shader_file_stage_t *stage = &file->stages[i];

		char        sub_filename[path_size];
		const char *stage_name = "";
		const char *lang       = "";
		bool        text       = false;
		switch (stage->language) {
			case skg_shader_lang_glsl:     lang = "glsl";      text = true; break;
			case skg_shader_lang_glsl_es:  lang = "glsl.es";   text = true; break;
			case skg_shader_lang_glsl_web: lang = "glsl.web";  text = true; break;
			case skg_shader_lang_hlsl:     lang = "hlsl.bin";               break;
			case skg_shader_lang_spirv:    lang = "spirv.bin";              break;
		}
		switch(stage->stage){
			case skg_stage_compute: stage_name = "compute"; break;
			case skg_stage_pixel:   stage_name = "pixel";   break;
			case skg_stage_vertex:  stage_name = "vertex";  break;
		}
		snprintf(sub_filename, sizeof(sub_filename), "%s%s%s.%s.%s", folder, trailing_slash ? "":"/", name_ext, stage_name, lang);
		if (text) {
			write_file_txt(sub_filename, stage->code, stage->code_size-1);
		} else {
			write_file    (sub_filename, stage->code, stage->code_size);
		}
	}
}

///////////////////////////////////////////

bool read_file(const char *filename, char **out_text, size_t *out_size) {
	*out_text = nullptr;
	*out_size = 0;

	FILE *fp = fopen(filename, "rb");
	if (fp == nullptr) {
		return false;
	}

	fseek(fp, 0L, SEEK_END);
	*out_size = ftell(fp);
	rewind(fp);

	*out_text = (char*)malloc(*out_size+1);
	if (*out_text == nullptr) { *out_size = 0; fclose(fp); return false; }
	if (fread(*out_text, 1, *out_size, fp) == 0) return false;
	fclose(fp);

	(*out_text)[*out_size] = 0;
	return true;
}

///////////////////////////////////////////

#ifdef _WIN32
#define SEPARATOR '\\'
#else
#define SEPARATOR '/'
#endif

// Windows does have a _fullpath function, but there is no Linux equivalent, so
// to keep the code consistent, both will use this code.
char *path_absolute(const char *relative_dir) {
	static char result[path_size];
	size_t write_at = 0;
	result[0] = '\0';
	
	// This is not an absolute path, prefix the working directory
	if (relative_dir[0] != '/' && relative_dir[1] != ':') {
#ifdef _WIN32
		if (_getcwd(result, sizeof(result)) == nullptr)
			return nullptr;
#else
		if (getcwd(result, sizeof(result)) == nullptr)
			return nullptr;
#endif
		// Ensure that the working path ends with a separator
		write_at = strlen(result);
		if (result[write_at] != '/' && result[write_at] != '\\') {
			result[write_at] = SEPARATOR;
			write_at++;
		}
	} else if (relative_dir[0] == '/') {
		result[0] = '/';
		write_at += 1;
	}

	const char *curr  = relative_dir;
	const char *start = relative_dir;
	while (*curr != '\0') {
		// Find the next path element
		while (*curr == '\\' || *curr == '/') {
			curr++;
		}
		start = curr;
		if (*curr == '\0') break;
		while (*curr != '\\' && *curr != '/' && *curr != '\0') {
			curr++;
		}

		if (curr - start == 2 && start[0] == '.' && start[1] == '.') {
			// Handle "up" a directory, the '..' path
			write_at--;
			while (write_at > 0) {
				write_at--;
				if (result[write_at] == '/' || result[write_at] == '\\') {
					write_at++;
					break;
				}
			}
		} else if (curr - start == 1 && start[0] == '.') {
			// We should be able to more or less ignore the current directory
			// indicator, '.'
		} else {
			// Otherwise, just copy in the path element!
			memcpy(&result[write_at], start, curr - start);
			write_at += curr - start;
			
			result[write_at] = SEPARATOR;
			write_at += 1;
		}
	}
	// Terminate the path
	if (write_at > 0) write_at--;
	result[write_at] = '\0';
	
	return result;
}

#include <errno.h>
bool recurse_mkdir(const char *dirname) {
	char *full = path_absolute(dirname);
	if (full == nullptr) return false;
	
	char *curr  = full;
	while (*curr != '\0') {
		while (*curr == '\\' || *curr == '/') {
			curr++;
		}
		if (*curr == '\0') return true;
		while (*curr != '\\' && *curr != '/' && *curr != '\0') {
			curr++;
		}
		
		char old = *curr;
		*curr = '\0';
#ifdef _WIN32
		if (CreateDirectoryA(full, NULL) == FALSE) {
			if (GetLastError() != ERROR_ALREADY_EXISTS) {
				return false;
			}
		}
#else
		if (mkdir(full, 0774) != 0) {
			if (errno != EEXIST) {
				return false;
			}
		}
#endif
		*curr = old;
	}
	return true;
}

///////////////////////////////////////////

bool write_file(const char *filename, void *file_data, size_t file_size) {
	// Open and write
	FILE *fp = fopen(filename, "wb");
	if (fp == nullptr) {
		return false;
	}
	fwrite(file_data, file_size, 1, fp);
	fflush(fp);
	fclose(fp);
	return true;
}


///////////////////////////////////////////

bool write_file_txt(const char *filename, void *file_data, size_t file_size) {
	// Open and write
	FILE *fp = fopen(filename, "w");
	if (fp == nullptr) {
		return false;
	}
	fwrite(file_data, file_size, 1, fp);
	fflush(fp);
	fclose(fp);
	return true;
}

///////////////////////////////////////////

bool write_header(const char *filename, void *file_data, size_t file_size, bool zipped) {
	char name[path_size];
	file_name(filename, name, sizeof(name));

	// '.' may be common, and will bork the variable name
	size_t len = strlen(name);
	for (size_t i = 0; i < len; i++) {
		if (name[i] == '.') name[i] = '_';
	}

	FILE *fp = fopen(filename, "w");
	if (fp == nullptr) {
		return false;
	}
	fprintf(fp, "#pragma once\n\n");
	int32_t ct = fprintf(fp, "const unsigned char sks_%s%s[%zu] = {\n", name, zipped ? "_zip" : "", file_size);
	for (size_t i = 0; i < file_size; i++) {
		unsigned char byte = ((unsigned char *)file_data)[i];  
		ct += fprintf(fp, "%d,", byte);
		if (ct > 80) { 
			fprintf(fp, "\n"); 
			ct = 0; 
		}
	}
	fprintf(fp, "};\n");
	fflush(fp);
	fclose(fp);

	return true;
}

///////////////////////////////////////////

uint64_t file_time(const char *file) {
#if defined(_WIN32)
	HANDLE   handle = CreateFileA(file, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	FILETIME write_time;

	if (!GetFileTime(handle, nullptr, nullptr, &write_time))
		return 0;
	CloseHandle(handle);
	return (static_cast<uint64_t>(write_time.dwHighDateTime) << 32) | write_time.dwLowDateTime;
#elif defined(__linux__)
	struct stat result;
	if(stat(file, &result)==0)
		return result.st_mtime;
	return 0;
#endif
}

///////////////////////////////////////////

void file_name(const char *file, char *out_name, size_t name_size) {
	size_t      len   = strlen(file);
	const char *start = file + len;
	const char *end   = file + len;

	while (*start != '\\' && *start != '/' && start != file) start--;
	if    (*start == '\\' || *start == '/') start++;

	while (*end != '.' && end != start) end--;
	if    (end == start) end = file + len;

	for (int32_t i=0; start+i != end && i<name_size; i++) {
		out_name[i] = start[i];
	}
	size_t last = end - start;
	if (last > name_size) last = name_size;
	out_name[last] = '\0';
}

///////////////////////////////////////////

void file_name_ext(const char *file, char *out_name, size_t name_size) {
	size_t      len   = strlen(file);
	const char *start = file + len;
	const char *end   = file + len;

	while (*start != '\\' && *start != '/' && start != file) start--;
	if    (*start == '\\' || *start == '/') start++;

	for (int32_t i=0; start+i != end && i<name_size; i++) {
		out_name[i] = start[i];
	}
	size_t last = end - start;
	if (last > name_size) last = name_size;
	out_name[last] = '\0';
}

///////////////////////////////////////////

void file_dir(const char *file, char *out_path, size_t path_size) {
	size_t      len   = strlen(file);
	const char *end = file + len;

	while (*end != '\\' && *end != '/' && end != file) end--;

	for (int32_t i=0; file+i <= end && i<path_size; i++) {
		out_path[i] = file[i];
	}
	size_t last = (end - file)+1;
	if (last > path_size) last = path_size;
	out_path[last] = '\0';
}

///////////////////////////////////////////

bool file_exists(const char *path) {
	struct stat buffer;
#if _WIN32
	return (stat(path, &buffer) == 0);
#else
	return (stat(path, &buffer) == 0 && !(S_ISDIR(buffer.st_mode)));
#endif
}

///////////////////////////////////////////

bool path_is_file(const char *path) {
	size_t      len = strlen(path);
	const char *end = path + len;

	while (*end != '\\' && *end != '/' && end != path) {
		if (*end == '.') return true;
		end--;
	}
	return false;
}

///////////////////////////////////////////

bool path_is_wild(const char *path) {
	size_t      len = strlen(path);
	const char *end = path + len;

	while (*end != '\\' && *end != '/' && end != path) {
		if (*end == '*') return true;
		end--;
	}
	return false;
}

///////////////////////////////////////////

void iterate_dir(const char *directory_path, void *callback_data, void (*on_item)(void *callback_data, const char *name, bool file)) {
#if defined(_WIN32)
	if (strcmp(directory_path, "") == 0) {
		char drive_names[path_size];
		GetLogicalDriveStringsA(sizeof(drive_names), drive_names);
		char *curr = drive_names;
		while (*curr != '\0') {
			on_item(callback_data, curr, false);
			curr = curr + strlen(curr)+1;
		}
		return;
	}

	WIN32_FIND_DATAA info;
	HANDLE           handle = nullptr;

	char directory[path_size];
	file_dir(directory_path, directory, sizeof(directory));

	char   filter[path_size];
	size_t path_len = strlen(directory_path);
	if (path_is_wild(directory_path)) {
		snprintf(filter, sizeof(filter), "%s", directory_path);
	} else if (directory_path[path_len] == '\\' || directory_path[path_len] == '/') {
		snprintf(filter, sizeof(filter), "%s*.*", directory_path);
	} else {
		snprintf(filter, sizeof(filter), "%s/*.*", directory_path);
	}

	handle = FindFirstFileA(filter, &info);
	if (handle == INVALID_HANDLE_VALUE) return;

	while (handle) {
		if (strcmp(info.cFileName, ".") != 0 && strcmp(info.cFileName, "..") != 0) {
			char file[path_size];
			snprintf(file, sizeof(file), "%s%s", directory, info.cFileName);

			if (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
				on_item(callback_data, file, false);
			else
				on_item(callback_data, file, true);
		}

		if (!FindNextFileA(handle, &info)) {
			FindClose(handle);
			handle = nullptr;
		}
	}
#endif
}