#include "_sksc.h"

#define ENABLE_HLSL

#include <glslang/Public/ShaderLang.h>
#include "StandAlone/DirStackFileIncluder.h"
#include "SPIRV/GlslangToSpv.h"

#include <spirv-tools/optimizer.hpp>

///////////////////////////////////////////

void sksc_glslang_init() {
	glslang::InitializeProcess();
}

///////////////////////////////////////////

void sksc_glslang_shutdown() {
	glslang::FinalizeProcess();
}

///////////////////////////////////////////
// HLSL to SPIR-V                        //
///////////////////////////////////////////

class SkscIncluder : public DirStackFileIncluder {
public:
	virtual IncludeResult* includeSystem(const char* header_name, const char* includer_name, size_t inclusion_depth) override {
		return readLocalPath(header_name, includer_name, (int)inclusion_depth);
	}
};

///////////////////////////////////////////

bool parse_startswith(const char* a, const char* is) {
	while (*is != '\0') {
		if (*a == '\0' || *is != *a)
			return false;
		a++;
		is++;
	}
	return true;
}

///////////////////////////////////////////

bool parse_readint(const char* a, char separator, int32_t *out_int, const char** out_at) {
	const char *end = a;
	while (*end != '\0' && *end != '\n' && *end != separator) {
		end++;
	}
	if (*end != separator) return false;

	char* success = nullptr;
	*out_int = (int32_t)strtol(a, &success, 10);
	*out_at  = end+1;

	return success <= end;
}

///////////////////////////////////////////

const char* parse_glslang_error(const char* at) {
	const char* curr  = at;
	log_level_  level = log_level_err;
	if      (parse_startswith(at, "ERROR: "  )) { level = log_level_err;  curr += 7;}
	else if (parse_startswith(at, "WARNING: ")) { level = log_level_warn; curr += 9;}

	bool has_line = false;
	int32_t line;
	int32_t col;

	const char* numbers = curr;
	// Check for 'col:line:' format line numbers
	if (parse_readint(numbers, ':', &col,  &numbers) &&
		parse_readint(numbers, ':', &line, &numbers)) {
		has_line = true;
		curr = numbers + 1;
	}
	numbers = curr;
	// check for '(line)' format line numbers
	if (!has_line && *numbers == '(' && parse_readint(numbers+1, ')', &line, &numbers)) {
		has_line = true;
		curr = numbers + 1;
		if (*curr != '\0') curr++;
	}

	const char* start = curr;
	while (*curr != '\0' && *curr != '\n') {
		curr++;
	}
	if (curr - at > 1) 
		has_line 
			? sksc_log_at(level, line, col, "%.*s", curr - start, start)
			: sksc_log   (level,            "%.*s", curr - start, start);
	if (*curr == '\n') curr++;
	return *curr == '\0' ? nullptr : curr;
}

///////////////////////////////////////////

void log_shader_msgs(glslang::TShader *shader) {
	const char* info_log  = shader->getInfoLog();
	const char* debug_log = shader->getInfoDebugLog();
	while (info_log  != nullptr && *info_log  != '\0') { info_log  = parse_glslang_error(info_log ); }
	while (debug_log != nullptr && *debug_log != '\0') { debug_log = parse_glslang_error(debug_log); }
}

///////////////////////////////////////////

