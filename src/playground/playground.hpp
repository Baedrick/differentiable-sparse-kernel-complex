// Copyright (C) 2026 Koh Swee Teck Dedrick.
// Licensed under the MIT license (https://opensource.org/license/mit/).

#ifndef PG_HPP__
#define PG_HPP__

//~ Dedrick: Generated code.
#include "generated/playground.embed.hpp"

namespace dk {
	enum PG_FileFormat {
		PG_FileFormat_Null = 0,
		PG_FileFormat_EXR,
		PG_FileFormat_HDR,
		PG_FileFormat_PNG,
		PG_FileFormat_JPEG
	};

	struct PG_ImageData {
		void *pixels;
		uvec2 dim;
		DXGI_FORMAT format;
	};

	struct PG_ImageLoadParams {
		Arena *arena;
		u64 request_id;
		String8 file_path;
		PG_ImageData *image_data;
		Semaphore load_completion_semaphore;
	};

	constexpr s32 PG_BlurRadiusMax = 250;
	constexpr f32 PG_ViewportZoomMin = 1.0f / 256.0f;
	constexpr f32 PG_ViewportZoomMax = 256.0f;

	enum PG_VShaderKind {
		PG_VShaderKind_DSKC,
		PG_VShaderKind_SeparableGaussian,
		PG_VShaderKind_Finalize,
		PG_VShaderKind_COUNT
	};
	inline String8 const pg_v_shader_kind_src_table[] = {
		pg_dskc_vs_bytes,
		pg_separable_gaussian_vs_bytes,
		pg_finalize_vs_bytes,
	};
	static_assert(array_count(pg_v_shader_kind_src_table) == PG_VShaderKind_COUNT);

	enum PG_PShaderKind {
		PG_PShaderKind_DSKC,
		PG_PShaderKind_SeparableGaussian,
		PG_PShaderKind_Finalize,
		PG_PShaderKind_COUNT
	};
	inline String8 const pg_p_shader_kind_src_table[] = {
		pg_dskc_ps_bytes,
		pg_separable_gaussian_ps_bytes,
		pg_finalize_ps_bytes,
	};
	static_assert(array_count(pg_p_shader_kind_src_table) == PG_PShaderKind_COUNT);

	constexpr u32 PG_DSKCLutMaxLayers = 48;

	struct PG_DSKCLutHeader {
		f32 param_min;
		f32 param_max;
		u32 layer_count;
		u32 sample_count;
		u32 directory_offset;
		u32 directory_count;
	};
	static_assert(sizeof(PG_DSKCLutHeader) == 24);

	struct PG_DSKCLutEntry {
		f32 param;
		u32 tap_offset;
		u32 tap_count;
	};
	static_assert(sizeof(PG_DSKCLutEntry) == 12);

	struct PG_DSKCLutTap {
		f32 offset_x;
		f32 offset_y;
		f32 weight;
	};
	static_assert(sizeof(PG_DSKCLutTap) == 12);

	struct PG_Uniform_DSKCPass {
		vec2 uv_step;
		s32 layer;
		s32 sample_count;
		u8 _pad0[240];
	};
	static_assert(sizeof(PG_Uniform_DSKCPass) % 256 == 0);

	struct PG_Uniform_DSKC {
		PG_Uniform_DSKCPass passes[PG_DSKCLutMaxLayers];
	};

	struct PG_Uniform_GaussianPass {
		vec2 uv_step;
		s32 kernel_size;
		u8 _pad0[244];
	};
	static_assert(sizeof(PG_Uniform_GaussianPass) % 256 == 0);

	struct PG_Uniform_Gaussian {
		PG_Uniform_GaussianPass passes[Axis2_COUNT];
	};

	enum PG_UniformKind {
		PG_UniformKind_DSKC,
		PG_UniformKind_SeparableGaussian,
		PG_UniformKind_COUNT
	};
	inline u64 const pg_uniform_type_kind_size_table[] = {
		sizeof(PG_Uniform_DSKC),
		sizeof(PG_Uniform_Gaussian),
	};
	static_assert(array_count(pg_uniform_type_kind_size_table) == PG_UniformKind_COUNT);

	enum PG_BlurKind {
		PG_BlurKind_DSKC,
		PG_BlurKind_SeparableGaussian,
		PG_BlurKind_COUNT
	};
	inline String8 const pg_blur_kind_name_table[] = {
		"Differentiable Sparse Kernel Complex"_str8,
		"Separable Gaussian"_str8,
	};
	static_assert(array_count(pg_blur_kind_name_table) == PG_BlurKind_COUNT);

