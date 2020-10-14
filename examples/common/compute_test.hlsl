struct skr_vert_t {
	float3 pos;
	float3 norm;
	float2 uv;
	int    col;
};

RWStructuredBuffer<skr_vert_t> out_verts : register(u0);

[numthreads(1, 1, 1)]
void cs( uint3 thread : SV_DispatchThreadID )
{
	out_verts[thread.xy].pos = float3(thread.x, thread.y, 0);
}