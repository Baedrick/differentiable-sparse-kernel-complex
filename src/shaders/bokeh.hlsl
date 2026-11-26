// Copyright (C) 2026 Koh Swee Teck Dedrick.
// Licensed under the MIT license (https://opensource.org/license/mit/).

struct VertexOutput {
	float4 position : SV_POSITION;
	float2 uv       : TEXCOORD0;
};

cbuffer Globals : register(b0) {
	float2 u_uv_step;
	int u_mask_dim;
};

SamplerState u_sampler_linear_clamp : register(s2);
Texture2D u_texture : register(t0);
Buffer<float> u_mask : register(t1);

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
	float3 color = 0.0;
	float weight_sum = 0.0;
	const float mask_center = (u_mask_dim - 1) * 0.5;
	for (int mask_y = 0; mask_y < u_mask_dim; ++mask_y) {
		for (int mask_x = 0; mask_x < u_mask_dim; ++mask_x) {
			const float weight = u_mask[mask_y * u_mask_dim + mask_x];
			if (weight > 0.0) {
				const float2 tap = (float2(mask_x, mask_y) - mask_center) / mask_center;
				color += u_texture.Sample(u_sampler_linear_clamp, input.uv + tap * u_uv_step).rgb * weight;
				weight_sum += weight;
			}
		}
	}
	return float4(color / weight_sum, 1.0);
}
