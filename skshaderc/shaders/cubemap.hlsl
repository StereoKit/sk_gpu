cbuffer SystemBuffer : register(b0) {
	float4x4 viewproj;
};

struct Inst {
	float4x4 world;
};
cbuffer TransformBuffer : register(b1) {
	Inst inst[100];
};

struct vsIn {
	float4 pos  : SV_POSITION;
	float3 norm : NORMAL;
	float2 uv   : TEXCOORD0;
	float4 color: COLOR0;
};
struct psIn {
	float4 pos   : SV_POSITION;
	float3 norm  : NORMAL;
	float2 uv    : TEXCOORD0;
	float3 color : COLOR0;
};

Texture2D    tex         : register(t0);
SamplerState tex_sampler : register(s0);

TextureCube  cubemap      : register(t8);
SamplerState cube_sampler : register(s8);

psIn vs(vsIn input, uint id : SV_InstanceID) {
	psIn output;
	output.pos = mul(float4(input.pos.xyz, 1), inst[id].world);
	output.pos = mul(output.pos, viewproj);
	output.norm = normalize(mul(float4(input.norm, 0), inst[id].world).xyz);
	output.color = input.color.rgb;
	output.uv = input.uv;
	return output;
}
float4 ps(psIn input) : SV_TARGET {
	return float4(input.color, 1) * tex.Sample(tex_sampler, input.uv) * cubemap.SampleLevel(cube_sampler, input.norm, 0);
}