	// https://lospec.com/palette-list/island-joy-16
	constexpr u32 pg_blur_kind_color_table[] = {
		0xFF6424C9,
		0xFFC1AD11,
		0xFF61B35B,
		0xFF684DCB,
	};
	static_assert(array_count(pg_blur_kind_color_table) >= PG_BlurKind_COUNT);

	constexpr u64 PG_BenchmarkSweepCount = 4;

	enum PG_BenchmarkState {
		PG_BenchmarkState_Null = 0,
		PG_BenchmarkState_Running,
		PG_BenchmarkState_Completed,
	};

	struct PG_BenchmarkSample {
		f32 ms;
	};

	struct PG_BenchmarkSampleChunkNode {
		PG_BenchmarkSampleChunkNode *next;
		PG_BenchmarkSample *v;
		u64 count;
		u64 capacity;
	};

	struct PG_BenchmarkSampleChunkList {
		PG_BenchmarkSampleChunkNode *first;
		PG_BenchmarkSampleChunkNode *last;
		u64 chunk_count;
		u64 total_count;
	};

	struct PG_Benchmark {
		PG_BenchmarkState state;

		//~ Dedrick: Run configuration.
		PG_BlurKind kind_order[PG_BlurKind_COUNT];
		u64 kind_count;
		s32 *radii;
		u64 radius_count;

		//~ Dedrick: Cursor.
		u64 sweep;
		u64 radius_idx;
		u32 kind_idx;

		//~ Dedrick: Sample time.
		f32 batch_iteration_ms[PG_BlurKind_COUNT];

		//~ Dedrick: Samples.
		PG_BenchmarkSampleChunkList samples[PG_BlurKind_COUNT];

		//~ Dedrick: Reduced result.
		f32 *point_ms[PG_BlurKind_COUNT];
	};

	struct PG_BlurTexture {
		PG_BlurTexture *next;
		PG_BlurTexture *prev;
		ID3D11Texture2D *texture;
		ID3D11ShaderResourceView *srv;
		ID3D11RenderTargetView *rtv;
		uvec2 dim;
		DXGI_FORMAT format;
	};

	struct PG_DisplayEntry {
		String8 label;
		ID3D11ShaderResourceView *srv;
	};

	struct PG_DisplayEntryNode {
		PG_DisplayEntryNode *next;
		PG_DisplayEntry v;
	};

	struct PG_DisplayEntryList {
		PG_DisplayEntryNode *first;
		PG_DisplayEntryNode *last;
		u64 count;
	};

	struct PG_DisplayEntryArray {
		PG_DisplayEntry *v;
		u64 count;
	};

	struct PG_BlurParams {
		PG_BlurKind blur_kind;
		s32 blur_radius;
		ID3D11ShaderResourceView *src_srv;
		uvec2 src_dim;
	};

	struct PG_Context {
		Arena *arena;
		b8 quit;

		//~ Dedrick: Config path.
		String8 ini_path;

		//~ Dedrick: Log.
		LogContext *log;

		//~ Dedrick: Frame history.
		u64 frame_index;
		Arena *frame_arenas[2];
		f64 time_in_seconds;

		//~ Dedrick: Frame timing.
		u64 last_frame_time_us;
		s64 target_frame_time_us;
		s64 vsync_max_error_us;
		s64 snap_frequencies[8];
		s64 time_averager[4];
		s64 time_averager_residual;
		u32 time_averager_head;
		f32 frame_dt;

		//~ Dedrick: Shaders.
		ID3D11VertexShader *v_shaders[PG_VShaderKind_COUNT];
		ID3D11PixelShader *p_shaders[PG_PShaderKind_COUNT];
		ID3D11Buffer *uniform_buffers[PG_UniformKind_COUNT];

		//~ Dedrick: Textures.
		ID3D11Texture3D *tonemap_lut;
		ID3D11ShaderResourceView *tonemap_lut_srv;

		//~ Dedrick: Blur options.
		PG_BlurKind blur_kind;
		s32 blur_radius;

		//~ Dedrick: Gaussian state.
		ID3D11Buffer *gaussian_kernel_buffer;
		ID3D11ShaderResourceView *gaussian_kernel_buffer_srv;
		s32 gaussian_kernel_radius;

		//~ Dedrick: DSKC state.
		ID3D11Buffer *dskc_lut_buffer;
		ID3D11ShaderResourceView *dskc_lut_buffer_srv;
		s32 dskc_lut_blur_radius;

