// Copyright (C) 2026 Koh Swee Teck Dedrick.
// Licensed under the MIT license (https://opensource.org/license/mit/).

#ifndef DDS_HPP__
#define DDS_HPP__

#include <dxgiformat.h>

namespace dk {
	constexpr u32 DDS_MAGIC = 0x20534444; // "DDS "

	using DDS_PixelFormatFlags = u32;
	enum : u32 {
		DDS_PixelFormatFlag_FOURCC     = 0x4,
		DDS_PixelFormatFlag_RGB        = 0x40,
		DDS_PixelFormatFlag_LUMINANCE  = 0x20000,
		DDS_PixelFormatFlag_ALPHA      = 0x2,
		DDS_PixelFormatFlag_BUMPDUDV   = 0x80000
	};

	using DDS_FourCC = u32;
	enum : u32 {
		DDS_FourCC_DX10 = 0x30315844, // "DX10"
		DDS_FourCC_DXT1 = 0x31545844, // "DXT1"
		DDS_FourCC_DXT2 = 0x32545844, // "DXT2"
		DDS_FourCC_DXT3 = 0x33545844, // "DXT3"
		DDS_FourCC_DXT4 = 0x34545844, // "DXT4"
		DDS_FourCC_DXT5 = 0x35545844, // "DXT5"
		DDS_FourCC_ATI1 = 0x31495441, // "ATI1"
		DDS_FourCC_BC4U = 0x55344342, // "BC4U"
		DDS_FourCC_BC4S = 0x53344342, // "BC4S"
		DDS_FourCC_ATI2 = 0x32495441, // "ATI2"
		DDS_FourCC_BC5U = 0x55354342, // "BC5U"
		DDS_FourCC_BC5S = 0x53354342, // "BC5S"
	};

	enum DDS_AlphaMode {
		DDS_AlphaMode_Null = 0,
		DDS_AlphaMode_Straight,
		DDS_AlphaMode_Premultiplied,
		DDS_AlphaMode_Opaque,
		DDS_AlphaMode_Custom
	};

	struct DDS_PixelFormat {
		u32 size;
		DDS_PixelFormatFlags flags;
		DDS_FourCC four_cc;
		u32 rgb_bit_count;
		u32 r_bit_mask;
		u32 g_bit_mask;
		u32 b_bit_mask;
		u32 a_bit_mask;
	};

	using DDS_HeaderFlags = u32;
	enum : u32 {
		DDS_HeaderFlag_VOLUME = 0x800000
	};

	using DDS_Caps2Flags = u32;
	enum : u32 {
		DDS_Caps2Flag_CUBEMAP            = 0x200,
		DDS_Caps2Flag_CUBEMAP_POSITIVEX  = 0x600,
		DDS_Caps2Flag_CUBEMAP_NEGATIVEX  = 0xA00,
		DDS_Caps2Flag_CUBEMAP_POSITIVEY  = 0x1200,
		DDS_Caps2Flag_CUBEMAP_NEGATIVEY  = 0x2200,
		DDS_Caps2Flag_CUBEMAP_POSITIVEZ  = 0x4200,
		DDS_Caps2Flag_CUBEMAP_NEGATIVEZ  = 0x8200,
		DDS_Caps2Flag_CUBEMAP_ALLFACES   = 0xFE00
	};

	struct DDS_Header {
		u32 magic;
		u32 size;
		DDS_HeaderFlags flags;
		u32 height;
		u32 width;
		u32 pitch_or_linear_size;
		u32 depth; ///< Only if DDS_HeaderFlag_VOLUME is set in flags.
		u32 mip_count;
		u32 reserved1[11];
		DDS_PixelFormat pixel_format;
		u32 caps;
		DDS_Caps2Flags caps2;
		u32 caps3;
		u32 caps4;
		u32 reserved2;
	};

