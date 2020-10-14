// https://simoncoenen.com/blog/programming/graphics/DxcRevised.html

#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define SKR_DIRECT3D11
#define SKR_IMPL
#include "../sk_gpu.h"

#define SKSC_IMPL
#include "sksc.h"

///////////////////////////////////////////

void            get_folder    (char *filename, char *out_dest,  size_t dest_size);
bool            read_file     (char *filename, char **out_text, size_t *out_size);
void            iterate_files (char *input_name, sksc_settings_t *settings);
sksc_settings_t check_settings(int32_t argc, char **argv);

///////////////////////////////////////////

int main(int argc, char **argv) {
	sksc_settings_t settings = check_settings(argc, argv);
	sksc_init();

	iterate_files(argv[argc - 1], &settings);

	sksc_shutdown();
}

///////////////////////////////////////////

sksc_settings_t check_settings(int32_t argc, char **argv) {
	sksc_settings_t result = {};
	result.debug         = false;
	result.optimize      = 3;
	result.replace_ext   = false;
	result.output_header = false;
	result.row_major     = false;

	// Get the inlcude folder
	get_folder(argv[argc-1], result.folder, sizeof(result.folder));

	for (int32_t i=1; i<argc; i++) {
		if      (strcmp(argv[i], "-h") == 0) result.output_header = true;
		else if (strcmp(argv[i], "-e") == 0) result.replace_ext   = true;
		else if (strcmp(argv[i], "-r") == 0) result.row_major     = true;
		else if (strcmp(argv[i], "-cs") == 0 && i<argc-1) { strncpy(result.cs_entrypoint, argv[i+1], sizeof(result.cs_entrypoint)); i++; }
		else if (strcmp(argv[i], "-vs") == 0 && i<argc-1) { strncpy(result.vs_entrypoint, argv[i+1], sizeof(result.vs_entrypoint)); i++; }
		else if (strcmp(argv[i], "-ps") == 0 && i<argc-1) { strncpy(result.ps_entrypoint, argv[i+1], sizeof(result.ps_entrypoint)); i++; }
		else if (strcmp(argv[i], "-m") == 0 && i<argc-1)  { strncpy(result.shader_model,  argv[i+1], sizeof(result.shader_model )); i++; }
	}

	if (result.shader_model[0] == 0)
		strncpy(result.shader_model, "5_0", sizeof(result.shader_model));

	// If no entrypoints were provided, then these are the defaults!
	if (result.ps_entrypoint[0] == 0 && result.vs_entrypoint[0] == 0 && result.cs_entrypoint[0] == 0) {
		strncpy(result.ps_entrypoint, "ps", sizeof(result.ps_entrypoint));
		strncpy(result.vs_entrypoint, "vs", sizeof(result.vs_entrypoint));
	}

	return result;
}

///////////////////////////////////////////

void iterate_files(char *input_name, sksc_settings_t *settings) {
	HANDLE           handle;
	WIN32_FIND_DATAA file_info;

	char folder[512] = {};
	get_folder(input_name, folder, sizeof(folder));

	if((handle = FindFirstFileA(input_name, &file_info)) != INVALID_HANDLE_VALUE) {
		do {
			char filename[1024];
			sprintf_s(filename, "%s%s", folder, file_info.cFileName);

			char  *file_text;
			size_t file_size;
			if (read_file(filename, &file_text, &file_size)) {
				skr_shader_file_t file;
				if (sksc_compile(filename, file_text, settings, &file)) {
					char new_filename[512];
					char drive[16];
					char dir  [512];
					char name [128];
					_splitpath_s(filename,
						drive, sizeof(drive),
						dir,   sizeof(dir),
						name,  sizeof(name), nullptr, 0); 

					if (settings->replace_ext) {
						sprintf_s(new_filename, "%s%s%s.sks", drive, dir, name);
					} else {
						sprintf_s(new_filename, "%s.sks", filename);
					}
					sksc_save(new_filename, &file);
					if (settings->output_header)
						sksc_save_header(new_filename);

					skr_shader_file_destroy(&file);
				}
				free(file_text);
			}
		} while(FindNextFileA(handle, &file_info));
		FindClose(handle);
	}
}

///////////////////////////////////////////

bool read_file(char *filename, char **out_text, size_t *out_size) {
	*out_text = nullptr;
	*out_size = 0;

	FILE *fp;
	if (fopen_s(&fp, filename, "rb") != 0 || fp == nullptr) {
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

void get_folder(char *filename, char *out_dest, size_t dest_size) {
	char drive[16];
	char dir  [512];
	_splitpath_s(filename,
		drive, sizeof(drive),
		dir,   sizeof(dir),
		nullptr, 0, nullptr, 0); 

	sprintf_s(out_dest, dest_size, "%s%s", drive, dir);
}