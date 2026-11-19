// Copyright (C) 2026 Koh Swee Teck Dedrick.
// Licensed under the MIT license (https://opensource.org/license/mit/).

dk::PG_Context *dk::pg_context;

#if !defined(EXR_IMAGE_H)
#	define EXR_IMAGE_IMPLEMENTATION
#	include "thirdparty/bartwe/exr_image.h"
#	undef EXR_IMAGE_IMPLEMENTATION
#endif

#if !defined(STBI_INCLUDE_STB_IMAGE_H)
#	define STB_IMAGE_IMPLEMENTATION
#	define STBI_ONLY_PNG
#	define STBI_ONLY_JPEG
#	define STBI_ONLY_HDR
#	include "thirdparty/stb/stb_image.h"
#	undef STB_IMAGE_IMPLEMENTATION
#endif

auto dk::pg_r11g11b10_from_rgba32f_pixels(Arena *arena, f32 const *pixels, u32 width, u32 height) noexcept -> u32 * {
	u64 const num_pixels = width * height;
	u32 *pixels_r11g11b10 = arena_push_array<u32>(arena, num_pixels);
	for (u64 idx = 0; idx < num_pixels; ++idx) {
		f32 const r = pixels[idx * 4 + 0];
		f32 const g = pixels[idx * 4 + 1];
		f32 const b = pixels[idx * 4 + 2];
		DirectX::PackedVector::XMFLOAT3PK pv = {};
		DirectX::PackedVector::XMStoreFloat3PK(&pv, DirectX::XMVectorSet(r, g, b, 0.0f));
		pixels_r11g11b10[idx] = pv.v;
	}
	return pixels_r11g11b10;
}

auto dk::pg_image_load_thread_entry_point(void *p) noexcept -> void {
	ZoneScoped;
	PG_ImageLoadParams *params = static_cast<PG_ImageLoadParams *>(p);
	Arena *const arena = params->arena;

	//~ Dedrick: Sniff supported file inputs.
	PG_FileFormat file_format = PG_FileFormat_Null;
	String8 input_file_path = params->file_path;
	{
		//~ Dedrick: Possibly relative -> absolute path.
		{
			PathStyle path_style = path_style_from_str8(input_file_path);
			if (path_style == PathStyle_Relative) {
				String8 const working_dir = get_current_dir(arena);
				String8 const abs_path = str8f(arena, "%.*s/%.*s", DK_STR8_VARG(working_dir), DK_STR8_VARG(input_file_path));
				input_file_path = path_normalized_from_path(arena, abs_path);
			}
		}
		File const file = file_open(input_file_path, FileAccessFlag_Read | FileAccessFlag_ShareRead);
		dk_defer(file_close(file));

		//~ Dedrick: EXR magic -> EXR input.
		if (file_format == PG_FileFormat_Null) {
			u32 const exr_magic = 0x01312F76;
			u32 exr_magic_maybe = 0;
			file_read(file, 0, sizeof(exr_magic_maybe), &exr_magic_maybe);
			if (exr_magic_maybe == exr_magic) {
				file_format = PG_FileFormat_EXR;
			}
		}

		//~ Dedrick: HDR magic -> HDR input.
		if (file_format == PG_FileFormat_Null) {
			u8 const hdr_magic[] = { 0x23, 0x3F, 0x52, 0x41, 0x44, 0x49, 0x41, 0x4E, 0x43, 0x45, 0x0A };
			u8 hdr_magic_maybe[sizeof(hdr_magic)] = {};
			file_read(file, 0, sizeof(hdr_magic_maybe), &hdr_magic_maybe);
			if (memory_match(hdr_magic_maybe, hdr_magic, sizeof(hdr_magic_maybe))) {
				file_format = PG_FileFormat_HDR;
			}
		}

		//~ Dedrick: PNG magic -> PNG input.
		if (file_format == PG_FileFormat_Null) {
			u64 const png_magic = 0x0A1A0A0D474E5089;
			u64 png_magic_maybe = 0;
			file_read(file, 0, sizeof(png_magic_maybe), &png_magic_maybe);
			if (png_magic_maybe == png_magic) {
				file_format = PG_FileFormat_PNG;
			}
		}

		//~ Dedrick: JPEG magic -> JPEG input.
		if (file_format == PG_FileFormat_Null) {
			u8 const jpeg_magic[] = { 0xFF, 0xD8, 0xFF };
			u8 jpeg_magic_maybe[sizeof(jpeg_magic)] = {};
			file_read(file, 0, sizeof(jpeg_magic_maybe), &jpeg_magic_maybe);
			if (memory_match(jpeg_magic_maybe, jpeg_magic, sizeof(jpeg_magic_maybe))) {
				file_format = PG_FileFormat_JPEG;
			}
		}
	}

	//~ Dedrick: Load recognized file types.
	String8 file_bytes = {};
	if (file_format != PG_FileFormat_Null) {
		file_bytes = read_bytes_from_file_path(arena, input_file_path);
	}

	//~ Dedrick: Parse image file.
	u32 image_w = 0;
	u32 image_h = 0;
	u32 image_c = 0;
	DXGI_FORMAT image_format = DXGI_FORMAT_UNKNOWN;
	void *image_pixels = nullptr;

	//~ Dedrick: EXR input -> Parse EXR.
	if (file_format == PG_FileFormat_EXR) {
		s32 width = 0;
		s32 height = 0;
		s32 channels = 0;
		f32 *pixels_f32 = nullptr;
		if (exri_loadf_from_memory(&pixels_f32, file_bytes.data, file_bytes.size, &width, &height, &channels, 4, EXRI_LOAD_DEFAULT) != 0) {
			image_pixels = pg_r11g11b10_from_rgba32f_pixels(arena, pixels_f32, width, height);
			image_w = static_cast<u32>(width);
			image_h = static_cast<u32>(height);
			image_c = static_cast<u32>(channels);
			image_format = DXGI_FORMAT_R11G11B10_FLOAT;
			exri_image_free(pixels_f32);
		}
	}

	//~ Dedrick: HDR input -> Parse HDR.
	if (file_format == PG_FileFormat_HDR) {
		s32 width = 0;
		s32 height = 0;
		s32 channels = 0;
		f32 *pixels_f32 = stbi_loadf_from_memory(file_bytes.data, static_cast<s32>(file_bytes.size), &width, &height, &channels, 4);
		if (pixels_f32 != nullptr) {
			image_pixels = pg_r11g11b10_from_rgba32f_pixels(arena, pixels_f32, width, height);
			image_w = static_cast<u32>(width);
			image_h = static_cast<u32>(height);
			image_c = static_cast<u32>(channels);
			image_format = DXGI_FORMAT_R11G11B10_FLOAT;
			stbi_image_free(pixels_f32);
		}
	}

	//~ Dedrick: PNG/JPEG input -> Parse PNG/JPEG.
	if (file_format == PG_FileFormat_PNG || file_format == PG_FileFormat_JPEG) {
		s32 width = 0;
		s32 height = 0;
		s32 channels = 0;
		u8 *pixels = stbi_load_from_memory(file_bytes.data, static_cast<s32>(file_bytes.size), &width, &height, &channels, 4);
		if (pixels != nullptr) {
			u64 const num_pixels = width * height * 4;
			u8 *pixels_u8 = arena_push_array<u8>(arena, num_pixels);
		 	memory_copy(pixels_u8, pixels, num_pixels);
			image_pixels = pixels_u8;
			image_w = static_cast<u32>(width);
			image_h = static_cast<u32>(height);
			image_c = static_cast<u32>(channels);
			image_format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
			stbi_image_free(pixels);
		}
	}

	//~ Dedrick: Store, if successful.
	if (image_pixels != nullptr) {
		PG_ImageData *image_data = arena_push<PG_ImageData>(arena);
		image_data->dim = { image_w, image_h };
		image_data->format = image_format;
		image_data->pixels = image_pixels;
		params->image_data = image_data;
	}

	//~ Dedrick: Signal completion.
	semaphore_signal(params->load_completion_semaphore);
}

auto dk::pg_benchmark_sample_chunk_list_push(Arena *arena, PG_BenchmarkSampleChunkList *list, u64 capacity, PG_BenchmarkSample const *sample) noexcept -> void {
	PG_BenchmarkSampleChunkNode *node = list->last;
	if (node == nullptr || node->count >= node->capacity) {
		node = arena_push<PG_BenchmarkSampleChunkNode>(arena);
		node->capacity = capacity;
		node->v = arena_push_array<PG_BenchmarkSample>(arena, node->capacity);
		forward_list_queue_push(&list->first, &list->last, node);
		list->chunk_count += 1;
	}
	memory_copy_struct(&node->v[node->count], sample);
	node->count += 1;
	list->total_count += 1;
}

auto dk::pg_benchmark_fill_radii(Arena *arena, PG_Benchmark *benchmark, s32 max_radius) noexcept -> void {
	//~ Dedrick: Geometric radius sweep points, at ~x1.4 growth, ending at the exact max radius.
	u64 count = 1;
	for (s32 radius = 5; radius * 11 < max_radius * 10; ) {
		radius = (radius * 14 + 5) / 10;
		count += 1;
	}

	//~ Dedrick: Allocate & fill.
	benchmark->radius_count = count;
	benchmark->radii = arena_push_array<s32>(arena, count);
	u64 idx = 0;
	for (s32 radius = 5; radius * 11 < max_radius * 10; ) {
		benchmark->radii[idx++] = radius;
		radius = (radius * 14 + 5) / 10;
	}
	benchmark->radii[idx++] = max_radius;
}

auto dk::pg_frame_arena() noexcept -> Arena * {
	return pg_context->frame_arenas[pg_context->frame_index % array_count(pg_context->frame_arenas)];
}

auto dk::pg_set_target_frame_time_and_snapping_from_monitor(RGFW_monitor const *monitor) noexcept -> void {
	f32 target_refresh_rate = 60.0f;
	if (monitor->mode.refreshRate > 0.0f) {
		target_refresh_rate = monitor->mode.refreshRate;
	}
	pg_context->target_frame_time_us = static_cast<s64>(1e+6f / target_refresh_rate);
	pg_context->vsync_max_error_us = 200; // 0.2ms
	for (u32 idx = 0; idx < array_count(pg_context->snap_frequencies); ++idx) {
		pg_context->snap_frequencies[idx] = pg_context->target_frame_time_us * (idx + 1);
	}
	for (u32 idx = 0; idx < array_count(pg_context->time_averager); ++idx) {
		pg_context->time_averager[idx] = pg_context->target_frame_time_us;
	}
	pg_context->time_averager_residual = 0;
}

auto dk::pg_blur_texture_cache_get_or_create(uvec2 dim, DXGI_FORMAT format) noexcept -> PG_BlurTexture * {
	PG_BlurTexture *result = nullptr;
	for (PG_BlurTexture *node = pg_context->blur_texture_free_first; node != nullptr; node = node->next) {
		if (node->dim.x == dim.x && node->dim.y == dim.y && node->format == format) {
			result = node;
			list_remove(&pg_context->blur_texture_free_first, &pg_context->blur_texture_free_last, node);
			break;
		}
	}
	if (result == nullptr) {
		result = arena_push<PG_BlurTexture>(pg_context->blur_texture_arena);
		result->dim = dim;
		result->format = format;
		D3D11_TEXTURE2D_DESC desc = {};
		{
			desc.Width = result->dim.x;
			desc.Height = result->dim.y;
			desc.MipLevels = 1;
			desc.ArraySize = 1;
			desc.Format = result->format;
			desc.SampleDesc.Count = 1;
			desc.Usage = D3D11_USAGE_DEFAULT;
			desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		}
		D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = {};
		{
			rtv_desc.Format = desc.Format;
			rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		}
		D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
		{
			srv_desc.Format = desc.Format;
			srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			srv_desc.Texture2D.MipLevels = static_cast<UINT>(-1);
		}
		d3d11_device()->CreateTexture2D(&desc, nullptr, &result->texture);
		d3d11_device()->CreateRenderTargetView(result->texture, &rtv_desc, &result->rtv);
		d3d11_device()->CreateShaderResourceView(result->texture, &srv_desc, &result->srv);
	}
	list_push_back(&pg_context->blur_texture_first, &pg_context->blur_texture_last, result);
	return result;
}