	using DDS_ResourceDim = u32;
	enum : u32 {
		DDS_ResourceDim_Unknown  = 0,
		DDS_ResourceDim_Tex1D    = 2,
		DDS_ResourceDim_Tex2D    = 3,
		DDS_ResourceDim_Tex3D    = 4
	};

	using DDS_MiscFlags = u32;
	enum : u32 {
		DDS_MiscFlags_TEXTURECUBE = 0x4
	};

	using DDS_MiscFlags2 = u32;
	enum : u32 {
		DDS_MiscFlags2_AlphaModeMask = 0x7
	};

	struct DDS_Header_DXT10 {
		DXGI_FORMAT dxgi_format;
		DDS_ResourceDim resource_dim;
		u32 misc_flag;
		u32 array_size;
		DDS_MiscFlags2 misc_flags2;
	};

	struct DDS_Parsed {
		DDS_Header const *header;
		DDS_Header_DXT10 const *header_dxt10;
		DXGI_FORMAT format;
		String8 pixel_bytes;
	};

	struct DDS_Subresource {
		String8 bytes;
		u32 width;
		u32 height;
		u32 depth;
		u32 row_pitch;
		u32 slice_pitch;
	};

	struct DDS_SubresourceNode {
		DDS_SubresourceNode *next;
		DDS_Subresource v;
	};

	struct DDS_SubresourceList {
		DDS_SubresourceNode *first;
		DDS_SubresourceNode *last;
		u64 count;
		u32 mip_count;
		u32 array_size;
	};

	struct DDS_SubresourceArray {
		DDS_Subresource *v;
		u64 count;
		u32 mip_count;
		u32 array_size;

		auto operator[](u64 index) noexcept -> DDS_Subresource &;
		auto operator[](u64 index) const noexcept -> DDS_Subresource const &;
	};

	struct DDS_SurfaceInfo {
		u32 slice_pitch;
		u32 row_pitch;
		u32 num_rows;
	};

	auto dds_parsed_from_bytes(Arena *arena, String8 bytes) noexcept -> DDS_Parsed *;

	// NOTE(Dedrick): Use pixelformat only when dxt10 ext is not present.
	auto dds_pixel_format_bit_mask_match(DDS_PixelFormat const *pf, u32 r, u32 g, u32 b, u32 a) noexcept -> b8;
	auto dds_format_from_pixel_format(DDS_PixelFormat const *pf) noexcept -> DXGI_FORMAT;

	auto dds_surface_info(u32 width, u32 height, DXGI_FORMAT format) noexcept -> DDS_SurfaceInfo;

	auto dds_alpha_mode_from_parsed(DDS_Parsed const *dds) noexcept -> DDS_AlphaMode;
	auto dds_resource_dim_from_parsed(DDS_Parsed const *dds) noexcept -> DDS_ResourceDim;
	auto dds_depth_from_parsed(DDS_Parsed const *dds) noexcept -> u32;
	auto dds_mip_count_from_parsed(DDS_Parsed const *dds) noexcept -> u32;
	auto dds_array_size_from_parsed(DDS_Parsed const *dds) noexcept -> u32;
	auto dds_misc_flags_from_parsed(DDS_Parsed const *dds) noexcept -> u32;
	auto dds_is_cubemap_from_parsed(DDS_Parsed const *dds) noexcept -> b8;

	auto dds_subresource_from_parsed(DDS_Parsed const *dds, u32 array_idx, u32 mip_idx) noexcept -> DDS_Subresource;
	auto dds_subresource_list_from_parsed(Arena *arena, DDS_Parsed const *dds) noexcept -> DDS_SubresourceList;
	auto dds_subresource_array_from_list(Arena *arena, DDS_SubresourceList const *list) noexcept -> DDS_SubresourceArray;
	auto dds_subresource_array_from_parsed(Arena *arena, DDS_Parsed const *dds) noexcept -> DDS_SubresourceArray;

	auto dds_subresource_from_array(DDS_SubresourceArray array, u32 array_idx, u32 mip_idx) noexcept -> DDS_Subresource;
}

#endif // DDS_HPP__
