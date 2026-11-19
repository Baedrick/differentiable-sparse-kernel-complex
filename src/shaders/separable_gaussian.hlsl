// Copyright (C) 2026 Koh Swee Teck Dedrick.
// Licensed under the MIT license (https://opensource.org/license/mit/).

struct VertexOutput {
	float4 position : SV_POSITION;
	float2 uv       : TEXCOORD0;
};

cbuffer Globals : register(b0) {
	float2 u_uv_step;
	int u_kernel_size;
	int _pad;
};

SamplerState u_sampler_linear_clamp : register(s2);
Texture2D u_texture : register(t0);
Buffer<float> u_kernel : register(t1);

VertexOutput vs_main(uint vertex_id : SV_VertexID) {
	const float3 position[3] = {
		float3(-1.0, -1.0, 0.0),
		float3(-1.0,  3.0, 0.0),
		float3( 3.0, -1.0, 0.0)
	};
	const float2 uv[3] = {
		float2(0.0, 1.0),
		float2(0.0,-1.0),
		float2(2.0, 1.0)
	};
	VertexOutput output;
	output.position = float4(position[vertex_id], 1.0);
	output.uv = uv[vertex_id];
	return output;
}

float4 ps_main(VertexOutput input) : SV_TARGET {
	float3 color = u_texture.Sample(u_sampler_linear_clamp, input.uv).rgb * u_kernel[0];
	for (int k = 1; k < u_kernel_size; ++k) {
		const float weight = u_kernel[k];
		const float2 offset = u_uv_step * (float)k;
		color += u_texture.Sample(u_sampler_linear_clamp, input.uv + offset).rgb * weight;
		color += u_texture.Sample(u_sampler_linear_clamp, input.uv - offset).rgb * weight;
	}
	return float4(color, 1.0);
}
