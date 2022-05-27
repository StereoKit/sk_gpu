//--name                 = unlit/test
//--time: color          = 1
//--tex: 2D              = white
//--uv_scale: range(0,2) = 0.5
//--chunks = 1, 2, 2, 1

// This is for the system to load in global values
cbuffer SystemBuffer : register(b1) {
	float4x4 viewproj;
};

// And these are for instanced rendering
struct Inst {
	float4x4 world;
};
cbuffer TransformBuffer : register(b2) {
	Inst inst[100];
};

/* Ugh */

/*struct vsIn {
	float4 pos  : SV_POSITION;
	float3 norm : NORMAL;
	float2 uv   : TEXCOORD0;
	float4 color: COLOR0;
};*/

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

uint chunks[4];
float tex_scale;
float4 time;

Texture2D    tex         : register(t0);
SamplerState tex_sampler : register(s0);

psIn vs(vsIn input, uint id : SV_InstanceID) {
	psIn output;
	output.pos = mul(float4(input.pos.xyz, 1), inst[id].world);
	output.pos = mul(output.pos, viewproj);
	float3 normal = normalize(mul(float4(input.norm, 0), inst[id].world).xyz);
	output.color = saturate(dot(normal, float3(0,1,0))).xxx * input.color.rgb;
	output.uv = input.uv * tex_scale * time.x;
	return output;
}
float4 ps(psIn input) : SV_TARGET {
	return float4(input.color, 1) * tex.Sample(tex_sampler, input.uv);
}