auto dk::pg_blur_texture_cache_reset() noexcept -> void {
	//~ Dedrick: Concat used to free list in-place.
	if (pg_context->blur_texture_first != nullptr) {
		if (pg_context->blur_texture_free_last != nullptr) {
			pg_context->blur_texture_free_last->next = pg_context->blur_texture_first;
			pg_context->blur_texture_first->prev = pg_context->blur_texture_free_last;
			pg_context->blur_texture_free_last = pg_context->blur_texture_last;
		}
		else {
			pg_context->blur_texture_free_first = pg_context->blur_texture_first;
			pg_context->blur_texture_free_last = pg_context->blur_texture_last;
		}
		pg_context->blur_texture_first = nullptr;
		pg_context->blur_texture_last = nullptr;
	}
}

auto dk::pg_display_entry_list_push(Arena *arena, PG_DisplayEntryList *list, String8 name, uvec2 dim, ID3D11ShaderResourceView *srv) noexcept -> void {
	PG_DisplayEntryNode *node = arena_push<PG_DisplayEntryNode>(arena);
	node->v.label = str8f(arena, "%.*s: %ux%u", DK_STR8_VARG(name), dim.x, dim.y);
	node->v.srv = srv;
	forward_list_queue_push(&list->first, &list->last, node);
	list->count += 1;
}

auto dk::pg_display_entry_list_concat_in_place(PG_DisplayEntryList *list, PG_DisplayEntryList *to_push) noexcept -> void {
	if (to_push->count > 0) {
		if (list->last != nullptr) {
			list->count += to_push->count;
			list->last->next = to_push->first;
			list->last = to_push->last;
		}
		else {
			*list = *to_push;
		}
		memory_zero(to_push);
	}
}

auto dk::pg_display_entry_array_from_list(Arena *arena, PG_DisplayEntryList const *list) noexcept -> PG_DisplayEntryArray {
	PG_DisplayEntryArray result = {};
	result.count = list->count;
	result.v = arena_push_array<PG_DisplayEntry>(arena, result.count);
	u64 idx = 0;
	for (PG_DisplayEntryNode *node = list->first; node != nullptr; node = node->next) {
		result.v[idx++] = node->v;
	}
	return result;
}

auto dk::pg_display_selected_entry_idx(PG_DisplayEntryArray const *entries) noexcept -> u64 {
	u64 idx = entries->count - 1;
	if (pg_context->selected_display_entry_idx != 0) {
		idx = min(pg_context->selected_display_entry_idx - 1, entries->count - 1);
	}
	return idx;
}