compile_result_ sksc_hlsl_to_spirv(const char *hlsl, const sksc_settings_t *settings, skg_stage_ type, const char** defines, int32_t define_count, skg_shader_file_stage_t *out_stage) {
	TBuiltInResource default_resource = {};
	EShMessages      messages         = EShMsgDefault;
	EShMessages      messages_link    = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules | EShMsgDebugInfo);
	EShLanguage      stage;
	const char*      entry = "na";
	switch(type) {
		case skg_stage_vertex:  stage = EShLangVertex;   entry = settings->vs_entrypoint; break;
		case skg_stage_pixel:   stage = EShLangFragment; entry = settings->ps_entrypoint; break;
		case skg_stage_compute: stage = EShLangCompute;  entry = settings->cs_entrypoint; break;
	}

	// Create the shader and set options
	glslang::TShader shader(stage);
	const char* shader_strings[1] = { hlsl };
	shader.setStrings         (shader_strings, 1);
	shader.setEntryPoint      (entry);
	shader.setSourceEntryPoint(entry);
	shader.setEnvInput        (glslang::EShSourceHlsl, stage, glslang::EShClientVulkan, 100);
	shader.setEnvClient       (glslang::EShClientVulkan,      glslang::EShTargetVulkan_1_0);
	shader.setEnvTarget       (glslang::EShTargetSpv,         glslang::EShTargetSpv_1_0);
	shader.setEnvTargetHlslFunctionality1();

	std::string preamble;
	if (define_count > 0) {
		for (int32_t i = 0; i < define_count; i++) {
			preamble += "#define " + std::string(defines[i]) + "\n";
		}
		shader.setPreamble(preamble.c_str());
	}

	// Setup includer
	SkscIncluder includer;
	includer.pushExternalLocalDirectory(settings->folder);
	for (int32_t i = 0; i < settings->include_folder_ct; i++) {
		includer.pushExternalLocalDirectory(settings->include_folders[i]);
	}

	std::string preprocessed_glsl;
	if (!shader.preprocess(
		&default_resource,
		100,                // default version
		ENoProfile,         // default profile
		false,              // don't force default version and profile
		false,              // not forward compatible
		messages,
		&preprocessed_glsl,
		includer)) {

		log_shader_msgs(&shader);
		return compile_result_fail;
	}

	// Set the preprocessed shader
	const char* preprocessed_strings[1] = { preprocessed_glsl.c_str() };
	shader.setStrings(preprocessed_strings, 1);

	// Parse the shader
	if (!shader.parse(&default_resource, 100, false, messages)) {
		log_shader_msgs(&shader);
		return compile_result_fail;
	}

	// Create and link program
	glslang::TProgram program;
	program.addShader(&shader);
	if (!program.link(messages_link)) {
		log_shader_msgs(&shader);
		return compile_result_fail;
	}

	// Check if we found an entry point
	const char *link_info = program.getInfoLog();
	if (link_info != nullptr) {
		if (strstr(link_info, "Entry point not found") != nullptr) {
			return compile_result_skip;
		}
	}

	// Generate SPIR-V
	glslang::TIntermediate* intermediate = program.getIntermediate(stage);
	if (!intermediate) {
		return compile_result_fail;
	}

	std::vector<unsigned int> spirv;
	spv::SpvBuildLogger logger;
	glslang::GlslangToSpv(*intermediate, spirv, &logger);

	// Log any SPIR-V generation messages
	std::string gen_messages = logger.getAllMessages();
	if (gen_messages.length() > 0) {
		sksc_log(log_level_info, gen_messages.c_str());
	}

	// Optimize the SPIRV we just generated
	spvtools::Optimizer optimizer(SPV_ENV_UNIVERSAL_1_0);
	optimizer.SetMessageConsumer([](spv_message_level_t, const char*, const spv_position_t&, const char* m) {
		printf("SPIRV optimization error: %s\n", m);
	});

	optimizer.RegisterPerformancePasses();
	std::vector<uint32_t> spirv_optimized;
	if (!optimizer.Run(spirv.data(), spirv.size(), &spirv_optimized)) {
		return compile_result_fail;
	}

	out_stage->language  = skg_shader_lang_spirv;
	out_stage->stage     = type;
	out_stage->code_size = (uint32_t)(spirv_optimized.size() * sizeof(unsigned int));
	out_stage->code      = malloc(out_stage->code_size);
	memcpy(out_stage->code, spirv_optimized.data(), out_stage->code_size);

	return compile_result_success;
}

///////////////////////////////////////////
// HLSL Bytecode                         //
///////////////////////////////////////////

#if defined(SKSC_D3D11)

#pragma comment(lib,"d3dcompiler.lib")

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <d3dcompiler.h>

DWORD sksc_d3d11_build_flags   (const sksc_settings_t *settings);
bool  sksc_d3d11_compile_shader(const char *filename, const char *hlsl_text, sksc_settings_t *settings, skg_stage_ type, skg_shader_file_stage_t *out_stage, skg_shader_meta_t *ref_meta);

