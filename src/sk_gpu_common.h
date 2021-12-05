#pragma once
///////////////////////////////////////////
// API independant functions             //
///////////////////////////////////////////

typedef enum {
	skg_shader_lang_hlsl,
	skg_shader_lang_spirv,
	skg_shader_lang_glsl,
	skg_shader_lang_glsl_es,
	skg_shader_lang_glsl_web,
} skg_shader_lang_;

typedef struct {
	skg_shader_lang_ language;
	skg_stage_       stage;
	uint32_t         code_size;
	void            *code;
} skg_shader_file_stage_t;

typedef struct {
	skg_shader_meta_t       *meta;
	uint32_t                 stage_count;
	skg_shader_file_stage_t *stages;
} skg_shader_file_t;

///////////////////////////////////////////

SKG_API void                    skg_log                        (skg_log_ level, const char *text);
SKG_API bool                    skg_read_file                  (const char *filename, void **out_data, size_t *out_size);
SKG_API uint64_t                skg_hash                       (const char *string);
SKG_API uint32_t                skg_mip_count                  (int32_t width, int32_t height);
SKG_API void                    skg_mip_dimensions             (int32_t width, int32_t height, int32_t mip_level, int32_t *out_width, int32_t *out_height);

SKG_API skg_color32_t           skg_col_hsv32                  (float hue, float saturation, float value, float alpha);
SKG_API skg_color128_t          skg_col_hsv128                 (float hue, float saturation, float value, float alpha);
SKG_API skg_color32_t           skg_col_hsl32                  (float hue, float saturation, float lightness, float alpha);
SKG_API skg_color128_t          skg_col_hsl128                 (float hue, float saturation, float lightness, float alpha);
SKG_API skg_color32_t           skg_col_hcy32                  (float hue, float chroma, float lightness, float alpha);
SKG_API skg_color128_t          skg_col_hcy128                 (float hue, float chroma, float lightness, float alpha);
SKG_API skg_color32_t           skg_col_lch32                  (float hue, float chroma, float lightness, float alpha);
SKG_API skg_color128_t          skg_col_lch128                 (float hue, float chroma, float lightness, float alpha);
SKG_API skg_color32_t           skg_col_helix32                (float hue, float saturation, float lightness, float alpha);
SKG_API skg_color128_t          skg_col_helix128               (float hue, float saturation, float lightness, float alpha);
SKG_API skg_color32_t           skg_col_jab32                  (float j, float a, float b, float alpha);
SKG_API skg_color128_t          skg_col_jab128                 (float j, float a, float b, float alpha);
SKG_API skg_color32_t           skg_col_jsl32                  (float hue, float saturation, float lightness, float alpha);
SKG_API skg_color128_t          skg_col_jsl128                 (float hue, float saturation, float lightness, float alpha);
SKG_API skg_color32_t           skg_col_lab32                  (float l, float a, float b, float alpha);
SKG_API skg_color128_t          skg_col_lab128                 (float l, float a, float b, float alpha);
SKG_API skg_color128_t          skg_col_rgb_to_lab128          (skg_color128_t rgb);
SKG_API skg_color128_t          skg_col_to_srgb                (skg_color128_t rgb_linear);
SKG_API skg_color128_t          skg_col_to_linear              (skg_color128_t srgb);

SKG_API bool                    skg_shader_file_verify         (const void *file_memory, size_t file_size, uint16_t *out_version, char *out_name, size_t out_name_size);
SKG_API bool                    skg_shader_file_load_memory    (const void *file_memory, size_t file_size, skg_shader_file_t *out_file);
SKG_API bool                    skg_shader_file_load           (const char *file, skg_shader_file_t *out_file);
SKG_API skg_shader_stage_t      skg_shader_file_create_stage   (const skg_shader_file_t *file, skg_stage_ stage);
SKG_API void                    skg_shader_file_destroy        (      skg_shader_file_t *file);

SKG_API skg_bind_t              skg_shader_meta_get_bind       (const skg_shader_meta_t *meta, const char *name);
SKG_API int32_t                 skg_shader_meta_get_var_count  (const skg_shader_meta_t *meta);
SKG_API int32_t                 skg_shader_meta_get_var_index  (const skg_shader_meta_t *meta, const char *name);
SKG_API int32_t                 skg_shader_meta_get_var_index_h(const skg_shader_meta_t *meta, uint64_t name_hash);
SKG_API const skg_shader_var_t *skg_shader_meta_get_var_info   (const skg_shader_meta_t *meta, int32_t var_index);
SKG_API void                    skg_shader_meta_reference      (skg_shader_meta_t *meta);
SKG_API void                    skg_shader_meta_release        (skg_shader_meta_t *meta);