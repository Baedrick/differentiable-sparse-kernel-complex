// Copyright (C) 2026 Koh Swee Teck Dedrick.
// Licensed under the MIT license (https://opensource.org/license/mit/).

struct VertexOutput {
	float4 position : SV_POSITION;
	float2 uv       : TEXCOORD0;
};

SamplerState u_sampler_linear_clamp : register(s2);
Texture2D u_texture : register(t0);
Texture3D<float3> u_tonemap_lut : register(t1);

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

float3 tony_mc_mapface(float3 c) {
	const float3 e = c / (1.0 + c);
	const float3 lut_dim = 48.0;
	const float3 uvw = e * ((lut_dim - 1.0) / lut_dim) + 0.5 / lut_dim;
	return u_tonemap_lut.SampleLevel(u_sampler_linear_clamp, uvw, 0);
}

float3 srgb_from_linear(float3 c) {
	const float3 srgb_lo = c * 12.92;
	const float3 srgb_hi = pow(abs(c), 1.0 / 2.4) * 1.055 - 0.055;
	return c <= 0.0031308 ? srgb_lo : srgb_hi;
}

float4 ps_main(VertexOutput input) : SV_TARGET {
	float3 color = u_texture.Sample(u_sampler_linear_clamp, input.uv).rgb;
	color = tony_mc_mapface(color);
	color = srgb_from_linear(color);
	return float4(color, 1.0);
}
