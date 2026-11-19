// Copyright (C) 2026 Koh Swee Teck Dedrick.
// Licensed under the MIT license (https://opensource.org/license/mit/).

struct VertexOutput {
	float4 position : SV_POSITION;
	float2 uv       : TEXCOORD0;
};

cbuffer Globals : register(b0) {
	float2 u_uv_step;
};

SamplerState u_sampler_linear_clamp : register(s2);
Texture2D u_texture : register(t0);

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
	float4 color = 0.0f;
	color += u_texture.Sample(u_sampler_linear_clamp, input.uv) * 4.0; // center
	color += u_texture.Sample(u_sampler_linear_clamp, input.uv + u_uv_step * float2(-0.5, -0.5)); // top left
	color += u_texture.Sample(u_sampler_linear_clamp, input.uv + u_uv_step * float2(-0.5,  0.5)); // bottom left
	color += u_texture.Sample(u_sampler_linear_clamp, input.uv + u_uv_step * float2( 0.5, -0.5)); // top right
	color += u_texture.Sample(u_sampler_linear_clamp, input.uv + u_uv_step * float2( 0.5,  0.5)); // bottom right
	return color / 8.0;
}