///////////////////////////////////////////

DWORD sksc_d3d11_build_flags(const sksc_settings_t *settings) {
	DWORD result = D3DCOMPILE_ENABLE_STRICTNESS;

	if (settings->row_major) result |= D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;
	else                     result |= D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR;
	if (settings->debug) {
		result |= D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_DEBUG;
	} else {
		switch (settings->optimize) {
		case 0:  result |= D3DCOMPILE_OPTIMIZATION_LEVEL0; break;
		case 1:  result |= D3DCOMPILE_OPTIMIZATION_LEVEL1; break;
		case 2:  result |= D3DCOMPILE_OPTIMIZATION_LEVEL2; break;
		default: result |= D3DCOMPILE_OPTIMIZATION_LEVEL3; break;
		}
	}
	return result;
}

///////////////////////////////////////////

class SKSCInclude : public ID3DInclude
{
public:
	const sksc_settings_t *settings;

	SKSCInclude(const sksc_settings_t *settings) {
		this->settings = settings;
	}

	HRESULT Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID* out_text, UINT* out_size) override
	{
		char path_filename[1024];
		snprintf(path_filename, sizeof(path_filename), "%s\\%s", settings->folder, pFileName);
		FILE *fp = nullptr;
		fopen_s(&fp, path_filename, "rb");
		for (int32_t i = 0; fp == nullptr && i < settings->include_folder_ct; i++) {
			snprintf(path_filename, sizeof(path_filename), "%s\\%s", settings->include_folders[i], pFileName);
			fopen_s(&fp, path_filename, "rb");
		}
		if (fp == nullptr) {
			return E_FAIL;
		}

		fseek(fp, 0L, SEEK_END);
		*out_size = ftell(fp);
		rewind(fp);

		*out_text = (char*)malloc(*out_size+1);
		if (*out_text == nullptr) { *out_size = 0; fclose(fp); return false; }
		fread((void*)*out_text, 1, *out_size, fp);
		fclose(fp);

		((char*)*out_text)[*out_size] = 0;

		return S_OK;
	}

	HRESULT Close(LPCVOID data) override
	{
		free((void*)data);
		return S_OK;
	}
};

///////////////////////////////////////////

bool sksc_hlsl_to_bytecode(const char *filename, const char *hlsl_text, const sksc_settings_t *settings, skg_stage_ type, skg_shader_file_stage_t *out_stage) {
	DWORD flags = sksc_d3d11_build_flags(settings);

	const char *entrypoint = nullptr;
	char target[64];
	switch (type) {
	case skg_stage_vertex:  entrypoint = settings->vs_entrypoint; break;
	case skg_stage_pixel:   entrypoint = settings->ps_entrypoint; break;
	case skg_stage_compute: entrypoint = settings->cs_entrypoint; break;
	}
	switch (type) {
	case skg_stage_vertex:  snprintf(target, sizeof(target), "vs_%s", settings->shader_model); break;
	case skg_stage_pixel:   snprintf(target, sizeof(target), "ps_%s", settings->shader_model); break;
	case skg_stage_compute: snprintf(target, sizeof(target), "cs_%s", settings->shader_model); break;
	}

	SKSCInclude includer(settings);
	ID3DBlob *errors, *compiled = nullptr;
	if (FAILED(D3DCompile(hlsl_text, strlen(hlsl_text), filename, nullptr, &includer, entrypoint, target, flags, 0, &compiled, &errors))) {
		const char* curr  = (char*)errors->GetBufferPointer();
		sksc_log(log_level_err_pre, curr);
		if (errors) errors->Release();
		return false;
	}
	else if (errors) {
		const char* curr = (char*)errors->GetBufferPointer();
		sksc_log(log_level_err_pre, curr);
		if (errors) errors->Release();
	}

	out_stage->language  = skg_shader_lang_hlsl;
	out_stage->stage     = type;
	out_stage->code_size = (uint32_t)compiled->GetBufferSize();
	out_stage->code       = malloc(out_stage->code_size);
	memcpy(out_stage->code, compiled->GetBufferPointer(), out_stage->code_size);

	compiled->Release();
	return true;
}

#endif