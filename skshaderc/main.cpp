// https://simoncoenen.com/blog/programming/graphics/DxcRevised.html

#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define SKG_IMPL
#include "../sk_gpu.h"

#define SKSC_IMPL
#include "sksc.h"

///////////////////////////////////////////

void            get_folder    (char *filename, char *out_dest,  size_t dest_size);
bool            read_file     (char *filename, char **out_text, size_t *out_size);
void            write_file    (char *filename, void *file_data, size_t file_size);
void            write_header  (char *filename, void *file_data, size_t file_size);
void            iterate_files (char *input_name, sksc_settings_t *settings);
sksc_settings_t check_settings(int32_t argc, char **argv, bool *exit); 
void            show_usage    ();
uint64_t        file_time     (char *file);

///////////////////////////////////////////

int main(int argc, char **argv) {
	bool exit = false;
	sksc_settings_t settings = check_settings(argc, argv, &exit);
	if (exit) return 0;

	sksc_init();

	iterate_files(argv[argc - 1], &settings);

	sksc_shutdown();

	return 0;
}

///////////////////////////////////////////

sksc_settings_t check_settings(int32_t argc, char **argv, bool *exit) {
	if (argc <= 1) {
		*exit = true;
		show_usage();
		return {};
	}

	sksc_settings_t result = {};
	result.debug         = false;
	result.optimize      = 3;
	result.replace_ext   = false;
	result.output_header = false;
	result.row_major     = false;
	result.silent_err    = false;
	result.silent_info   = false;
	result.silent_warn   = false;

	// Get the inlcude folder
	get_folder(argv[argc-1], result.folder, sizeof(result.folder));

	for (int32_t i=1; i<argc-1; i++) {
		if      (strcmp(argv[i], "-h" ) == 0) result.output_header = true;
		else if (strcmp(argv[i], "-e" ) == 0) result.replace_ext   = false;
		else if (strcmp(argv[i], "-r" ) == 0) result.row_major     = true;
		else if (strcmp(argv[i], "-d" ) == 0) result.debug         = true;
		else if (strcmp(argv[i], "-c" ) == 0) result.only_if_changed=true;
		else if (strcmp(argv[i], "-si") == 0) result.silent_info   = true;
		else if (strcmp(argv[i], "-sw") == 0) { result.silent_info = true; result.silent_warn = true; }
		else if (strcmp(argv[i], "-s" ) == 0) { result.silent_err = true; result.silent_info = true; result.silent_warn = true;}
		else if (strcmp(argv[i], "-o0") == 0 ||
				 strcmp(argv[i], "-O0") == 0) result.optimize = 0;
		else if (strcmp(argv[i], "-o1") == 0 ||
				 strcmp(argv[i], "-O1") == 0) result.optimize = 1;
		else if (strcmp(argv[i], "-o2") == 0 ||
				 strcmp(argv[i], "-O2") == 0) result.optimize = 2;
		else if (strcmp(argv[i], "-o3") == 0 ||
				 strcmp(argv[i], "-O3") == 0) result.optimize = 3;
		else if (strcmp(argv[i], "-help" ) == 0 ||
				 strcmp(argv[i], "/?" ) == 0 ) *exit = true;
		else if (strcmp(argv[i], "-cs") == 0 && i<argc-1) { strncpy(result.cs_entrypoint, argv[i+1], sizeof(result.cs_entrypoint)); i++; }
		else if (strcmp(argv[i], "-vs") == 0 && i<argc-1) { strncpy(result.vs_entrypoint, argv[i+1], sizeof(result.vs_entrypoint)); i++; }
		else if (strcmp(argv[i], "-ps") == 0 && i<argc-1) { strncpy(result.ps_entrypoint, argv[i+1], sizeof(result.ps_entrypoint)); i++; }
		else if (strcmp(argv[i], "-gl") == 0 && i<argc-1) { result.gl_version = atoi(argv[i+1]); i++; }
		else if (strcmp(argv[i], "-m" ) == 0 && i<argc-1) { strncpy(result.shader_model,  argv[i+1], sizeof(result.shader_model )); i++; }
		else if (strcmp(argv[i], "-i" ) == 0 && i<argc-1) {
			size_t len = strlen(argv[i + 1]) + 1;
			result.include_folder_ct += 1;
			result.include_folders    = (char**)realloc(result.include_folders, sizeof(result.include_folder_ct * sizeof(char *)));
			result.include_folders[result.include_folder_ct-1] = (char*)malloc(len);
			strncpy(result.include_folders[result.include_folder_ct-1], argv[i+1], len); 
			i++; }
		else { printf("Unrecognized option '%s'\n", argv[i]); *exit = true; }
	}

	if (result.shader_model[0] == 0)
		strncpy(result.shader_model, "5_0", sizeof(result.shader_model));

	if (result.gl_version == 0)
		result.gl_version = 430;

	// If no entrypoints were provided, then these are the defaults!
	if (result.ps_entrypoint[0] == 0 && result.vs_entrypoint[0] == 0 && result.cs_entrypoint[0] == 0) {
		strncpy(result.ps_entrypoint, "ps", sizeof(result.ps_entrypoint));
		strncpy(result.vs_entrypoint, "vs", sizeof(result.vs_entrypoint));
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
	-e		Appends the sks extension to the resulting file instead of 
			replacing the extension with sks. Default will replace the 
			extension.
	-s		Silent, no errors, warnings or info are printed when compiling 
			shaders.
	-sw		No info or warnings are printed when compiling shaders.
	-si		No info is printed when compiling shaders.
	-c		Only compile if the source code is newer than the output file.
	
	-d		Compile shaders with debug info embedded. Enabling this will
			disable shader optimizations.
	-o0		Optimization level 0. Default is 3.
	-o1		Optimization level 1. Default is 3.
	-o2		Optimization level 2. Default is 3.
	-o3		Optimization level 3. Default is 3.

	-cs name	Compiles a compute shader stage from this file, using an entry
			function of [name]. Specifying this removes the default entry 
			names from vertex and pixel shader stages.
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
	-gl version	Sets the target GLSL version for generated desktop OpenGL 
			shaders. By default this is '430'.

	target_file	This can be any filename, and can use the wildcard '*' to 
			compile multiple files in the same call.
)_");
}

