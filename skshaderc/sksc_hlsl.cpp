#include "_sksc.h"

#include <glslang/Include/glslang_c_interface.h>
#include "glslang/Include/ShHandle.h"
#include "StandAlone/DirStackFileIncluder.h"

#include <spirv-tools/optimizer.hpp>

///////////////////////////////////////////
// HLSL to SPIR-V                        //
///////////////////////////////////////////

typedef struct glslang_shader_s {
	glslang::TShader* shader;
	std::string       preprocessed_glsl;
} glslang_shader_t;

///////////////////////////////////////////

class SkscIncluder : public DirStackFileIncluder {
public:
	virtual IncludeResult* includeSystem(const char* header_name, const char* includer_name, size_t inclusion_depth) override {
		return readLocalPath(header_name, includer_name, (int)inclusion_depth);
	}
};

///////////////////////////////////////////

int strcmp_nocase(char const *a, char const *b) {
	for (;; a++, b++) {
		int d = tolower((unsigned char)*a) - tolower((unsigned char)*b);
		if (d != 0 || !*a)
			return d;
	}
}

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
	if (parse_readint(numbers, ':', &col, &numbers) &&
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
			: sksc_log(level, "%.*s", curr - start, start);
	if (*curr == '\n') curr++;
	return *curr == '\0' ? nullptr : curr;
}

///////////////////////////////////////////

