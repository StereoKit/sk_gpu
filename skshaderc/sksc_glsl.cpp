#define _CRT_SECURE_NO_WARNINGS

#include "_sksc.h"

#include <spirv_glsl.hpp>

///////////////////////////////////////////

bool sksc_check_tags(const char *tag_list, const char *tag);

///////////////////////////////////////////

bool sksc_spirv_to_glsl(const skg_shader_file_stage_t *src_stage, const sksc_settings_t *settings, skg_shader_lang_ lang, skg_shader_file_stage_t *out_stage, const skg_shader_meta_t *meta, array_t<sksc_meta_item_t> var_meta) {
	try {
		// Create compiler instance
		spirv_cross::CompilerGLSL glsl((uint32_t*)src_stage->code, src_stage->code_size/ sizeof(uint32_t));

		// Set up compiler options
		spirv_cross::CompilerGLSL::Options options;
		if (lang == skg_shader_lang_glsl_web) {
			options.version = 300;
			options.es      = true;
			options.vertex.support_nonzero_base_instance = false;
		} else if (lang == skg_shader_lang_glsl_es) {
			options.version = 320;
			options.es      = true;
			options.vertex.support_nonzero_base_instance = false;
		} else if (lang == skg_shader_lang_glsl) {
			options.version = settings->gl_version;
			options.es      = false;
		}
		//if (src_stage->stage == skg_stage_vertex)
		//	options.ovr_multiview_view_count = 2;
		glsl.set_common_options(options);

		// Reflect shader resources
		spirv_cross::ShaderResources resources = glsl.get_shader_resources();

		// Ensure buffer ids stay the same
		for (spirv_cross::Resource &resource : resources.uniform_buffers) {
			const std::string &name = glsl.get_name(resource.id);
			for (size_t b = 0; b < meta->buffer_count; b++) {
				if (strcmp(name.c_str(), meta->buffers[b].name) == 0 || (strcmp(name.c_str(), "_Global") == 0 && strcmp(meta->buffers[b].name, "$Global") == 0)) {
					glsl.set_decoration(resource.id, spv::DecorationBinding, meta->buffers[b].bind.slot);
					break;
				}
			}
		}

		// Convert tagged textures to use OES samplers
		glsl.set_variable_type_remap_callback([&](const spirv_cross::SPIRType& type, const std::string& var_name, std::string& name_of_type) {
			for (size_t i = 0; i < var_meta.count; i++) {
				if (strcmp(var_meta[i].name, var_name.c_str()) != 0) continue;
				if (sksc_check_tags(var_meta[i].tag, "external")) {
					name_of_type = "samplerExternalOES";
				}
				break;
			}
		});
		
		// Check if we had an external texture. We need to know this now, but
		// the callback above doesn't happen until later.
		bool use_external = false;
		for (spirv_cross::Resource &image : resources.separate_images) {
			const std::string &name = glsl.get_name(image.id);

			for (size_t i = 0; i < var_meta.count; i++) {
				if (strcmp(var_meta[i].name, name.c_str()) != 0) continue;
				if (sksc_check_tags(var_meta[i].tag, "external")) {
					use_external = true;
				}
				break;
			}
		}

		glsl.add_header_line("#extension GL_EXT_gpu_shader5 : enable");
		//glsl.add_header_line("#extension GL_OES_sample_variables : enable");
		if (use_external == true && lang == skg_shader_lang_glsl_es) {
			glsl.add_header_line("#extension GL_OES_EGL_image_external_essl3 : enable");
		}
		// Add custom header lines for vertex shaders
		if (src_stage->stage == skg_stage_vertex) {
			//glsl.add_header_line("#extension GL_OVR_multiview2 : require");
			//glsl.add_header_line("layout(num_views = 2) in;");
			glsl.add_header_line("#define gl_Layer int _dummy_gl_layer_var");
		}

		// Build dummy sampler for combined images
		spirv_cross::VariableID dummy_sampler_id = glsl.build_dummy_sampler_for_combined_images();
		if (dummy_sampler_id) {
			glsl.set_decoration(dummy_sampler_id, spv::DecorationDescriptorSet, 0);
			glsl.set_decoration(dummy_sampler_id, spv::DecorationBinding,       0);
		}

		// Combine samplers and textures
		glsl.build_combined_image_samplers();

		// Make sure sampler names stay the same in GLSL
		for (const spirv_cross::CombinedImageSampler &remap : glsl.get_combined_image_samplers()) {
			const std::string &name    = glsl.get_name      (remap.image_id);
			uint32_t           binding = glsl.get_decoration(remap.image_id, spv::DecorationBinding);
			glsl.set_name      (remap.combined_id, name);
			glsl.set_decoration(remap.combined_id, spv::DecorationBinding, binding);
		}

		// Rename stage inputs/outputs for vertex/pixel shaders
		if (src_stage->stage == skg_stage_vertex || src_stage->stage == skg_stage_pixel) {
			size_t      off = src_stage->stage == skg_stage_vertex ? sizeof("@entryPointOutput.")-1 : sizeof("input.")-1;
			spirv_cross::SmallVector<spirv_cross::Resource> &stage_resources = src_stage->stage == skg_stage_vertex ? resources.stage_outputs : resources.stage_inputs;
			for (spirv_cross::Resource &resource : stage_resources) {
				char fs_name[64];
				snprintf(fs_name, sizeof(fs_name), "fs_%s", glsl.get_name(resource.id).c_str()+off);
				glsl.set_name(resource.id, fs_name);
			}
		}

		// Compile to GLSL
		std::string source = glsl.compile();

		// Set output stage details
		out_stage->stage     = src_stage->stage;
		out_stage->language  = lang;
		out_stage->code_size = static_cast<uint32_t>(source.size()) + 1;
		out_stage->code      = malloc(out_stage->code_size);
		strncpy(static_cast<char*>(out_stage->code), source.c_str(), out_stage->code_size);

		return true;
	} catch (const spirv_cross::CompilerError &e) {
		sksc_log(log_level_err, "[SPIRV-Cross] %s", e.what());
		return false;
	}
}

///////////////////////////////////////////

bool sksc_check_tags(const char *tag_list, const char *tag) {
	const char *start = tag_list;
	const char *end   = tag_list;
	while (*end != '\0') {
		if (*end == ',' || *end == ' ') {
			size_t length = end - start;
			if (length > 0 && strncmp(start, tag, length) == 0) {
				return true;
			}
			start = end + 1;
		}
		end++;
	}
	size_t length = end - start;
	if (length > 0 && strncmp(start, tag, length) == 0) {
		return true;
	}
	return false;
}