//~ Dedrick: @blur_impl
auto dk::pg_blur(Arena *arena, PG_BlurParams const *params) noexcept -> PG_DisplayEntryList {
	PG_DisplayEntryList display_entries = {};
	if (params->blur_radius > 0) {
		TempArena const scratch = scratch_begin(&arena, 1);
		dk_defer(scratch_end(scratch));

		ID3D11DeviceContext1 *const d_ctx = d3d11_device_context();
		switch (params->blur_kind) {
			case PG_BlurKind_DualFiltering: {
				ID3D11VertexShader *v_shader_down = pg_context->v_shaders[PG_VShaderKind_DualFilteringDown];
				ID3D11PixelShader *p_shader_down = pg_context->p_shaders[PG_PShaderKind_DualFilteringDown];
				ID3D11VertexShader *v_shader_up = pg_context->v_shaders[PG_VShaderKind_DualFilteringUp];
				ID3D11PixelShader *p_shader_up = pg_context->p_shaders[PG_PShaderKind_DualFilteringUp];
				ID3D11Buffer *uniform_buffer = pg_context->uniform_buffers[PG_UniformKind_DualFiltering];
				uvec2 const src_dim = params->src_dim;

				//~ Dedrick: Count maximum mip down/up.
				u32 max_pass_count = 0;
				{
					uvec2 dim = src_dim;
					while (dim.x > 1 || dim.y > 1) {
						dim = max(uvec2{ 1, 1 }, uvec2{ dim.x >> 1, dim.y >> 1 });
						max_pass_count += 1;
					}
				}

				//~ Dedrick: @dual_filtering
				f32 const radius = static_cast<f32>(params->blur_radius);
				u32 const pass_count = min(max_pass_count, max<u32>(1, static_cast<u32>(round(0.6214f * log_e(radius) + 0.2908f))));
				f32 const distance = 0.5287f * radius / sqrt(static_cast<f32>((1u << (2 * pass_count)) - 1u));

				//~ Dedrick: Build blur mip chain & set up uniforms.
				PG_BlurTexture **mips = arena_push_array<PG_BlurTexture *>(scratch.arena, pass_count + 1);
				{
					PG_Uniform_DualFiltering uniforms = {};

					//~ Dedrick: Fill uniforms for blur mip chain.
					uvec2 dim = src_dim;
					mips[0] = pg_blur_texture_cache_get_or_create(dim, DXGI_FORMAT_R11G11B10_FLOAT);
					uniforms.passes[0].uv_step = { distance / dim.x, distance / dim.y };
					for (u32 pass = 1; pass <= pass_count; ++pass) {
						dim = max(uvec2{ 1, 1 }, uvec2{ dim.x >> 1, dim.y >> 1 });
						mips[pass] = pg_blur_texture_cache_get_or_create(dim, DXGI_FORMAT_R11G11B10_FLOAT);
						uniforms.passes[pass].uv_step = { distance / dim.x, distance / dim.y };
					}

					D3D11_MAPPED_SUBRESOURCE mapped_cbuffer = {};
					d_ctx->Map(uniform_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_cbuffer);
					memory_copy_struct(mapped_cbuffer.pData, &uniforms);
					d_ctx->Unmap(uniform_buffer, 0);
				}

				u32 const uniform_stride = static_cast<u32>(sizeof(PG_Uniform_DualFilteringPass) / 16);

				//~ Dedrick: Set input assembly.
				d_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				d_ctx->IASetInputLayout(nullptr);

				// NOTE(Dedrick): For unbinding rtv.
				ID3D11RenderTargetView *rtv = nullptr;

				//~ Dedrick: Downsample passes.
				{
					d_ctx->VSSetShader(v_shader_down, nullptr, 0);
					d_ctx->PSSetShader(p_shader_down, nullptr, 0);
					ID3D11ShaderResourceView *srv = params->src_srv;
					for (u32 pass = 1; pass <= pass_count; ++pass) {
						PG_BlurTexture *const blur_texture = mips[pass];
						D3D11_VIEWPORT const viewport = { 0.0f, 0.0f, static_cast<f32>(blur_texture->dim.x), static_cast<f32>(blur_texture->dim.y), 0.0f, 1.0f };
						d_ctx->RSSetViewports(1, &viewport);
						d_ctx->OMSetRenderTargets(1, &blur_texture->rtv, nullptr);
						u32 const uniform_offset = uniform_stride * pass;
						d_ctx->PSSetConstantBuffers1(0, 1, &uniform_buffer, &uniform_offset, &uniform_stride);
						d_ctx->PSSetShaderResources(0, 1, &srv);
						d_ctx->Draw(3, 0);
						d_ctx->OMSetRenderTargets(1, &rtv, nullptr);
						pg_display_entry_list_push(arena, &display_entries, str8f(arena, "Down %d", pass), blur_texture->dim, blur_texture->srv);
						srv = blur_texture->srv;
					}
				}

				//~ Dedrick: Upsample passes.
				{
					d_ctx->VSSetShader(v_shader_up, nullptr, 0);
					d_ctx->PSSetShader(p_shader_up, nullptr, 0);
					for (u32 pass = pass_count; pass --> 0; ) {
						PG_BlurTexture *const blur_texture = mips[pass];
						D3D11_VIEWPORT const viewport = { 0.0f, 0.0f, static_cast<f32>(blur_texture->dim.x), static_cast<f32>(blur_texture->dim.y), 0.0f, 1.0f };
						d_ctx->RSSetViewports(1, &viewport);
						d_ctx->OMSetRenderTargets(1, &blur_texture->rtv, nullptr);
						u32 const uniform_offset = uniform_stride * pass;
						d_ctx->PSSetConstantBuffers1(0, 1, &uniform_buffer, &uniform_offset, &uniform_stride);
						d_ctx->PSSetShaderResources(0, 1, &mips[pass + 1]->srv);
						d_ctx->Draw(3, 0);
						d_ctx->OMSetRenderTargets(1, &rtv, nullptr);
						pg_display_entry_list_push(arena, &display_entries, str8f(arena, "Up %d", pass_count - pass), blur_texture->dim, blur_texture->srv);
					}
				}
				break;
			}
			case PG_BlurKind_SeparableGaussian: {
				ID3D11VertexShader *v_shader = pg_context->v_shaders[PG_VShaderKind_SeparableGaussian];
				ID3D11PixelShader *p_shader = pg_context->p_shaders[PG_PShaderKind_SeparableGaussian];
				ID3D11Buffer *uniform_buffer = pg_context->uniform_buffers[PG_UniformKind_SeparableGaussian];
				uvec2 const src_dim = params->src_dim;

				//~ Dedrick: Blur radius changed -> calculate new kernel.
				s32 const kernel_size = params->blur_radius + 1;
				if (pg_context->gaussian_kernel_radius != params->blur_radius) {
					pg_context->gaussian_kernel_radius = params->blur_radius;

					// NOTE(Dedrick): Derivation of kernel constants.
					//
					//   Standard Gaussian: G(x) = (1 / sqrt(2 * PI * sigma^2)) * exp(-x^2 / (2 * sigma^2))
					//
					// The effective blur radius R of a Gaussian is 3 sigma (6 sigma for diameter):
					//
					//   R = 3 * sigma -> sigma = R / 3
					//
					// Substituting sigma into the exponent:
					//
					//   -x^2 / (2 * (R / 3)^2) = -9 * x^2 / (2 * R^2) = -4.5 * (x / R)^2
					//
					// For discrete taps x = i in [0, R]:
					//   weight[i] proportional to exp(-4.5 * (i / R)^2)
					// The leading factor (1 / sqrt(2 * PI * sigma^2)) is constant across all taps and cancels
					// out completely during discrete normalization (sum(weights) = 1.0). exp(0) == 1.0.
					f32 *const kernel = arena_push_array<f32>(scratch.arena, kernel_size);
					f32 const radius = static_cast<f32>(params->blur_radius);
					f32 const inv_radius = 1.0f / radius;
					f32 const factor = -4.5f * inv_radius * inv_radius;

					//~ Dedrick: Fill kernel.
					kernel[0] = 1.0f;
					f32 sum = 1.0f;
					for (s32 i = 1; i < kernel_size; ++i) {
						f32 const fi = static_cast<f32>(i);
						f32 const w = exp(factor * (fi * fi));
						kernel[i] = w;
						sum += w * 2.0f;
					}

					//~ Dedrick: Normalize to get energy == 1.0.
					f32 const inv_sum = 1.0f / sum;
					for (s32 i = 0; i < kernel_size; ++i) {
						kernel[i] *= inv_sum;
					}

					D3D11_MAPPED_SUBRESOURCE mapped = {};
					d_ctx->Map(pg_context->gaussian_kernel_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
					memory_copy(mapped.pData, kernel, kernel_size * sizeof(f32));
					d_ctx->Unmap(pg_context->gaussian_kernel_buffer, 0);
				}

				//~ Dedrick: Set up uniforms.
				{
					PG_Uniform_Gaussian uniforms = {};
					f32 const inv_w = 1.0f / static_cast<f32>(src_dim.x);
					f32 const inv_h = 1.0f / static_cast<f32>(src_dim.y);

					//~ Dedrick: Horizontal pass uniforms.
					uniforms.passes[Axis2_X].uv_step     = { inv_w, 0.0f };
					uniforms.passes[Axis2_X].kernel_size = kernel_size;

					//~ Dedrick: Vertical pass uniforms.
					uniforms.passes[Axis2_Y].uv_step     = { 0.0f, inv_h };
					uniforms.passes[Axis2_Y].kernel_size = kernel_size;

					D3D11_MAPPED_SUBRESOURCE mapped_cbuffer = {};
					d_ctx->Map(uniform_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_cbuffer);
					memory_copy_struct(mapped_cbuffer.pData, &uniforms);
					d_ctx->Unmap(uniform_buffer, 0);
				}

				u32 const uniform_offsets[Axis2_COUNT] = {
					0 * sizeof(PG_Uniform_GaussianPass) / 16,
					1 * sizeof(PG_Uniform_GaussianPass) / 16,
				};
				u32 const uniform_counts[Axis2_COUNT] = {
					sizeof(PG_Uniform_GaussianPass) / 16,
					sizeof(PG_Uniform_GaussianPass) / 16,
				};

				//~ Dedrick: Set rasterizer.
				D3D11_VIEWPORT const viewport = { 0.0f, 0.0f, static_cast<f32>(src_dim.x), static_cast<f32>(src_dim.y), 0.0f, 1.0f };
				d_ctx->RSSetViewports(1, &viewport);

				//~ Dedrick: Set input assembly.
				d_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				d_ctx->IASetInputLayout(nullptr);

				//~ Dedrick: Set shaders.
				d_ctx->VSSetShader(v_shader, nullptr, 0);
				d_ctx->PSSetShader(p_shader, nullptr, 0);
				d_ctx->PSSetShaderResources(1, 1, &pg_context->gaussian_kernel_buffer_srv);

				// NOTE(Dedrick): For unbinding rtv.
				ID3D11RenderTargetView *rtv = nullptr;

				//~ Dedrick: Horizontal pass.
				PG_BlurTexture *const blur_h = pg_blur_texture_cache_get_or_create(src_dim, DXGI_FORMAT_R11G11B10_FLOAT);
				d_ctx->OMSetRenderTargets(1, &blur_h->rtv, nullptr);
				d_ctx->PSSetConstantBuffers1(0, 1, &uniform_buffer, &uniform_offsets[Axis2_X], &uniform_counts[Axis2_X]);
				d_ctx->PSSetShaderResources(0, 1, &params->src_srv);
				d_ctx->Draw(3, 0);
				d_ctx->OMSetRenderTargets(1, &rtv, nullptr);
				pg_display_entry_list_push(arena, &display_entries, "Horizontal"_str8, blur_h->dim, blur_h->srv);

				//~ Dedrick: Vertical pass.
				PG_BlurTexture *const blur_v = pg_blur_texture_cache_get_or_create(src_dim, DXGI_FORMAT_R11G11B10_FLOAT);
				d_ctx->OMSetRenderTargets(1, &blur_v->rtv, nullptr);
				d_ctx->PSSetConstantBuffers1(0, 1, &uniform_buffer, &uniform_offsets[Axis2_Y], &uniform_counts[Axis2_Y]);
				d_ctx->PSSetShaderResources(0, 1, &blur_h->srv);
				d_ctx->Draw(3, 0);
				d_ctx->OMSetRenderTargets(1, &rtv, nullptr);
				pg_display_entry_list_push(arena, &display_entries, "Vertical"_str8, blur_v->dim, blur_v->srv);
				break;
			}
		}
	}
	return display_entries;
}

auto dk::pg_init(CmdLine *cmd_line) noexcept -> void {
	ZoneScoped;
	(void)cmd_line;

	//~ Dedrick: Set up state.
	Arena *arena = arena_alloc();
	pg_context = arena_push<PG_Context>(arena);
	pg_context->arena = arena;
	for (u32 i = 0; i < array_count(pg_context->frame_arenas); ++i) {
		pg_context->frame_arenas[i] = arena_alloc();
	}
	pg_context->log = log_alloc();
	log_select(pg_context->log);
	pg_context->image_load_request_arena = arena_alloc();
	pg_context->source_texture_arena = arena_alloc();
	pg_context->blur_texture_arena = arena_alloc();

	//~ Dedrick: Build vertex shaders.
	{
		ZoneScopedN("build vertex shaders");
		for (PG_VShaderKind kind = static_cast<PG_VShaderKind>(0); kind < PG_VShaderKind_COUNT; kind = static_cast<PG_VShaderKind>(kind + 1)) {
			String8 const source = pg_v_shader_kind_src_table[kind];
			ID3D11VertexShader *v_shader = nullptr;
			d3d11_device()->CreateVertexShader(source.data, source.size, nullptr, &v_shader);
			pg_context->v_shaders[kind] = v_shader;
		}
	}

	//~ Dedrick: Build pixel shaders.
	{
		ZoneScopedN("build pixel shaders");
		for (PG_PShaderKind kind = static_cast<PG_PShaderKind>(0); kind < PG_PShaderKind_COUNT; kind = static_cast<PG_PShaderKind>(kind + 1)) {
			String8 const source = pg_p_shader_kind_src_table[kind];
			ID3D11PixelShader *p_shader = nullptr;
			d3d11_device()->CreatePixelShader(source.data, source.size, nullptr, &p_shader);
			pg_context->p_shaders[kind] = p_shader;
		}
	}

	//~ Dedrick: Set up uniforms.
	{
		ZoneScopedN("set up uniforms");
		for (PG_UniformKind kind = static_cast<PG_UniformKind>(0); kind < PG_UniformKind_COUNT; kind = static_cast<PG_UniformKind>(kind + 1)) {
			ID3D11Buffer *buffer = nullptr;
			{
				D3D11_BUFFER_DESC desc = {};
				desc.ByteWidth = static_cast<UINT>(pg_uniform_type_kind_size_table[kind]);
				desc.ByteWidth += 15;
				desc.ByteWidth -= desc.ByteWidth % 16;
				desc.Usage = D3D11_USAGE_DYNAMIC;
				desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
				desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
				d3d11_device()->CreateBuffer(&desc, nullptr, &buffer);
			}
			pg_context->uniform_buffers[kind] = buffer;
		}
	}

	//~ Dedrick: Create Tonemap.
	{
		TempArena const scratch = scratch_begin(nullptr, 0);
		dk_defer(scratch_end(scratch));

		DDS_Parsed const *dds = dds_parsed_from_bytes(scratch.arena, pg_tonemap_lut_file_bytes);
		DDS_SubresourceArray const subresources = dds_subresource_array_from_parsed(scratch.arena, dds);
		D3D11_SUBRESOURCE_DATA *initial_data = arena_push_array<D3D11_SUBRESOURCE_DATA>(scratch.arena, subresources.mip_count);
		for (u32 mip = 0; mip < subresources.mip_count; ++mip) {
			initial_data[mip].pSysMem = subresources[mip].bytes.data;
			initial_data[mip].SysMemPitch = subresources[mip].row_pitch;
			initial_data[mip].SysMemSlicePitch = subresources[mip].slice_pitch;
		}
		D3D11_TEXTURE3D_DESC desc = {};
		desc.Width = subresources[0].width;
		desc.Height = subresources[0].height;
		desc.Depth = subresources[0].depth;
		desc.MipLevels = subresources.mip_count;
		desc.Format = dds->format;
		desc.Usage = D3D11_USAGE_IMMUTABLE;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		d3d11_device()->CreateTexture3D(&desc, initial_data, &pg_context->tonemap_lut);
		d3d11_device()->CreateShaderResourceView(pg_context->tonemap_lut, nullptr, &pg_context->tonemap_lut_srv);
	}

	//~ Dedrick: Set up samplers.
	{
		ID3D11SamplerState *const samplers[4] = {
			d3d11_sampler(D3D11_SampleKind_NearestClamp),
			d3d11_sampler(D3D11_SampleKind_Nearest),
			d3d11_sampler(D3D11_SampleKind_LinearClamp),
			d3d11_sampler(D3D11_SampleKind_Linear)
		};
		d3d11_device_context()->PSSetSamplers(0, static_cast<UINT>(array_count(samplers)), samplers);
	}

	//~ Dedrick: Set up gaussian state.
	{
		D3D11_BUFFER_DESC desc = {};
		{
			desc.ByteWidth = static_cast<UINT>((PG_BlurRadiusMax + 1) * sizeof(f32));
			desc.Usage = D3D11_USAGE_DYNAMIC;
			desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		}
		D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
		{
			srv_desc.Format = DXGI_FORMAT_R32_FLOAT;
			srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
			srv_desc.Buffer.FirstElement = 0;
			srv_desc.Buffer.NumElements = PG_BlurRadiusMax + 1;
		}
		d3d11_device()->CreateBuffer(&desc, nullptr, &pg_context->gaussian_kernel_buffer);
		d3d11_device()->CreateShaderResourceView(pg_context->gaussian_kernel_buffer, &srv_desc, &pg_context->gaussian_kernel_buffer_srv);
	}

	//~ Dedrick: Set up benchmark.
	{
		pg_context->benchmark_arena = arena_alloc();

		//~ Dedrick: Enable all blurs for benchmarking.
		for (PG_BlurKind k = static_cast<PG_BlurKind>(0); k < PG_BlurKind_COUNT; k = static_cast<PG_BlurKind>(k + 1)) {
			pg_context->benchmark_blur_kind_flags |= 1u << k;
		}

		//~ Dedrick: Query objects.
		D3D11_QUERY_DESC desc = {};
		{
			desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
			d3d11_device()->CreateQuery(&desc, &pg_context->benchmark_disjoint_query);
		}
		{
			desc.Query = D3D11_QUERY_TIMESTAMP;
			d3d11_device()->CreateQuery(&desc, &pg_context->benchmark_start_query);
			d3d11_device()->CreateQuery(&desc, &pg_context->benchmark_end_query);
		}
		{
			desc.Query = D3D11_QUERY_EVENT;
			d3d11_device()->CreateQuery(&desc, &pg_context->benchmark_event_query);
		}
	}

	//~ Dedrick: Set up main window.
	{
		pg_context->window = dt_window_open("playground"_str8, 0, 0, 960, 768, RGFW_windowCenter | RGFW_windowAllowDND);
		RGFW_window_setMinSize(pg_context->window, 800, 600);
		pg_context->window_d3d11 = d3d11_window_equip(pg_context->window);
		pg_context->active_monitor = RGFW_window_getMonitor(pg_context->window);
		pg_set_target_frame_time_and_snapping_from_monitor(pg_context->active_monitor);
		RGFW_window_setUserPtr(pg_context->window, pg_context->window_d3d11);
	}

	//~ Dedrick: Set up ImGui.
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO &io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		{
			String8 const binary_dir = get_process_info()->binary_dir;
			pg_context->ini_path = str8f(pg_context->arena, "%.*s/imgui.ini", DK_STR8_VARG(binary_dir));
			io.IniFilename = reinterpret_cast<char const *>(pg_context->ini_path.data);
		}
		ImGui_ImplRgfw_InitForOther(pg_context->window, true);
		ImGui_ImplDX11_Init(d3d11_device(), d3d11_device_context());
	}

	//~ Dedrick: Begin measuring per-frame work.
	pg_context->last_frame_time_us = now_time_us();
}

