#pragma once

///////////////////////////////////////////

const char *shader_hlsl = R"_(
cbuffer TransformBuffer : register(b0) {
	float4x4 world;
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
	psIn output;
	output.pos = mul(float4(input.pos.xyz, 1), world);
	output.pos = mul(output.pos, viewproj);
	float3 normal = normalize(mul(float4(input.norm, 0), world).xyz);
	output.color = saturate(dot(normal, float3(0,1,0))).xxx * input.color.rgb;
	output.uv = input.uv;
	return output;
}
float4 ps(psIn input) : SV_TARGET {
	return float4(input.color, 1) * tex.Sample(tex_sampler, input.uv);
}
)_";

///////////////////////////////////////////

const char *shader_glsl_vs = R"_(#version 450

layout(binding = 0, std140) uniform type_TransformBuffer
{
    layout(row_major) mat4 world;
    layout(row_major) mat4 viewproj;
} TransformBuffer;

layout(location = 0) in vec4 in_var_SV_POSITION;
layout(location = 1) in vec3 in_var_NORMAL;
layout(location = 2) in vec2 in_var_TEXCOORD0;
layout(location = 3) in vec4 in_var_COLOR0;
layout(location = 0) out vec2 out_var_TEXCOORD0;
layout(location = 1) out vec3 out_var_COLOR0;

void main()
{
    gl_Position = TransformBuffer.viewproj * (TransformBuffer.world * vec4(in_var_SV_POSITION.xyz, 1.0));
    out_var_TEXCOORD0 = in_var_TEXCOORD0;
    out_var_COLOR0 = vec3(clamp(normalize((TransformBuffer.world * vec4(in_var_NORMAL, 0.0)).xyz).y, 0.0, 1.0)) * in_var_COLOR0.xyz;
}
)_";

///////////////////////////////////////////

const char *shader_glsl_ps = R"_(#version 450
precision mediump float;
precision highp int;

uniform highp sampler2D SPIRV_Cross_Combinedtextex_sampler;

layout(location = 0) in highp vec2 in_var_TEXCOORD0;
layout(location = 1) in highp vec3 in_var_COLOR0;
layout(location = 0) out highp vec4 out_var_SV_TARGET;

void main()
{
    out_var_SV_TARGET = vec4(in_var_COLOR0, 1.0) * texture(SPIRV_Cross_Combinedtextex_sampler, in_var_TEXCOORD0);
}
)_";