///////////////////////////////////////////

void iterate_files(char *input_name, sksc_settings_t *settings) {
	HANDLE           handle;
	WIN32_FIND_DATAA file_info;

	int count = 0;
	char folder[512] = {};
	get_folder(input_name, folder, sizeof(folder));

	if((handle = FindFirstFileA(input_name, &file_info)) != INVALID_HANDLE_VALUE) {
		do {
			char filename[1024];
			sprintf_s(filename, "%s%s", folder, file_info.cFileName);

			char new_filename[512];
			char drive[16];
			char dir  [512];
			char name [128];
			_splitpath_s(filename,
				drive, sizeof(drive),
				dir,   sizeof(dir),
				name,  sizeof(name), nullptr, 0); 

			if (settings->replace_ext) {
				sprintf_s(new_filename, "%s%s%s.%s", drive, dir, name, settings->output_header?"h":"sks");
			} else {
				sprintf_s(new_filename, "%s.%s", filename, settings->output_header?"h":"sks");
			}

			// Skip this file if it hasn't changed
			if (settings->only_if_changed && file_time(filename) < file_time(new_filename)) {
				if (!settings->silent_info) {
					printf("File '%s' is already up-to-date, skipping...\n", filename);
				}
				continue;
			}

			char  *file_text;
			size_t file_size;
			if (read_file(filename, &file_text, &file_size)) {
				skg_shader_file_t file;
				if (sksc_compile(filename, file_text, settings, &file)) {
					

					void  *sks_data;
					size_t sks_size;
					sksc_build_file(&file, &sks_data, &sks_size);
					if (settings->output_header)
						write_header(new_filename, sks_data, sks_size);
					else 
						write_file  (new_filename, sks_data, sks_size);
					free(sks_data);

					skg_shader_file_destroy(&file);
				}
				sksc_log_print(settings);
				sksc_log_clear();
				free(file_text);
			} else {
				printf("Couldn't read file '%s'!\n", filename);
			}
		} while(FindNextFileA(handle, &file_info));
		FindClose(handle);
	} else {
		printf("Couldn't find or read file from '%s'!\n", input_name);
	}
}

///////////////////////////////////////////

bool read_file(char *filename, char **out_text, size_t *out_size) {
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
	fread (*out_text, 1, *out_size, fp);
	fclose(fp);

	(*out_text)[*out_size] = 0;
	return true;
}

///////////////////////////////////////////

void write_file(char *filename, void *file_data, size_t file_size) {
	FILE *fp = fopen(filename, "wb");
	if (fp == nullptr) {
		return;
	}
	fwrite(file_data, file_size, 1, fp);
	fflush(fp);
	fclose(fp);
}

///////////////////////////////////////////

void write_header(char *filename, void *file_data, size_t file_size) {
	char drive[16];
	char dir  [512];
	char name [128];
	_splitpath_s(filename,
		drive, sizeof(drive),
		dir,   sizeof(dir),
		name,  sizeof(name), nullptr, 0);

	// '.' may be common, and will bork the variable name
	size_t len = strlen(name);
	for (size_t i = 0; i < len; i++) {
		if (name[i] == '.') name[i] = '_';
	}

	FILE *fp = fopen(filename, "w");
	if (fp == nullptr) {
		return;
	}
	fprintf(fp, "#pragma once\n\n");
	int32_t ct = fprintf_s(fp, "const unsigned char sks_%s[%zu] = {", name, file_size);
	for (size_t i = 0; i < file_size; i++) {
		unsigned char byte = ((unsigned char *)file_data)[i];
		ct += fprintf_s(fp, "%d,", byte);
		if (ct > 80) { 
			fprintf(fp, "\n"); 
			ct = 0; 
		}
	}
	fprintf_s(fp, "};\n");
	fflush(fp);
	fclose(fp);
}

///////////////////////////////////////////

void get_folder(char *filename, char *out_dest, size_t dest_size) {
	char drive[16];
	char dir  [512];
	_splitpath_s(filename,
		drive, sizeof(drive),
		dir,   sizeof(dir),
		nullptr, 0, nullptr, 0); 

	sprintf_s(out_dest, dest_size, "%s%s", drive, dir);
}

///////////////////////////////////////////

uint64_t file_time(char *file) {
	HANDLE   handle = CreateFileA(file, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
	FILETIME write_time;

	if (!GetFileTime(handle, nullptr, nullptr, &write_time))
		return 0;
	CloseHandle(handle);
	return (static_cast<uint64_t>(write_time.dwHighDateTime) << 32) | write_time.dwLowDateTime;
}