		//~ Dedrick: Benchmark options.
		u32 benchmark_blur_kind_flags;

		//~ Dedrick: Benchmark state.
		Arena *benchmark_arena;
		ID3D11Query *benchmark_disjoint_query;
		ID3D11Query *benchmark_start_query;
		ID3D11Query *benchmark_end_query;
		ID3D11Query *benchmark_event_query;
		PG_Benchmark benchmark;
		b8 benchmark_start_requested;
		b8 benchmark_cancel_requested;
		b8 benchmark_graph_open;

		//~ Dedrick: Pending image load request.
		Arena *image_load_request_arena;
		String8 image_load_request_path;
		u64 image_load_request_id;

		//~ Dedrick: Image load state.
		Thread image_load_thread;
		PG_ImageLoadParams *image_load_params;

		//~ Dedrick: Source texture.
		Arena *source_texture_arena;
		String8 source_texture_name;
		ID3D11Texture2D *source_texture;
		ID3D11ShaderResourceView *source_texture_srv;
		uvec2 source_texture_dim;
		DXGI_FORMAT source_texture_format;
		b8 source_texture_is_user_provided;
		b8 source_texture_clear;

		//~ Dedrick: Blur texture pool.
		Arena *blur_texture_arena;
		PG_BlurTexture *blur_texture_first;
		PG_BlurTexture *blur_texture_last;
		PG_BlurTexture *blur_texture_free_first;
		PG_BlurTexture *blur_texture_free_last;

		//~ Dedrick: Display list state.
		PG_DisplayEntryArray display_entries;
		u64 selected_display_entry_idx; // NOTE(Dedrick): 0 = auto (last entry); else 1-based entry number.

		//~ Dedrick: Display buffer.
		ID3D11Texture2D *display_buffer;
		ID3D11RenderTargetView *display_buffer_rtv;
		ID3D11ShaderResourceView *display_buffer_srv;
		uvec2 display_buffer_dim;

		//~ Dedrick: Viewport interaction.
		f32 viewport_zoom; // NOTE(Dedrick): 0 = auto (fit to viewport); else screen px per image px.
		vec2 viewport_center_pos;

		//~ Dedrick: Window.
		RGFW_monitor const *active_monitor;
		RGFW_window *window;
		D3D11_Window *window_d3d11;

		//~ Dedrick: Error display state.
		u8 error_buffer[512];
		u64 error_string_size;
		f32 error_t;
	};

	extern PG_Context *pg_context;

	auto pg_r11g11b10_from_rgba32f_pixels(Arena *arena, f32 const *pixels, u32 width, u32 height) noexcept -> u32 *;
	auto pg_image_load_thread_entry_point(void *p) noexcept -> void;

	auto pg_benchmark_sample_chunk_list_push(Arena *arena, PG_BenchmarkSampleChunkList *list, u64 capacity, PG_BenchmarkSample const *sample) noexcept -> void;
	auto pg_benchmark_fill_radii(Arena *arena, PG_Benchmark *benchmark, s32 max_radius) noexcept -> void;

	auto pg_frame_arena() noexcept -> Arena *;

	auto pg_set_target_frame_time_and_snapping_from_monitor(RGFW_monitor const *monitor) noexcept -> void;

	auto pg_blur_texture_cache_get_or_create(uvec2 dim, DXGI_FORMAT format) noexcept -> PG_BlurTexture *;
	auto pg_blur_texture_cache_reset() noexcept -> void;

	auto pg_display_entry_list_push(Arena *arena, PG_DisplayEntryList *list, String8 name, uvec2 dim, ID3D11ShaderResourceView *srv) noexcept -> void;
	auto pg_display_entry_list_concat_in_place(PG_DisplayEntryList *list, PG_DisplayEntryList *to_push) noexcept -> void;
	auto pg_display_entry_array_from_list(Arena *arena, PG_DisplayEntryList const *list) noexcept -> PG_DisplayEntryArray;
	auto pg_display_selected_entry_idx(PG_DisplayEntryArray const *entries) noexcept -> u64;

	auto pg_blur(Arena *arena, PG_BlurParams const *params) noexcept -> PG_DisplayEntryList;

	auto pg_init(CmdLine *cmd_line) noexcept -> void;
	auto pg_shutdown() noexcept -> void;
	auto pg_frame() noexcept -> b8;
}

#endif // PG_HPP__