compile_result_ sksc_glslang_compile_shader(const char *hlsl, sksc_settings_t *settings, skg_stage_ type, skg_shader_lang_ lang, skg_shader_file_stage_t *out_stage, skg_shader_meta_t *out_meta) {
	glslang_resource_s default_resource = {};
	glslang_input_t    input            = {};
	input.language                = GLSLANG_SOURCE_HLSL;
	input.code                    = hlsl;
	input.client                  = GLSLANG_CLIENT_VULKAN;
	input.client_version          = GLSLANG_TARGET_VULKAN_1_0,
	input.target_language         = GLSLANG_TARGET_SPV;
	input.target_language_version = GLSLANG_TARGET_SPV_1_0;
	input.default_version         = 100;
	input.default_profile         = GLSLANG_NO_PROFILE;
	input.messages                = GLSLANG_MSG_DEFAULT_BIT;
	input.resource                = &default_resource;
	const char *entry = "na";
	switch(type) {
		case skg_stage_vertex:  input.stage = GLSLANG_STAGE_VERTEX;   entry = settings->vs_entrypoint; break;
		case skg_stage_pixel:   input.stage = GLSLANG_STAGE_FRAGMENT; entry = settings->ps_entrypoint; break;
		case skg_stage_compute: input.stage = GLSLANG_STAGE_COMPUTE;  entry = settings->cs_entrypoint; break;
	}

	glslang_shader_t *shader = glslang_shader_create(&input);
	shader->shader->setEntryPoint(entry);

	SkscIncluder includer;
	includer.pushExternalLocalDirectory(settings->folder);
	for (int32_t i = 0; i < settings->include_folder_ct; i++) {
		includer.pushExternalLocalDirectory(settings->include_folders[i]);
	}
	//if (!sksc_glslang_shader_preprocess(shader, &input, includer)) {
	if (!shader->shader->preprocess(
			reinterpret_cast<const TBuiltInResource*>(input.resource),
			input.default_version,
			(EProfile)input.default_profile,
			input.force_default_version_and_profile != 0,
			input.forward_compatible != 0,
			(EShMessages)input.messages,
			&shader->preprocessed_glsl,
			includer )) {
		const char* curr = glslang_shader_get_info_log(shader);
		while (curr != nullptr) curr = parse_glslang_error(curr);
		curr = glslang_shader_get_info_debug_log(shader);
		while (curr != nullptr) curr = parse_glslang_error(curr);
		glslang_shader_delete (shader);
		return compile_result_fail;
	}

	if (!glslang_shader_parse(shader, &input)) {
		const char* curr = glslang_shader_get_info_log(shader);
		while (curr != nullptr) curr = parse_glslang_error(curr);
		curr = glslang_shader_get_info_debug_log(shader);
		while (curr != nullptr) curr = parse_glslang_error(curr);
		glslang_shader_delete (shader);
		return compile_result_fail;
	}

	glslang_program_t* program = glslang_program_create();
	glslang_program_add_shader(program, shader);

	if (!glslang_program_link(program, GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT | GLSLANG_MSG_DEBUG_INFO_BIT)) {
		const char* curr = glslang_shader_get_info_log(shader);
		while (curr != nullptr) curr = parse_glslang_error(curr);
		curr = glslang_shader_get_info_debug_log(shader);
		while (curr != nullptr) curr = parse_glslang_error(curr);
		glslang_shader_delete (shader);
		glslang_program_delete(program);
		return compile_result_fail;
	}

	// Check if we found an entry point
	const char *link_info = glslang_program_get_info_log(program);
	if (link_info != nullptr) {
		if (strstr(link_info, "Entry point not found") != nullptr) {
			glslang_shader_delete (shader);
			glslang_program_delete(program);
			return compile_result_skip;
		}
	}
	
	glslang_program_SPIRV_generate(program, input.stage);

	if (glslang_program_SPIRV_get_messages(program)) {
		sksc_log(log_level_info, glslang_program_SPIRV_get_messages(program));
	}

	// Get the generated SPIRV code, and wrap up glslang's responsibilities
	size_t spirv_size = glslang_program_SPIRV_get_size(program) * sizeof(unsigned int);
	void  *spirv_code = malloc(spirv_size);
	glslang_program_SPIRV_get(program, (unsigned int*)spirv_code);
	glslang_shader_delete (shader);
	glslang_program_delete(program);

	// Optimize the SPIRV we just generated
	spvtools::Optimizer optimizer(SPV_ENV_UNIVERSAL_1_0);
	optimizer.SetMessageConsumer([](spv_message_level_t, const char*, const spv_position_t&, const char* m) {
		printf("SPIRV optimization error: %s\n", m);
	});

	optimizer.RegisterPerformancePasses();
	std::vector<uint32_t> spirv_optimized;
	if (!optimizer.Run((uint32_t*)spirv_code, spirv_size/sizeof(uint32_t), &spirv_optimized)) {
		free(spirv_code);
		return compile_result_fail;
	}

	out_stage->language  = lang;
	out_stage->stage     = type;
	out_stage->code_size = (uint32_t)(spirv_optimized.size() * sizeof(unsigned int));
	out_stage->code      = malloc(out_stage->code_size);
	memcpy(out_stage->code, spirv_optimized.data(), out_stage->code_size);
	
	free(spirv_code);

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
		FILE *fp = fopen(path_filename, "rb");
		for (int32_t i = 0; fp == nullptr && i < settings->include_folder_ct; i++) {
			snprintf(path_filename, sizeof(path_filename), "%s\\%s", settings->include_folders[i], pFileName);
			fp = fopen(path_filename, "rb");
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

bool sksc_d3d11_compile_shader(const char *filename, const char *hlsl_text, sksc_settings_t *settings, skg_stage_ type, skg_shader_file_stage_t *out_stage, skg_shader_meta_t *ref_meta) {
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

	// Get some info about the shader!
	ID3D11ShaderReflection* reflector = nullptr; 
	const GUID IID_ID3D11ShaderReflection = { 0x8d536ca1, 0x0cca, 0x4956, { 0xa8, 0x37, 0x78, 0x69, 0x63, 0x75, 0x55, 0x84 } };
	if (FAILED(D3DReflect( out_stage->code, out_stage->code_size, IID_ID3D11ShaderReflection, (void**)&reflector))) {
		sksc_log(log_level_err_pre, "Shader reflection failed!");
		return false;
	}

	D3D11_SHADER_DESC shader_desc = {};
	reflector->GetDesc(&shader_desc);

	// Snag some perf related to data
	skg_shader_ops_t *ops = nullptr;
	if (type == skg_stage_vertex) ops = &ref_meta->ops_vertex;
	if (type == skg_stage_pixel)  ops = &ref_meta->ops_pixel;
	if (ops) {
		ops->total        = shader_desc.InstructionCount;
		ops->tex_read     = shader_desc.TextureLoadInstructions + shader_desc.TextureNormalInstructions;
		ops->dynamic_flow = shader_desc.DynamicFlowControlCount;
	}

	// Get information about the vertex input data
	if (type == skg_stage_vertex) {
		ref_meta->vertex_input_count = shader_desc.InputParameters;
		ref_meta->vertex_inputs = (skg_vert_component_t*)malloc(sizeof(skg_vert_component_t) * ref_meta->vertex_input_count);

		int32_t curr = 0;
		for (int32_t i = 0; i < (int32_t)shader_desc.InputParameters; i++) {
			D3D11_SIGNATURE_PARAMETER_DESC param_desc = {};
			reflector->GetInputParameterDesc(i, &param_desc);

			// Ignore SV_ inputs, unless they're SV_Position
			if (strlen(param_desc.SemanticName)>3 &&
				tolower(param_desc.SemanticName[0]) == 's' &&
				tolower(param_desc.SemanticName[1]) == 'v' &&
				tolower(param_desc.SemanticName[2]) == '_' &&
				strcmp_nocase(param_desc.SemanticName, "sv_position") != 0)
			{
				ref_meta->vertex_input_count--;
				continue;
			}
			
			ref_meta->vertex_inputs[curr].count         = 0;
			ref_meta->vertex_inputs[curr].semantic_slot = param_desc.SemanticIndex;
			if      (strcmp_nocase(param_desc.SemanticName, "binormal")     == 0) ref_meta->vertex_inputs[curr].semantic = skg_semantic_binormal;
			else if (strcmp_nocase(param_desc.SemanticName, "blendindices") == 0) ref_meta->vertex_inputs[curr].semantic = skg_semantic_blendindices;
			else if (strcmp_nocase(param_desc.SemanticName, "blendweight")  == 0) ref_meta->vertex_inputs[curr].semantic = skg_semantic_blendweight;
			else if (strcmp_nocase(param_desc.SemanticName, "color")        == 0) ref_meta->vertex_inputs[curr].semantic = skg_semantic_color;
			else if (strcmp_nocase(param_desc.SemanticName, "normal")       == 0) ref_meta->vertex_inputs[curr].semantic = skg_semantic_normal;
			else if (strcmp_nocase(param_desc.SemanticName, "sv_position")  == 0) ref_meta->vertex_inputs[curr].semantic = skg_semantic_position;
			else if (strcmp_nocase(param_desc.SemanticName, "position")     == 0) ref_meta->vertex_inputs[curr].semantic = skg_semantic_position;
			else if (strcmp_nocase(param_desc.SemanticName, "psize")        == 0) ref_meta->vertex_inputs[curr].semantic = skg_semantic_psize;
			else if (strcmp_nocase(param_desc.SemanticName, "tangent")      == 0) ref_meta->vertex_inputs[curr].semantic = skg_semantic_tangent;
			else if (strcmp_nocase(param_desc.SemanticName, "texcoord")     == 0) ref_meta->vertex_inputs[curr].semantic = skg_semantic_texcoord;
			switch(param_desc.ComponentType) {
				case D3D_REGISTER_COMPONENT_FLOAT32: ref_meta->vertex_inputs[curr].format = skg_fmt_f32;  break;
				case D3D_REGISTER_COMPONENT_SINT32:  ref_meta->vertex_inputs[curr].format = skg_fmt_i32;  break;
				case D3D_REGISTER_COMPONENT_UINT32:  ref_meta->vertex_inputs[curr].format = skg_fmt_ui32; break;
				default: ref_meta->vertex_inputs[curr].format = skg_fmt_none; break;
			}
			curr += 1;
		}
	}
	
	/*char text[256];
	for (uint32_t i = 0; i < shader_desc.ConstantBuffers; i++) {
		ID3D11ShaderReflectionConstantBuffer *buff = reflector->GetConstantBufferByIndex(i);
		D3D11_SHADER_BUFFER_DESC buff_desc = {};
		buff->GetDesc(&buff_desc);

		snprintf(text, sizeof(text), "Buffer: %s", buff_desc.Name);
		sksc_log(log_level_info, text);
		for (uint32_t v = 0; v < buff_desc.Variables; v++) {
			ID3D11ShaderReflectionVariable *var = buff->GetVariableByIndex(v);
			D3D11_SHADER_VARIABLE_DESC var_desc = {};
			var->GetDesc(&var_desc);

			snprintf(text, sizeof(text), "  %s", var_desc.Name);
			sksc_log(log_level_info, text);
		}
	}*/
	
	reflector->Release();

	compiled->Release();
	return true;
}

#endif