auto dk::pg_shutdown() noexcept -> void {
	// NOTE(Dedrick): ImGui needs proper shutdown to persist configs.
	ImGui_ImplDX11_Shutdown();
	ImGui_ImplRgfw_Shutdown();
	ImGui::DestroyContext();
}

auto dk::pg_frame() noexcept -> b8 {
	ZoneScoped;
	TempArena const scratch = scratch_begin(nullptr, 0);
	dk_defer(scratch_end(scratch));

	//~ Dedrick: Determine frame time.
	// https://medium.com/@tglaiel/how-to-make-your-game-run-at-60fps-24c61210fe75
	{
		u64 const current_time_us = now_time_us();
		s64 delta_time = static_cast<s64>(current_time_us - pg_context->last_frame_time_us);
		pg_context->last_frame_time_us = current_time_us;
		if (delta_time > pg_context->target_frame_time_us * 8) {
			delta_time = pg_context->target_frame_time_us;
		}
		if (delta_time < 0) {
			delta_time = 0;
		}
		for (u64 i = 0; i < array_count(pg_context->snap_frequencies); ++i) {
			s64 const snap = pg_context->snap_frequencies[i];
			if (abs(delta_time - snap) < pg_context->vsync_max_error_us) {
				delta_time = snap;
				break;
			}
		}
		u64 const history_count = array_count(pg_context->time_averager);
		pg_context->time_averager[pg_context->time_averager_head] = delta_time;
		pg_context->time_averager_head = (pg_context->time_averager_head + 1) % history_count;

		u64 averager_sum = 0;
		for (u32 i = 0; i < array_count(pg_context->time_averager); ++i) {
			averager_sum += pg_context->time_averager[i];
		}

		delta_time = averager_sum / history_count;
		pg_context->time_averager_residual += averager_sum % history_count;
		delta_time += pg_context->time_averager_residual / history_count;
		pg_context->time_averager_residual %= history_count;

		f32 const frame_dt = static_cast<f32>(delta_time) / 1000000.0f;
		pg_context->frame_dt = frame_dt;
		pg_context->time_in_seconds += frame_dt;
	}

	//~ Dedrick: Do per-frame resets.
	{
		arena_clear(pg_frame_arena());
	}

	//~ Dedrick: Begin log frame scope.
	log_frame_begin();

	//~ Dedrick: Get & consume platform events.
	DT_EventList const events = dt_get_events(scratch.arena);
	{
		ZoneScopedN("consume platform events");
		for (DT_EventNode const *node = events.first; node != nullptr; node = node->next) {
			RGFW_window *window = node->event.common.win;
			switch (node->event.type) {
				case RGFW_windowClose: {
					pg_context->quit = true;
					break;
				}
				case RGFW_windowMoved: {
					RGFW_monitor const *monitor = RGFW_window_getMonitor(window);
					if (pg_context->active_monitor != monitor) {
						pg_context->active_monitor = monitor;
						pg_set_target_frame_time_and_snapping_from_monitor(pg_context->active_monitor);
					}
					break;
				}
				case RGFW_dataDrop: {
					String8 const drop_path = str8_cstring(reinterpret_cast<u8 const *>(node->event.drop.value->data));
					arena_clear(pg_context->image_load_request_arena);
					pg_context->image_load_request_path = str8_copy(pg_context->image_load_request_arena, drop_path);
					pg_context->image_load_request_id += 1;
					break;
				}
			}
		}
	}

	//~ Dedrick: Finish file load.
	{
		ZoneScopedN("finish file load");
		b8 const async_load_ready = pg_context->image_load_params != nullptr && semaphore_wait(pg_context->image_load_params->load_completion_semaphore, 0);
		if (async_load_ready) {
			thread_join(pg_context->image_load_thread);
			PG_ImageLoadParams *const params = pg_context->image_load_params;
			PG_ImageData *const image = params->image_data;
			if (image == nullptr) {
				DK_LOG_ERRORF("ERROR: failed to load image: %.*s\n", DK_STR8_VARG(params->file_path));
			}
			else if (params->request_id == pg_context->image_load_request_id && image != nullptr) {
				//~ Dedrick: Release previous source texture.
				if (pg_context->source_texture_srv != nullptr) {
					pg_context->source_texture_srv->Release();
					pg_context->source_texture_srv = nullptr;
				}
				if (pg_context->source_texture != nullptr) {
					pg_context->source_texture->Release();
					pg_context->source_texture = nullptr;
				}

				//~ Dedrick: Upload source texture.
				DDS_SurfaceInfo const info = dds_surface_info(image->dim.x, image->dim.y, image->format);
				D3D11_TEXTURE2D_DESC desc = {};
				desc.Width = image->dim.x;
				desc.Height = image->dim.y;
				desc.MipLevels = 1;
				desc.ArraySize = 1;
				desc.Format = image->format;
				desc.SampleDesc.Count = 1;
				desc.Usage = D3D11_USAGE_IMMUTABLE;
				desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
				D3D11_SUBRESOURCE_DATA initial_data = {};
				initial_data.pSysMem = image->pixels;
				initial_data.SysMemPitch = info.row_pitch;
				HRESULT const hr = d3d11_device()->CreateTexture2D(&desc, &initial_data, &pg_context->source_texture);
				if (FAILED(hr)) {
					dt_show_dialog(pg_context->window, "Fatal Error"_str8, "Couldn't create source texture"_str8, true);
					abort_self(0);
				}
				d3d11_device()->CreateShaderResourceView(pg_context->source_texture, nullptr, &pg_context->source_texture_srv);

				//~ Dedrick: Fill source texture state.
				arena_clear(pg_context->source_texture_arena);
				pg_context->source_texture_name = str8_copy(pg_context->source_texture_arena, params->file_path);
				pg_context->source_texture_dim = params->image_data->dim;
				pg_context->source_texture_format = image->format;
				pg_context->source_texture_is_user_provided = true;

				//~ Dedrick: Reset viewport state.
				pg_context->viewport_zoom = 0.0f;
				pg_context->viewport_center_pos = { 0.0f, 0.0f };
			}

			//~ Dedrick: Reset state.
			pg_context->image_load_params = nullptr;
			semaphore_release(params->load_completion_semaphore);
			arena_release(params->arena);
		}
	}

	//~ Dedrick: Unload user image.
	if (pg_context->source_texture_clear) {
		pg_context->source_texture_clear = false;
		if (pg_context->source_texture_srv != nullptr) {
			pg_context->source_texture_srv->Release();
			pg_context->source_texture_srv = nullptr;
		}
		if (pg_context->source_texture != nullptr) {
			pg_context->source_texture->Release();
			pg_context->source_texture = nullptr;
		}
		pg_context->source_texture_is_user_provided = false;
		pg_context->image_load_request_id += 1;

		//~ Dedrick: Clear error message state.
		pg_context->error_t = 0.0f;
	}

	//~ Dedrick: Built-in Image loading.
	if (pg_context->source_texture == nullptr) {
		DDS_Parsed const *dds = dds_parsed_from_bytes(scratch.arena, pg_default_image_file_bytes);
		DDS_SubresourceArray const subresources = dds_subresource_array_from_parsed(scratch.arena, dds);
		D3D11_SUBRESOURCE_DATA *initial_data = arena_push_array<D3D11_SUBRESOURCE_DATA>(scratch.arena, subresources.count);
		for (u64 idx = 0; idx < subresources.count; ++idx) {
			initial_data[idx].pSysMem = subresources[idx].bytes.data;
			initial_data[idx].SysMemPitch = subresources[idx].row_pitch;
			initial_data[idx].SysMemSlicePitch = subresources[idx].slice_pitch;
		}
		D3D11_TEXTURE2D_DESC desc = {};
		desc.Width = dds->header->width;
		desc.Height = dds->header->height;
		desc.MipLevels = subresources.mip_count;
		desc.ArraySize = subresources.array_size;
		desc.Format = dds->format;
		desc.SampleDesc.Count = 1;
		desc.Usage = D3D11_USAGE_IMMUTABLE;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		HRESULT const hr = d3d11_device()->CreateTexture2D(&desc, initial_data, &pg_context->source_texture);
		if (FAILED(hr)) {
			dt_show_dialog(pg_context->window, "Fatal Error"_str8, "Couldn't create texture for built-in source"_str8, true);
			abort_self(0);
		}
		d3d11_device()->CreateShaderResourceView(pg_context->source_texture, nullptr, &pg_context->source_texture_srv);

		//~ Dedrick: Fill source texture state.
		arena_clear(pg_context->source_texture_arena);
		pg_context->source_texture_dim = { dds->header->width, dds->header->height };
		pg_context->source_texture_format = dds->format;
		pg_context->source_texture_name = str8_copy(pg_context->source_texture_arena, "Artist Workshop (Built-in)"_str8);

		//~ Dedrick: Reset viewport state.
		pg_context->viewport_zoom = 0.0f;
		pg_context->viewport_center_pos = { 0.0f, 0.0f };
	}

	//~ Dedrick: Source texture resolution change -> Resize display buffers.
	{
		b8 const resolution_change = pg_context->display_buffer_dim.x != pg_context->source_texture_dim.x || pg_context->display_buffer_dim.y != pg_context->source_texture_dim.y;
		if (resolution_change) {
			pg_context->display_buffer_dim = pg_context->source_texture_dim;

			//~ Dedrick: Release render target resources if needed.
			if (pg_context->display_buffer_srv != nullptr) { pg_context->display_buffer_srv->Release(); }
			if (pg_context->display_buffer_rtv != nullptr) { pg_context->display_buffer_rtv->Release(); }
			if (pg_context->display_buffer != nullptr) { pg_context->display_buffer->Release(); }

			//~ Dedrick: Resize display buffer.
			D3D11_TEXTURE2D_DESC desc = {};
			{
				desc.Width = pg_context->display_buffer_dim.x;
				desc.Height = pg_context->display_buffer_dim.y;
				desc.MipLevels = 1;
				desc.ArraySize = 1;
				desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
				desc.SampleDesc.Count = 1;
				desc.Usage = D3D11_USAGE_DEFAULT;
				desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			}
			D3D11_RENDER_TARGET_VIEW_DESC rtv_desc = {};
			{
				rtv_desc.Format = desc.Format;
				rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			}
			D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
			{
				srv_desc.Format = desc.Format;
				srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
				srv_desc.Texture2D.MipLevels = static_cast<UINT>(-1);
			}
			d3d11_device()->CreateTexture2D(&desc, nullptr, &pg_context->display_buffer);
			d3d11_device()->CreateRenderTargetView(pg_context->display_buffer, &rtv_desc, &pg_context->display_buffer_rtv);
			d3d11_device()->CreateShaderResourceView(pg_context->display_buffer, &srv_desc, &pg_context->display_buffer_srv);

			//~ Dedrick: Release blur textures cache.
			for (PG_BlurTexture *node = pg_context->blur_texture_free_first; node != nullptr; node = node->next) {
				node->srv->Release();
				node->rtv->Release();
				node->texture->Release();
			}
			for (PG_BlurTexture *node = pg_context->blur_texture_first; node != nullptr; node = node->next) {
				node->srv->Release();
				node->rtv->Release();
				node->texture->Release();
			}
			pg_context->blur_texture_first = nullptr;
			pg_context->blur_texture_last = nullptr;
			pg_context->blur_texture_free_first = nullptr;
			pg_context->blur_texture_free_last = nullptr;
			arena_clear(pg_context->blur_texture_arena);
		}

		//~ Dedrick: Mark blur textures as unused.
		if (!resolution_change) {
			pg_blur_texture_cache_reset();
		}
	}

	//~ Dedrick: @ui Build UI.
	{
		ZoneScopedN("build ui");

		//~ Dedrick: Begin ImGui frame scope.
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplRgfw_NewFrame();
		ImGui::GetIO().DeltaTime = pg_context->frame_dt;
		ImGui::NewFrame();

		//~ Dedrick: Build dockspace.
		{
			ImGuiID const dockspace_id = ImGui::GetID("My Dockspace");
			ImGuiViewport *const viewport = ImGui::GetMainViewport();

			//~ Dedrick: First time dockspace layout.
			if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr) {
				ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
				ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);
				ImGuiID dock_id_main = dockspace_id;
				ImGuiID dock_id_bottom = 0;
				f32 const text_height = (ImGui::GetStyle().WindowPadding.y * 2.0f + ImGui::GetFontSize()) / viewport->Size.y;
				ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Down, text_height, &dock_id_bottom, &dock_id_main);
				ImGuiID dock_id_top_left = 0;
				ImGuiID dock_id_top_right = 0;
				ImGui::DockBuilderSplitNode(dock_id_main, ImGuiDir_Left, 0.35f, &dock_id_top_left, &dock_id_top_right);
				ImGui::DockBuilderDockWindow("Controls", dock_id_top_left);
				ImGui::DockBuilderDockWindow("Viewport", dock_id_top_right);
				ImGui::DockBuilderDockWindow("BottomBar", dock_id_bottom);

				//~ Dedrick: Set bottom bar flags.
				ImGuiDockNode *bottom_node = ImGui::DockBuilderGetNode(dock_id_bottom);
				ImGuiDockNodeFlags bottom_node_flags = ImGuiDockNodeFlags_NoResize;
				bottom_node_flags |= static_cast<ImGuiDockNodeFlags>(ImGuiDockNodeFlags_NoTabBar);
				bottom_node_flags |= static_cast<ImGuiDockNodeFlags>(ImGuiDockNodeFlags_NoDockingOverMe);
				bottom_node->LocalFlags |= bottom_node_flags;

				//~ Dedrick: Finalize layout.
				ImGui::DockBuilderFinish(dockspace_id);
			}

			//~ Dedrick: Create dockspace from layout.
			ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
			dockspace_flags	|= static_cast<ImGuiDockNodeFlags>(ImGuiDockNodeFlags_NoTabBar);
			ImGui::DockSpaceOverViewport(dockspace_id, viewport, dockspace_flags);
		}

		//~ Dedrick: Build controls.
		if (ImGui::Begin("Controls")) {
			if (ImGui::CollapsingHeader("Image", ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::PushID("Image");
				b8 const benchmark_running = pg_context->benchmark.state == PG_BenchmarkState_Running;
				if (benchmark_running) {
					ImGui::PushDisabled();
				}

				//~ Dedrick: Source image selection.
				{
					ImGuiStyle const &style = ImGui::GetStyle();
					char const *open_label = "Open";
					char const *clear_label = "X";
					ImGui::Text("Source");
					ImGui::SameLine();
					f32 const open_button_width = ImGui::CalcTextSize(open_label).x + style.FramePadding.x * 2.0f;
					f32 total_button_width = open_button_width + style.ItemSpacing.x;
					if (pg_context->source_texture_is_user_provided) {
						f32 const clear_button_width = ImGui::CalcTextSize(clear_label).x + style.FramePadding.x * 2.0f;
						total_button_width += clear_button_width + style.ItemSpacing.x;
					}
					ImGui::SetNextItemWidth(-total_button_width);
					ImGui::PushDisabled();
					ImGui::InputText("##path", reinterpret_cast<char *>(const_cast<u8 *>(pg_context->source_texture_name.data)), pg_context->source_texture_name.size + 1, ImGuiInputTextFlags_ElideLeft);
					ImGui::PopDisabled();
					ImGui::SameLine();
					if (ImGui::Button(open_label)) {
						DT_FileDialogFilter const filters[] = {
							{ "Supported Files"_str8, "png,jpg,jpeg,exr,hdr"_str8 },
							{ "PNG"_str8, "png"_str8 },
							{ "JPEG"_str8, "jpg,jpeg"_str8 },
							{ "HDR"_str8, "hdr"_str8 },
							{ "EXR"_str8, "exr"_str8 },
						};
						String8 const path = dt_file_dialog_pick_file(scratch.arena, pg_context->window, filters, array_count(filters));
						if (path.size > 0) {
							arena_clear(pg_context->image_load_request_arena);
							pg_context->image_load_request_path = str8_copy(pg_context->image_load_request_arena, path);
							pg_context->image_load_request_id += 1;
						}
					}
					if (pg_context->source_texture_is_user_provided) {
						ImGui::SameLine();
						if (ImGui::Button(clear_label)) {
							pg_context->source_texture_clear = true;
						}
					}
				}

				if (benchmark_running) {
					ImGui::PopDisabled();
				}

				ImGui::PopID();
			}
			if (ImGui::CollapsingHeader("Blur Options", ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::PushID("Blur Options");
				b8 const benchmark_running = pg_context->benchmark.state == PG_BenchmarkState_Running;
				if (benchmark_running) {
					ImGui::PushDisabled();
				}
				ImGui::Text("Blur Kind");
				for (PG_BlurKind k = static_cast<PG_BlurKind>(0); k < PG_BlurKind_COUNT; k = static_cast<PG_BlurKind>(k + 1)) {
					char const *label = reinterpret_cast<char const *>(pg_blur_kind_name_table[k].data);
					if (ImGui::RadioButton(label, pg_context->blur_kind == k)) {
						pg_context->blur_kind = k;
						pg_context->selected_display_entry_idx = 0;
					}
				}
				ImGui::TextUnformatted("Blur Radius");
				ImGui::SameLine();
				ImGui::SetNextItemWidth(-FLT_MIN);
				ImGui::SliderInt("##Blur Radius", &pg_context->blur_radius, 0, PG_BlurRadiusMax, "%d px");
				if (benchmark_running) {
					ImGui::PopDisabled();
				}
				ImGui::PopID();
			}
			if (ImGui::CollapsingHeader("Benchmark", ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::PushID("Benchmark");
				PG_Benchmark *const benchmark = &pg_context->benchmark;
				b8 const benchmark_running = benchmark->state == PG_BenchmarkState_Running;

				//~ Dedrick: Start/Cancel.
				if (benchmark_running) {
					if (ImGui::Button("Cancel", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
						pg_context->benchmark_cancel_requested = true;
					}
				}
				else {
					if (ImGui::Button("Start", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
						pg_context->benchmark_start_requested = true;
					}
				}

				if (benchmark->state == PG_BenchmarkState_Completed) {
					//~ Dedrick: Benchmark graph.
					if (ImGui::Button("Show Graph", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
						pg_context->benchmark_graph_open = true;
					}

					//~ Dedrick: CSV export.
					if (ImGui::Button("Export .csv", ImVec2(ImGui::GetContentRegionAvail().x, 0.0f))) {
						DT_FileDialogFilter const filters[] = { { "CSV"_str8, "csv"_str8 } };
						String8 const default_name = str8f(scratch.arena, "blur_benchmark_%ux%u.csv", pg_context->source_texture_dim.x, pg_context->source_texture_dim.y);
						String8 const path = dt_file_dialog_save(scratch.arena, pg_context->window, default_name, filters, array_count(filters), nullptr);
						if (path.size > 0) {
							//~ Dedrick: Collect the profiled blurs.
							PG_BlurKind columns[PG_BlurKind_COUNT] = {};
							u64 column_count = 0;
							for (PG_BlurKind kind = static_cast<PG_BlurKind>(0); kind < PG_BlurKind_COUNT; kind = static_cast<PG_BlurKind>(kind + 1)) {
								if (benchmark->point_ms[kind] != nullptr) {
									columns[column_count++] = kind;
								}
							}

							//~ Dedrick: Serialize; one column per profiled blur, one row per profiled radius.
							String8List rows = {};
							{
								String8List header = {};
								str8_list_push(scratch.arena, &header, "blur_radius_px"_str8);
								for (u64 column = 0; column < column_count; ++column) {
									str8_list_push(scratch.arena, &header, pg_blur_kind_name_table[columns[column]]);
								}
								String8JoinParams const join_params = { .separator = ","_str8 };
								str8_list_push(scratch.arena, &rows, str8f(scratch.arena, "%.*s\n", DK_STR8_VARG(str8_list_join(scratch.arena, &header, &join_params))));
							}
							for (u64 radius_idx = 0; radius_idx < benchmark->radius_count; ++radius_idx) {
								String8List row = {};
								str8_list_push(scratch.arena, &row, str8f(scratch.arena, "%d", benchmark->radii[radius_idx]));
								for (u64 column = 0; column < column_count; ++column) {
									str8_list_push(scratch.arena, &row, str8f(scratch.arena, "%.4f", benchmark->point_ms[columns[column]][radius_idx]));
								}
								String8JoinParams const join_params = { .separator = ","_str8 };
								str8_list_push(scratch.arena, &rows, str8f(scratch.arena, "%.*s\n", DK_STR8_VARG(str8_list_join(scratch.arena, &row, &join_params))));
							}
							if (!write_bytes_list_to_file_path(path, &rows)) {
								DK_LOG_ERRORF("ERROR: failed to write benchmark csv: %.*s\n", DK_STR8_VARG(path));
							}
						}
					}
				}

				//~ Dedrick: Benchmark blur selection.
				{
					if (benchmark_running) {
						ImGui::PushDisabled();
					}
					for (PG_BlurKind k = static_cast<PG_BlurKind>(0); k < PG_BlurKind_COUNT; k = static_cast<PG_BlurKind>(k + 1)) {
						char const *label = reinterpret_cast<char const *>(pg_blur_kind_name_table[k].data);
						ImGui::CheckboxFlags(label, &pg_context->benchmark_blur_kind_flags, 1u << k);
					}
					if (benchmark_running) {
						ImGui::PopDisabled();
					}
				}

				//~ Dedrick: Benchmark progress.
				if (benchmark->state == PG_BenchmarkState_Running) {
					ImGui::Separator();
					ImGui::TextUnformatted(reinterpret_cast<char const *>(pg_blur_kind_name_table[benchmark->kind_order[benchmark->kind_idx]].data));
					ImGui::Text("Radius: %d px", benchmark->radii[benchmark->radius_idx]);
					if (benchmark->sweep == 0) {
						ImGui::TextUnformatted("Warm-up sweep");
					}
					else {
						ImGui::Text("Sweep %u/%u", benchmark->sweep, PG_BenchmarkSweepCount - 1);
					}
				}

				ImGui::PopID();
			}
		}
		ImGui::End();

		//~ Dedrick: Build image viewer.
		if (ImGui::Begin("Viewport")) {
			ImGui::PushID("Viewport");

			//~ Dedrick: Viewport fit scale (pillar/letter box).
			f32 const toolbar_height = ImGui::GetFrameHeightWithSpacing();
			ImVec2 const viewer_size = ImGui::GetContentRegionAvail() - ImVec2(0.0f, toolbar_height);
			f32 const viewport_fit_scale = min(viewer_size.x / pg_context->display_buffer_dim.x, viewer_size.y / pg_context->display_buffer_dim.y);
			f32 const viewport_zoom = epsilon_equal(pg_context->viewport_zoom, 0.0f, 1e-6f) ? viewport_fit_scale : pg_context->viewport_zoom;

			//~ Dedrick: Toolbar pass selector.
			{
				b8 const benchmark_running = pg_context->benchmark.state == PG_BenchmarkState_Running;
				ImGui::BeginChild(
					"Toolbar",
					ImVec2(0, toolbar_height),
					ImGuiChildFlags_None,
					ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
				);

				if (benchmark_running) {
					ImGui::PushDisabled();
				}

				//~ Dedrick: Zoom level.
				{
					f32 zoom_pct_edit = viewport_zoom * 100.0f;
					ImGui::SetNextItemWidth(100.0f);
					if (ImGui::InputFloat("##zoom", &zoom_pct_edit, 10.0f, 100.0f, "%.0f%%")) {
						pg_context->viewport_zoom = clamp(zoom_pct_edit, 0.0f, PG_ViewportZoomMax * 100.0f) / 100.0f;
					}
				}

				//~ Dedrick: Display pass.
				if (pg_context->display_entries.count > 0) {
					ImGui::SameLine();
					PG_DisplayEntryArray const *display_entries = &pg_context->display_entries;
					u64 const selected_entry_idx = pg_display_selected_entry_idx(display_entries);
					PG_DisplayEntry const *selected_entry = &display_entries->v[selected_entry_idx];
					ImGui::PushItemWidth(300.0f);
					if (ImGui::BeginCombo("##display", reinterpret_cast<char const *>(selected_entry->label.data))) {
						for (u64 entry_idx = 0; entry_idx < display_entries->count; ++entry_idx) {
							b8 const is_selected = entry_idx == selected_entry_idx;
							char const *label = reinterpret_cast<char const *>(display_entries->v[entry_idx].label.data);
							if (ImGui::Selectable(label, is_selected)) {
								pg_context->selected_display_entry_idx = (entry_idx + 1 == display_entries->count) ? 0 : entry_idx + 1;
							}
							if (is_selected) {
								ImGui::SetItemDefaultFocus();
							}
						}
						ImGui::EndCombo();
					}
					ImGui::PopItemWidth();
				}

				if (benchmark_running) {
					ImGui::PopDisabled();
				}

				ImGui::EndChild();
			}

			//~ Dedrick: Image viewer.
			{
				ImGui::BeginChild("ImageViewer", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar);
				ImGuiIO const &io = ImGui::GetIO();
				ImVec2 const canvas_min = ImGui::GetCursorScreenPos();
				ImVec2 const canvas_size = ImGui::GetContentRegionAvail();
				vec2 const canvas_center = { canvas_min.x + canvas_size.x * 0.5f, canvas_min.y + canvas_size.y * 0.5f };

				//~ Dedrick: Wheel -> zoom at mouse.
				if (ImGui::IsWindowHovered() && io.MouseWheel != 0.0f) {
					f32 const zoom_new = clamp(viewport_zoom + viewport_zoom * io.MouseWheel / 10.0f, PG_ViewportZoomMin, PG_ViewportZoomMax);
					vec2 const mouse_scrn = vec2{ io.MousePos.x, io.MousePos.y } - canvas_center;
					vec2 const mouse_canvas = mouse_scrn / viewport_zoom + pg_context->viewport_center_pos;
					vec2 const mouse_scrn_post = (mouse_canvas - pg_context->viewport_center_pos) * zoom_new;
					pg_context->viewport_center_pos = pg_context->viewport_center_pos + (mouse_scrn_post - mouse_scrn) / zoom_new;
					pg_context->viewport_zoom = zoom_new;
				}

				//~ Dedrick: Left-drag -> pan.
				if (ImGui::IsWindowHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
					pg_context->viewport_center_pos.x -= io.MouseDelta.x / viewport_zoom;
					pg_context->viewport_center_pos.y -= io.MouseDelta.y / viewport_zoom;
				}

				//~ Dedrick: Double-click -> reset view.
				if (ImGui::IsWindowHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
					pg_context->viewport_zoom = 0.0f;
					pg_context->viewport_center_pos = { 0.0f, 0.0f };
				}

				//~ Dedrick: Image -> canvas.
				{
					vec2 const img_dim = vec2{ static_cast<f32>(pg_context->display_buffer_dim.x), static_cast<f32>(pg_context->display_buffer_dim.y) };
					vec2 const img_half = img_dim * (0.5f * viewport_zoom);
					vec2 const img_min = canvas_center - pg_context->viewport_center_pos * viewport_zoom - img_half;
					vec2 const img_max = img_min + img_half * 2.0f;
					ImGui::GetWindowDrawList()->AddImage(
						reinterpret_cast<ImTextureID>(pg_context->display_buffer_srv),
						ImVec2{ img_min.x, img_min.y }, ImVec2{ img_max.x, img_max.y }
					);
				}
				ImGui::EndChild();
			}
			ImGui::PopID();
		}
		ImGui::End();

		//~ Dedrick: Build bottom bar.
		{
			b8 const has_error_msg = pg_context->error_t > 0.0f && pg_context->error_string_size > 0;
			ImVec4 const bar_color = has_error_msg ? ImVec4{ 0.7f, 0.1f, 0.1f, 1.0f } : ImGui::GetStyleColorVec4(ImGuiCol_MenuBarBg);
			ImGui::PushStyleColor(ImGuiCol_WindowBg, bar_color);
			if (ImGui::Begin("BottomBar", nullptr, ImGuiWindowFlags_NoScrollbar)) {
				String8 status_str = {};

				//~ Dedrick: Pick status string.
				if (has_error_msg) {
					pg_context->error_t -= pg_context->frame_dt / 8.0f;
					status_str = str8(pg_context->error_buffer, pg_context->error_string_size);
				}
				else if (pg_context->image_load_params != nullptr) {
					status_str = str8f(scratch.arena, "Loading image: %.*s", DK_STR8_VARG(pg_context->image_load_params->file_path));
				}
				else if (pg_context->benchmark.state == PG_BenchmarkState_Running) {
					PG_Benchmark *const benchmark = &pg_context->benchmark;
					status_str = str8f(scratch.arena, "Benchmarking %.*s, radius %u px", DK_STR8_VARG(pg_blur_kind_name_table[benchmark->kind_order[benchmark->kind_idx]]), benchmark->radii[benchmark->radius_idx]);
				}
				else {
					status_str = "Ready"_str8;
				}

				//~ Dedrick: Show status string.
				ImGui::TextUnformatted(
					reinterpret_cast<char const *>(status_str.data),
					reinterpret_cast<char const *>(status_str.data + status_str.size)
				);

				//~ Dedrick: Show source texture info, if the status string left room for it.
				{
					String8 const info_str = str8f(scratch.arena, "%ux%u %.*s", pg_context->source_texture_dim.x, pg_context->source_texture_dim.y, DK_STR8_VARG(d3d11_format_name_table[pg_context->source_texture_format]));
					ImGuiStyle const &style = ImGui::GetStyle();
					f32 const window_width = ImGui::GetWindowWidth();
					f32 const status_width = ImGui::CalcTextSize(
						reinterpret_cast<char const *>(status_str.data),
						reinterpret_cast<char const *>(status_str.data + status_str.size)
					).x;
					f32 const info_width = ImGui::CalcTextSize(
						reinterpret_cast<char const *>(info_str.data),
						reinterpret_cast<char const *>(info_str.data + info_str.size)
					).x;
					b8 const show_info_str = status_width + info_width <= window_width - style.WindowPadding.x * 2.0f;
					if (show_info_str) {
						ImGui::SameLine(window_width - info_width - style.WindowPadding.x);
						ImGui::TextUnformatted(
							reinterpret_cast<char const *>(info_str.data),
							reinterpret_cast<char const *>(info_str.data + info_str.size)
						);
					}
				}
			}
			ImGui::End();
			ImGui::PopStyleColor();
		}

		//~ Dedrick: Build benchmark graph window.
		if (pg_context->benchmark_graph_open) {
			ImGui::SetNextWindowPos(ImVec2{ 50.0f, 50.0f }, ImGuiCond_Once);
			ImGui::SetNextWindowSizeConstraints(ImVec2{ 800.0f, 560.0f }, ImVec2{ FLT_MAX, FLT_MAX });
			ImGui::SetNextWindowSize(ImVec2{ 800.0f, 560.0f }, ImGuiCond_Once);
			if (ImGui::Begin("Benchmark Graph", &pg_context->benchmark_graph_open, ImGuiWindowFlags_NoDocking)) {
				String8 const title = str8f(scratch.arena, "Blur Timings (%ux%u)", pg_context->source_texture_dim.x, pg_context->source_texture_dim.y);
				ImGui::Text("%.*s", DK_STR8_VARG(title));
				ImDrawList *const draw_list = ImGui::GetWindowDrawList();

				//~ Dedrick: Plot region.
				ImVec2 const region = ImGui::GetContentRegionAvail();
				ImVec2 const plot_min = ImGui::GetCursorScreenPos();
				ImGui::Dummy(region);
				ImVec2 const plot_lo = { plot_min.x + 64.0f, plot_min.y + 26.0f };
				ImVec2 const plot_hi = { plot_min.x + region.x - 24.0f, plot_min.y + region.y - 76.0f };

				//~ Dedrick: Collect the profiled blurs.
				struct PG_GraphSeries {
					PG_BlurKind kind;
					f32 *ms;
				};
				PG_Benchmark *const benchmark = &pg_context->benchmark;
				PG_GraphSeries series[PG_BlurKind_COUNT] = {};
				u64 series_count = 0;
				f32 min_ms = FLT_MAX;
				f32 max_ms = 0.0f;
				for (PG_BlurKind kind = static_cast<PG_BlurKind>(0); kind < PG_BlurKind_COUNT; kind = static_cast<PG_BlurKind>(kind + 1)) {
					f32 *const ms = benchmark->point_ms[kind];
					if (ms == nullptr) {
						continue;
					}
					for (u64 radius_idx = 0; radius_idx < benchmark->radius_count; ++radius_idx) {
						min_ms = min(min_ms, ms[radius_idx]);
						max_ms = max(max_ms, ms[radius_idx]);
					}
					series[series_count].kind = kind;
					series[series_count].ms = ms;
					series_count += 1;
				}

				//~ Dedrick: Log-scale axis domain.
				f32 const low = (min_ms >= 0.05f) ? 0.05f : pow(10.0f, floor(log10(min_ms / 1.2f)));
				f32 const high = max(1.0f, max_ms * 1.6f);
				f32 const x_span_log = log_e(static_cast<f32>(benchmark->radii[benchmark->radius_count - 1]) / static_cast<f32>(benchmark->radii[0]));
				f32 const y_span_log = log_e(high / low);
				f32 const x_px_per_log = (plot_hi.x - plot_lo.x) / x_span_log;
				f32 const y_px_per_log = (plot_hi.y - plot_lo.y) / y_span_log;
				ImU32 const text_u32 = ImGui::GetColorU32(ImGuiCol_Text);
				ImU32 const grid_row_u32 = ImGui::GetColorU32(ImGuiCol_Text, 0.2f);
				ImU32 const grid_column_u32 = ImGui::GetColorU32(ImGuiCol_Text, 0.12f);

				//~ Dedrick: Horizontal gridlines & Y tick labels, at 1/2/5 per decade.
				f32 const tick_multipliers[3] = { 1.0f, 2.0f, 5.0f };
				for (s32 e = static_cast<s32>(floor(log10(low))); e <= static_cast<s32>(ceil(log10(high))); ++e) {
					for (u64 m = 0; m < array_count(tick_multipliers); ++m) {
						f32 const tick = tick_multipliers[m] * pow(10.0f, static_cast<f32>(e));
						if (tick < low || tick > high) {
							continue;
						}
						f32 const py = plot_hi.y - log_e(tick / low) * y_px_per_log;
						draw_list->AddLine(ImVec2{ plot_lo.x, py }, ImVec2{ plot_hi.x, py }, grid_row_u32);
						String8 const label = str8f(scratch.arena, "%.4g", tick);
						ImVec2 const label_dim = ImGui::CalcTextSize(reinterpret_cast<char const *>(label.data), reinterpret_cast<char const *>(label.data + label.size));
						draw_list->AddText(ImVec2{ plot_lo.x - 12.0f - label_dim.x, py - label_dim.y * 0.5f }, text_u32, reinterpret_cast<char const *>(label.data), reinterpret_cast<char const *>(label.data + label.size));
					}
				}

				//~ Dedrick: Radius x-positions on the log axis.
				f32 *const radius_px = arena_push_array<f32>(scratch.arena, benchmark->radius_count);
				for (u64 radius_idx = 0; radius_idx < benchmark->radius_count; ++radius_idx) {
					radius_px[radius_idx] = plot_lo.x + log_e(static_cast<f32>(benchmark->radii[radius_idx]) / static_cast<f32>(benchmark->radii[0])) * x_px_per_log;
				}

				//~ Dedrick: Vertical gridlines & radius tick labels.
				for (u64 radius_idx = 0; radius_idx < benchmark->radius_count; ++radius_idx) {
					f32 const px = radius_px[radius_idx];
					draw_list->AddLine(ImVec2{ px, plot_lo.y }, ImVec2{ px, plot_hi.y }, grid_column_u32);
					String8 const label = str8f(scratch.arena, "%d", benchmark->radii[radius_idx]);
					ImVec2 const label_dim = ImGui::CalcTextSize(reinterpret_cast<char const *>(label.data), reinterpret_cast<char const *>(label.data + label.size));
					f32 const anchor_offset = (radius_idx == 0) ? 0.0f : label_dim.x * 0.5f;
					draw_list->AddText(ImVec2{ px - anchor_offset, plot_hi.y + 10.0f }, text_u32, reinterpret_cast<char const *>(label.data), reinterpret_cast<char const *>(label.data + label.size));
				}

				//~ Dedrick: Axis captions.
				{
					String8 const caption = str8f(scratch.arena, "Blur Radius (pixels)");
					ImVec2 const caption_dim = ImGui::CalcTextSize(reinterpret_cast<char const *>(caption.data), reinterpret_cast<char const *>(caption.data + caption.size));
					draw_list->AddText(ImVec2{ plot_lo.x + (plot_hi.x - plot_lo.x) * 0.5f - caption_dim.x * 0.5f, plot_hi.y + 28.0f }, text_u32, reinterpret_cast<char const *>(caption.data), reinterpret_cast<char const *>(caption.data + caption.size));
					draw_list->AddText(ImVec2{ plot_min.x + 4.0f, plot_min.y + 4.0f }, text_u32, "Time (ms)");
				}

				//~ Dedrick: Series polylines.
				for (u64 series_idx = 0; series_idx < series_count; ++series_idx) {
					ImU32 const color_u32 = static_cast<ImU32>(pg_blur_kind_color_table[series[series_idx].kind]);
					f32 *const ms = series[series_idx].ms;
					f32 px_previous = radius_px[0];
					f32 py_previous = plot_hi.y - log_e(ms[0] / low) * y_px_per_log;
					for (u64 radius_idx = 1; radius_idx < benchmark->radius_count; ++radius_idx) {
						f32 const py = plot_hi.y - log_e(ms[radius_idx] / low) * y_px_per_log;
						draw_list->AddLine(ImVec2{ px_previous, py_previous }, ImVec2{ radius_px[radius_idx], py }, color_u32, 2.5f);
						px_previous = radius_px[radius_idx];
						py_previous = py;
					}
				}

				//~ Dedrick: Point dots.
				for (u64 series_idx = 0; series_idx < series_count; ++series_idx) {
					ImU32 const color_u32 = static_cast<ImU32>(pg_blur_kind_color_table[series[series_idx].kind]);
					f32 *const ms = series[series_idx].ms;
					for (u64 radius_idx = 0; radius_idx < benchmark->radius_count; ++radius_idx) {
						f32 const py = plot_hi.y - log_e(ms[radius_idx] / low) * y_px_per_log;
						draw_list->AddCircleFilled(ImVec2{ radius_px[radius_idx], py }, 4.0f, color_u32);
					}
				}

				//~ Dedrick: Value labels, nudged apart vertically within each radius column.
				struct PG_GraphLabel {
					f32 y;
					u64 series_idx;
				};
				for (u64 radius_idx = 0; radius_idx < benchmark->radius_count; ++radius_idx) {
					f32 const px = radius_px[radius_idx];
					PG_GraphLabel labels[PG_BlurKind_COUNT] = {};
					u64 label_count = 0;
					for (u64 series_idx = 0; series_idx < series_count; ++series_idx) {
						labels[label_count].series_idx = series_idx;
						labels[label_count].y = plot_hi.y - log_e(series[series_idx].ms[radius_idx] / low) * y_px_per_log - ImGui::GetFontSize() - 10.0f;
						label_count += 1;
					}
					insertion_sort(labels, label_count, [](PG_GraphLabel const &a, PG_GraphLabel const &b) { return a.y < b.y; });
					for (u64 label_idx = 1; label_idx < label_count; ++label_idx) {
						labels[label_idx].y = max(labels[label_idx].y, labels[label_idx - 1].y + 16.0f);
					}
					f32 const overflow = max(0.0f, labels[label_count - 1].y - (plot_hi.y - 5.0f));
					for (u64 label_idx = 0; label_idx < label_count; ++label_idx) {
						labels[label_idx].y -= overflow;
					}
					for (u64 label_idx = 0; label_idx < label_count; ++label_idx) {
						PG_GraphLabel const *label = &labels[label_idx];
						ImU32 const color_u32 = static_cast<ImU32>(pg_blur_kind_color_table[series[label->series_idx].kind]);
						String8 const text = str8f(scratch.arena, "%.2f", series[label->series_idx].ms[radius_idx]);
						ImVec2 const text_dim = ImGui::CalcTextSize(reinterpret_cast<char const *>(text.data), reinterpret_cast<char const *>(text.data + text.size));
						draw_list->AddText(ImVec2{ px - text_dim.x * 0.5f, label->y }, color_u32, reinterpret_cast<char const *>(text.data), reinterpret_cast<char const *>(text.data + text.size));
					}
				}

				//~ Dedrick: Legend rows wrap at the plot's right edge, each row centered.
				{
					f32 const avail = plot_hi.x - plot_lo.x;
					f32 const line_height = ImGui::GetTextLineHeight();
					f32 widths[PG_BlurKind_COUNT] = {}; // 38 px swatch + name + 28 px gap per entry
					for (u64 series_idx = 0; series_idx < series_count; ++series_idx) {
						String8 const name = pg_blur_kind_name_table[series[series_idx].kind];
						widths[series_idx] = 38.0f + ImGui::CalcTextSize(reinterpret_cast<char const *>(name.data), reinterpret_cast<char const *>(name.data + name.size)).x + 28.0f;
					}
					f32 legend_y = plot_hi.y + 48.0f;
					u64 row_begin = 0;
					while (row_begin < series_count) {
						//~ Dedrick: Take the longest run of entries that fits the plot width.
						f32 row_width = 0.0f;
						u64 row_count = 0;
						while (row_begin + row_count < series_count) {
							if (row_count != 0 && row_width + widths[row_begin + row_count] > avail) {
								break;
							}
							row_width += widths[row_begin + row_count];
							row_count += 1;
						}
						f32 legend_x = plot_lo.x + (avail - row_width) * 0.5f;
						for (u64 row_idx = 0; row_idx < row_count; ++row_idx) {
							u64 const series_idx = row_begin + row_idx;
							ImU32 const color_u32 = static_cast<ImU32>(pg_blur_kind_color_table[series[series_idx].kind]);
							String8 const name = pg_blur_kind_name_table[series[series_idx].kind];
							draw_list->AddLine(ImVec2{ legend_x, legend_y + line_height * 0.5f }, ImVec2{ legend_x + 28.0f, legend_y + line_height * 0.5f }, color_u32, 2.5f);
							draw_list->AddText(ImVec2{ legend_x + 38.0f, legend_y }, text_u32, reinterpret_cast<char const *>(name.data), reinterpret_cast<char const *>(name.data + name.size));
							legend_x += widths[series_idx];
						}
						legend_y += line_height + 4.0f;
						row_begin += row_count;
					}
				}
			}
			ImGui::End();
		}

		//~ Dedrick: Build UI draw list.
		ImGui::Render();
	}

	//~ Dedrick: Start async file load.
	if (pg_context->benchmark.state != PG_BenchmarkState_Running &&
	    pg_context->image_load_request_path.size > 0 &&
	    pg_context->image_load_params == nullptr) {
		Arena *arena = arena_alloc();
		PG_ImageLoadParams *params = arena_push<PG_ImageLoadParams>(arena);
		params->arena = arena;
		params->file_path = str8_copy(arena, pg_context->image_load_request_path);
		params->load_completion_semaphore = semaphore_alloc(0, 1, "pg_image_load_completion"_str8);
		params->request_id = pg_context->image_load_request_id;
		pg_context->image_load_params = params;
		pg_context->image_load_thread = thread_launch(pg_image_load_thread_entry_point, params);

		//~ Dedrick: Clear image load state.
		memory_zero(&pg_context->image_load_request_path);
		arena_clear(pg_context->image_load_request_arena);

		//~ Dedrick: Clear error message state.
		pg_context->error_t = 0.0f;
	}

	//~ Dedrick: Reset blur textures.
	pg_blur_texture_cache_reset();

	//~ Dedrick: Blur passes & Rendering.
	{
		ID3D11DeviceContext1 *const d_ctx = d3d11_device_context();

		//~ Dedrick: Cancel benchmark.
		if (pg_context->benchmark_cancel_requested) {
			pg_context->benchmark_cancel_requested = false;
			memory_zero(&pg_context->benchmark);
			arena_clear(pg_context->benchmark_arena);
			pg_context->benchmark_graph_open = false;
		}

		//~ Dedrick: Start benchmark.
		if (pg_context->benchmark_start_requested) {
			pg_context->benchmark_start_requested = false;

			//~ Dedrick: Start the run, or log why not.
			if (pg_context->image_load_params != nullptr) {
				DK_LOG_ERRORF("ERROR: can't start benchmark while an image is loading\n");
			}
			else if (pg_context->benchmark_blur_kind_flags == 0) {
				DK_LOG_ERRORF("ERROR: can't start benchmark with no blurs selected\n");
			}
			else {
				memory_zero(&pg_context->benchmark);
				arena_clear(pg_context->benchmark_arena);
				pg_context->benchmark_graph_open = false;
				PG_Benchmark *const benchmark = &pg_context->benchmark;

				//~ Dedrick: Fill kind order with selected blurs.
				u64 kind_idx = 0;
				for (PG_BlurKind kind = static_cast<PG_BlurKind>(0); kind < PG_BlurKind_COUNT; kind = static_cast<PG_BlurKind>(kind + 1)) {
					if ((pg_context->benchmark_blur_kind_flags & (1u << kind)) != 0) {
						benchmark->kind_order[kind_idx++] = kind;
					}
				}
				benchmark->kind_count = kind_idx;

				//~ Dedrick: Fill geometric radius to profiling points.
				pg_benchmark_fill_radii(pg_context->benchmark_arena, benchmark, PG_BlurRadiusMax);

				//~ Dedrick: Begin benchmarking state.
				benchmark->state = PG_BenchmarkState_Running;
			}
		}

		//~ Dedrick: Display entries -> push source texture.
		PG_DisplayEntryList display_entry_list = {};
		pg_display_entry_list_push(pg_frame_arena(), &display_entry_list, "Source"_str8, pg_context->source_texture_dim, pg_context->source_texture_srv);

		//~ Dedrick: Benchmark blurs or run selected blur only.
		if (pg_context->benchmark.state == PG_BenchmarkState_Running) {
			ZoneScopedN("benchmark blur passes");
			PG_Benchmark *benchmark = &pg_context->benchmark;
			PG_BlurKind const blur_kind = benchmark->kind_order[benchmark->kind_idx];
			s32 const blur_radius = benchmark->radii[benchmark->radius_idx];

			//~ Dedrick: Flush the device command queue before profiling.
			{
				BOOL completed = FALSE;
				d_ctx->End(pg_context->benchmark_event_query);
				while (true) {
					if (d_ctx->GetData(pg_context->benchmark_event_query, &completed, sizeof(completed), 0) == S_OK) {
						break;
					}
				}
			}

			//~ Dedrick: Get iteration count, aim for 30ms batches.
			u32 const iterations = max(1u, min(128u, static_cast<u32>(ceil(30.0f / max(benchmark->batch_iteration_ms[blur_kind], 0.01f)))));

			//~ Dedrick: Begin profiling.
			d_ctx->Begin(pg_context->benchmark_disjoint_query);
			d_ctx->End(pg_context->benchmark_start_query);

			//~ Dedrick: Run blur passes.
			{
				TempArena const scratch2 = scratch_begin(nullptr, 0);
				dk_defer(scratch_end(scratch2));
				for (u32 idx = 0; idx < iterations; ++idx) {
					PG_BlurParams params = {};
					params.blur_kind = blur_kind;
					params.blur_radius = blur_radius;
					params.src_srv = pg_context->source_texture_srv;
					params.src_dim = pg_context->source_texture_dim;
					pg_blur(scratch2.arena, &params);

					//~ Dedrick: Recycle blur textures.
					pg_blur_texture_cache_reset();
				}
			}

			//~ Dedrick: End profiling.
			d_ctx->End(pg_context->benchmark_end_query);
			d_ctx->End(pg_context->benchmark_disjoint_query);
			{
				BOOL completed = FALSE;
				d_ctx->End(pg_context->benchmark_event_query);
				while (true) {
					if (d_ctx->GetData(pg_context->benchmark_event_query, &completed, sizeof(completed), 0) == S_OK) {
						break;
					}
				}
			}

			//~ Dedrick: Read the GPU time.
			{
				D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint = {};
				u64 start_ticks = 0;
				u64 end_ticks = 0;
				d_ctx->GetData(pg_context->benchmark_disjoint_query, &disjoint, sizeof(disjoint), 0);
				d_ctx->GetData(pg_context->benchmark_start_query, &start_ticks, sizeof(start_ticks), 0);
				d_ctx->GetData(pg_context->benchmark_end_query, &end_ticks, sizeof(end_ticks), 0);
				if (disjoint.Disjoint == FALSE) {
					f32 const avg_ms = static_cast<f32>(static_cast<f64>(end_ticks - start_ticks) * 1000.0 / static_cast<f64>(disjoint.Frequency)) / static_cast<f32>(iterations);
					benchmark->batch_iteration_ms[blur_kind] = avg_ms;

					// NOTE(Dedrick): Discard warmup sweep results.
					if (benchmark->sweep > 0) {
						PG_BenchmarkSample sample = {};
						sample.ms = avg_ms;
						pg_benchmark_sample_chunk_list_push(pg_context->benchmark_arena, &benchmark->samples[blur_kind], 256, &sample);
					}
				}
			}

			//~ Dedrick: Advance to the next point.
			benchmark->kind_idx += 1;
			if (benchmark->kind_idx == benchmark->kind_count) {
				benchmark->kind_idx = 0;
				benchmark->radius_idx += 1;
				if (benchmark->radius_idx == benchmark->radius_count) {
					benchmark->radius_idx = 0;
					benchmark->sweep += 1;
					// NOTE(Dedrick): Rotate kind order to spread clock/thermal drift across kinds.
					PG_BlurKind const first = benchmark->kind_order[0];
					for (u32 idx = 0; idx < benchmark->kind_count - 1; ++idx) {
						benchmark->kind_order[idx] = benchmark->kind_order[idx + 1];
					}
					benchmark->kind_order[benchmark->kind_count - 1] = first;
					if (benchmark->sweep == PG_BenchmarkSweepCount) {
						benchmark->state = PG_BenchmarkState_Completed;

						//~ Dedrick: Reduce the samples to a per-point minimum of sweep averages.
						// NOTE(Dedrick): Samples are pushed in cursor order; within a kind, radius_idx = i % radius_count.
						for (u64 kind_idx = 0; kind_idx < benchmark->kind_count; ++kind_idx) {
							PG_BlurKind const kind = benchmark->kind_order[kind_idx];
							f32 *points = arena_push_array<f32>(pg_context->benchmark_arena, benchmark->radius_count);
							u64 sample_idx = 0;
							for (PG_BenchmarkSampleChunkNode *node = benchmark->samples[kind].first; node != nullptr; node = node->next) {
								for (u64 idx = 0; idx < node->count; ++idx) {
									u64 const radius_idx = sample_idx % benchmark->radius_count;
									f32 *point = &points[radius_idx];
									if (epsilon_equal(*point, 0.0f, 1e-6f) || node->v[idx].ms < *point) {
										*point = node->v[idx].ms;
									}
									sample_idx += 1;
								}
							}
							benchmark->point_ms[kind] = points;
						}
					}
				}
			}
		}
		else {
			ZoneScopedN("blur passes");
			PG_BlurParams params = {};
			params.blur_kind = pg_context->blur_kind;
			params.blur_radius = pg_context->blur_radius;
			params.src_srv = pg_context->source_texture_srv;
			params.src_dim = pg_context->source_texture_dim;
			PG_DisplayEntryList passes = pg_blur(pg_frame_arena(), &params);
			pg_display_entry_list_concat_in_place(&display_entry_list, &passes);
		}

		//~ Dedrick: Display entries -> Push final output.
		pg_display_entry_list_push(pg_frame_arena(), &display_entry_list, "Blurred Output"_str8, pg_context->display_buffer_dim, display_entry_list.last->v.srv);
		pg_context->display_entries = pg_display_entry_array_from_list(pg_frame_arena(), &display_entry_list);

		//~ Dedrick: Finalize selected pass texture.
		{
			ID3D11VertexShader *v_shader = pg_context->v_shaders[PG_VShaderKind_Finalize];
			ID3D11PixelShader *p_shader = pg_context->p_shaders[PG_PShaderKind_Finalize];

			//~ Dedrick: Pick source srv.
			ID3D11ShaderResourceView *source_srv = pg_context->display_entries.v[pg_display_selected_entry_idx(&pg_context->display_entries)].srv;

			//~ Dedrick: Set output merger.
			d_ctx->OMSetRenderTargets(1, &pg_context->display_buffer_rtv, nullptr);

			//~ Dedrick: Set rasterizer.
			uvec2 const viewport_dim = pg_context->display_buffer_dim;
			D3D11_VIEWPORT const viewport = { 0.0f, 0.0f, static_cast<f32>(viewport_dim.x), static_cast<f32>(viewport_dim.y), 0.0f, 1.0f };
			d_ctx->RSSetViewports(1, &viewport);

			//~ Dedrick: Set input assembly.
			d_ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			d_ctx->IASetInputLayout(nullptr);

			//~ Dedrick: Set shaders.
			d_ctx->VSSetShader(v_shader, nullptr, 0);
			d_ctx->PSSetShader(p_shader, nullptr, 0);

			//~ Dedrick: Set shader resources.
			ID3D11ShaderResourceView *const shader_resources[2] = { source_srv, pg_context->tonemap_lut_srv };
			d_ctx->PSSetShaderResources(0, static_cast<UINT>(array_count(shader_resources)), shader_resources);

			//~ Dedrick: Draw.
			d_ctx->Draw(3, 0);
		}

		//~ Dedrick: Resize swapchain, if needed.
		d3d11_window_resize_swapchain_if_needed(pg_context->window, pg_context->window_d3d11);

		//~ Dedrick: Present to frame.
		vec4 const clear_color = { 0.0f, 0.0f, 0.0f, 1.0f };
		d_ctx->ClearRenderTargetView(pg_context->window_d3d11->swapchain_rtv, &clear_color[0]);
		d_ctx->OMSetRenderTargets(1, &pg_context->window_d3d11->swapchain_rtv, nullptr);
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
		pg_context->window_d3d11->swapchain->Present(1, 0);
	}

	//~ Dedrick: Write logs.
	{
		ZoneScopedN("write logs");
		LogFrameResult const log_frame = log_frame_end(scratch.arena);
		if (log_frame.kind_counts[LogKind_Error] > 0) {
			LogEntry *entries = log_frame.entries;
			u64 *const err_indices = log_frame.kind_indices[LogKind_Error];
			u64 const err_count = log_frame.kind_counts[LogKind_Error];
			String8List error_log_lines = {};
			for (u64 e = 0; e < err_count; ++e) {
				LogEntry *entry = &entries[err_indices[e]];
				String8 str = str8(log_frame.string.data + entry->offset, entry->size);
				str = str8_trim_whitespace(str);
				str8_list_push(scratch.arena, &error_log_lines, str);
			}
			String8JoinParams const join_params = { .separator = " "_str8 };
			String8 const error_log_string = str8_list_join(scratch.arena, &error_log_lines, &join_params);

			//~ Dedrick: Fill error buffer.
			pg_context->error_string_size = min(array_count(pg_context->error_buffer), error_log_string.size);
			memory_copy(pg_context->error_buffer, error_log_string.data, pg_context->error_string_size);
			pg_context->error_t = 1.0f;
		}
		if (log_frame.count > 0) {
			std::fwrite(log_frame.string.data, log_frame.string.size, 1, stdout);
		}
	}

	//~ Dedrick: Bump frame counters.
	pg_context->frame_index += 1;

	return pg_context->quit;
}
