// [name] sk/default

#include "stereokit.hlslinc"

cbuffer ParamBuffer : register(b3) {
	// [param] color color {1, 1, 1, 1}
	float4 _color;
	// [param] float tex_scale 1
	float tex_scale;
};
struct vsIn {
	float4 pos  : SV_POSITION;
	float3 norm : NORMAL;
	float4 col  : COLOR;
	float2 uv   : TEXCOORD0;
};
struct psIn {
	float4 pos   : SV_POSITION;
	float4 color : COLOR0;
	float2 uv    : TEXCOORD0;
	uint view_id : SV_RenderTargetArrayIndex;
};

// [texture] diffuse white
Texture2D    tex         : register(t0);
SamplerState tex_sampler : register(s0);

// [texture] diffuse white
Texture2D    test         : register(t3);
SamplerState test_sampler : register(s3);

psIn vs(vsIn input, uint id : SV_InstanceID) {
	psIn output;
	float4 world = mul(input.pos, sk_inst[id].world);
	output.pos   = mul(world,     sk_viewproj[sk_inst[id].view_id]);

	float3 normal = normalize(mul(input.norm, (float3x3)sk_inst[id].world));

	output.view_id = sk_inst[id].view_id;
	output.uv      = input.uv * tex_scale;
	output.color   = _color * input.col * sk_inst[id].color;
	output.color.rgb *= Lighting(normal);
	return output;
}
float4 ps(psIn input) : SV_TARGET {
	float4 col = tex.Sample(tex_sampler, input.uv) * test.Sample(test_sampler, input.uv);
	return col * input.color * _color;
}