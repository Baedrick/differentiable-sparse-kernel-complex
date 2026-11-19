// Copyright (C) 2026 Koh Swee Teck Dedrick.
// Licensed under the MIT license (https://opensource.org/license/mit/).

auto dk::DDS_SubresourceArray::operator[](u64 index) noexcept -> DDS_Subresource & {
	DK_ASSERT(index < count);
	return v[index];
}

auto dk::DDS_SubresourceArray::operator[](u64 index) const noexcept -> DDS_Subresource const & {
	DK_ASSERT(index < count);
	return v[index];
}

auto dk::dds_parsed_from_bytes(Arena *arena, String8 bytes) noexcept -> DDS_Parsed * {
	DDS_Parsed *result = nullptr;
	u8 const *ptr = bytes.data;
	u8 const *opl = ptr + bytes.size;

	//~ Dedrick:
	DDS_Header const *header = nullptr;
	if (ptr + sizeof(*header) <= opl) {
		header = reinterpret_cast<DDS_Header const *>(ptr);
		if (header->magic != DDS_MAGIC || header->size != 124) {
			header = nullptr;
		}
		ptr += sizeof(*header);
	}

	//~ Dedrick:
	if (header != nullptr) {
		DDS_Header_DXT10 const *header_dxt10 = nullptr;
		if ((header->pixel_format.flags & DDS_PixelFormatFlag_FOURCC) != 0 && header->pixel_format.four_cc == DDS_FourCC_DX10) {
			if (ptr + sizeof(*header_dxt10) <= opl) {
				header_dxt10 = reinterpret_cast<DDS_Header_DXT10 const *>(ptr);
				ptr += sizeof(*header_dxt10);
			}
		}

		//~ Dedrick:
		DXGI_FORMAT const format = (header_dxt10 != nullptr) ? header_dxt10->dxgi_format : dds_format_from_pixel_format(&header->pixel_format);

		//~ Dedrick:
		result = arena_push_array<DDS_Parsed>(arena, 1);
		result->header = header;
		result->header_dxt10 = header_dxt10;
		result->format = format;
		result->pixel_bytes = str8(ptr, opl - ptr);
	}

	return result;
}

auto dk::dds_pixel_format_bit_mask_match(DDS_PixelFormat const *pf, u32 r, u32 g, u32 b, u32 a) noexcept -> b8 {
	return pf->r_bit_mask == r && pf->g_bit_mask == g && pf->b_bit_mask == b && pf->a_bit_mask == a;
}

auto dk::dds_format_from_pixel_format(DDS_PixelFormat const *pf) noexcept -> DXGI_FORMAT {
	DXGI_FORMAT result = DXGI_FORMAT_UNKNOWN;
	if ((pf->flags & DDS_PixelFormatFlag_FOURCC) != 0) {
		switch (pf->four_cc) {
			case DDS_FourCC_DXT1: { result = DXGI_FORMAT_BC1_UNORM; break; }
			case DDS_FourCC_DXT2:
			case DDS_FourCC_DXT3: { result = DXGI_FORMAT_BC2_UNORM; break; }
			case DDS_FourCC_DXT4:
			case DDS_FourCC_DXT5: { result = DXGI_FORMAT_BC3_UNORM; break; }
			case DDS_FourCC_ATI1:
			case DDS_FourCC_BC4U: { result = DXGI_FORMAT_BC4_UNORM; break; }
			case DDS_FourCC_BC4S: { result = DXGI_FORMAT_BC4_SNORM; break; }
			case DDS_FourCC_ATI2:
			case DDS_FourCC_BC5U: { result = DXGI_FORMAT_BC5_UNORM; break; }
			case DDS_FourCC_BC5S: { result = DXGI_FORMAT_BC5_SNORM; break; }
			case 36:              { result = DXGI_FORMAT_R16G16B16A16_UNORM; break; }
			case 110:             { result = DXGI_FORMAT_R16G16B16A16_SNORM; break; }
			case 111:             { result = DXGI_FORMAT_R16_FLOAT; break; }
			case 112:             { result = DXGI_FORMAT_R16G16_FLOAT; break; }
			case 113:             { result = DXGI_FORMAT_R16G16B16A16_FLOAT; break; }
			case 114:             { result = DXGI_FORMAT_R32_FLOAT; break; }
			case 115:             { result = DXGI_FORMAT_R32G32_FLOAT; break; }
			case 116:             { result = DXGI_FORMAT_R32G32B32A32_FLOAT; break; }
		}
	}
	else if ((pf->flags & DDS_PixelFormatFlag_RGB) != 0) {
		if (pf->rgb_bit_count == 32) {
			if (dds_pixel_format_bit_mask_match(pf, 0x000000FF, 0x0000FF00, 0x00FF0000, 0xFF000000)) {
				result = DXGI_FORMAT_R8G8B8A8_UNORM;
			}
			else if (dds_pixel_format_bit_mask_match(pf, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000)) {
				result = DXGI_FORMAT_B8G8R8A8_UNORM;
			}
			else if (dds_pixel_format_bit_mask_match(pf, 0x00FF0000, 0x0000FF00, 0x000000FF, 0)) {
				result = DXGI_FORMAT_B8G8R8X8_UNORM;
			}
			// NOTE(Dedrick): https://github.com/microsoft/DirectXTex/blob/1acf4ebe6330e06b7eb52e8db63d4f824ad77427/DDSTextureLoader/DDSTextureLoader11.cpp#L662
			else if (dds_pixel_format_bit_mask_match(pf, 0x3FF00000, 0x000FFC00, 0x000003FF, 0xC0000000)) {
				result = DXGI_FORMAT_R10G10B10A2_UNORM;
			}
			else if (dds_pixel_format_bit_mask_match(pf, 0x0000FFFF, 0xFFFF0000, 0, 0)) {
				result = DXGI_FORMAT_R16G16_UNORM;
			}
		}
		else if (pf->rgb_bit_count == 16) {
			if (dds_pixel_format_bit_mask_match(pf, 0x7C00, 0x03E0, 0x001F, 0x8000)) {
				result = DXGI_FORMAT_B5G5R5A1_UNORM;
			}
			else if (dds_pixel_format_bit_mask_match(pf, 0xF800, 0x07E0, 0x001F, 0)) {
				result = DXGI_FORMAT_B5G6R5_UNORM;
			}
		}
	}
	else if ((pf->flags & DDS_PixelFormatFlag_LUMINANCE) != 0) {
		if (pf->rgb_bit_count == 16) {
			if (dds_pixel_format_bit_mask_match(pf, 0xFFFF, 0, 0, 0)) {
				result = DXGI_FORMAT_R16_UNORM;
			}
			else if (dds_pixel_format_bit_mask_match(pf, 0x00FF, 0, 0, 0xFF00)) {
				result = DXGI_FORMAT_R8G8_UNORM;
			}
		}
		else if (pf->rgb_bit_count == 8) {
			if (dds_pixel_format_bit_mask_match(pf, 0xFF, 0, 0, 0)) {
				result = DXGI_FORMAT_R8_UNORM;
			}
		}
	}
	else if ((pf->flags & DDS_PixelFormatFlag_ALPHA) != 0) {
		if (pf->rgb_bit_count == 8) {
			result = DXGI_FORMAT_A8_UNORM;
		}
	}
	return result;
}

auto dk::dds_surface_info(u32 width, u32 height, DXGI_FORMAT format) noexcept -> DDS_SurfaceInfo {
	u32 block_dim = 1;
	u32 block_bytes = 0;
	switch (format) {
		case DXGI_FORMAT_BC1_TYPELESS:
		case DXGI_FORMAT_BC1_UNORM:
		case DXGI_FORMAT_BC1_UNORM_SRGB:
		case DXGI_FORMAT_BC4_TYPELESS:
		case DXGI_FORMAT_BC4_UNORM:
		case DXGI_FORMAT_BC4_SNORM: {
			block_dim = 4;
			block_bytes = 8;
			break;
		}
		case DXGI_FORMAT_BC2_TYPELESS:
		case DXGI_FORMAT_BC2_UNORM:
		case DXGI_FORMAT_BC2_UNORM_SRGB:
		case DXGI_FORMAT_BC3_TYPELESS:
		case DXGI_FORMAT_BC3_UNORM:
		case DXGI_FORMAT_BC3_UNORM_SRGB:
		case DXGI_FORMAT_BC5_TYPELESS:
		case DXGI_FORMAT_BC5_UNORM:
		case DXGI_FORMAT_BC5_SNORM:
		case DXGI_FORMAT_BC6H_TYPELESS:
		case DXGI_FORMAT_BC6H_UF16:
		case DXGI_FORMAT_BC6H_SF16:
		case DXGI_FORMAT_BC7_TYPELESS:
		case DXGI_FORMAT_BC7_UNORM:
		case DXGI_FORMAT_BC7_UNORM_SRGB: {
			block_dim = 4;
			block_bytes = 16;
			break;
		}
		case DXGI_FORMAT_R32G32B32A32_TYPELESS:
		case DXGI_FORMAT_R32G32B32A32_FLOAT:
		case DXGI_FORMAT_R32G32B32A32_UINT:
		case DXGI_FORMAT_R32G32B32A32_SINT: {
			block_bytes = 16;
			break;
		}
		case DXGI_FORMAT_R16G16B16A16_TYPELESS:
		case DXGI_FORMAT_R16G16B16A16_FLOAT:
		case DXGI_FORMAT_R16G16B16A16_UNORM:
		case DXGI_FORMAT_R16G16B16A16_UINT:
		case DXGI_FORMAT_R16G16B16A16_SNORM:
		case DXGI_FORMAT_R16G16B16A16_SINT:
		case DXGI_FORMAT_R32G32_TYPELESS:
		case DXGI_FORMAT_R32G32_FLOAT:
		case DXGI_FORMAT_R32G32_UINT:
		case DXGI_FORMAT_R32G32_SINT: {
			block_bytes = 8;
			break;
		}
		case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
		case DXGI_FORMAT_R8G8B8A8_TYPELESS:
		case DXGI_FORMAT_R8G8B8A8_UNORM:
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		case DXGI_FORMAT_B8G8R8A8_UNORM:
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		case DXGI_FORMAT_B8G8R8X8_UNORM:
		case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
		case DXGI_FORMAT_R10G10B10A2_TYPELESS:
		case DXGI_FORMAT_R10G10B10A2_UNORM:
		case DXGI_FORMAT_R10G10B10A2_UINT:
		case DXGI_FORMAT_R11G11B10_FLOAT:
		case DXGI_FORMAT_R16G16_FLOAT:
		case DXGI_FORMAT_R16G16_UNORM:
		case DXGI_FORMAT_R32_FLOAT:
		case DXGI_FORMAT_R32_UINT: {
			block_bytes = 4;
			break;
		}
		case DXGI_FORMAT_R8G8_UNORM:
		case DXGI_FORMAT_R8G8_UINT:
		case DXGI_FORMAT_R16_FLOAT:
		case DXGI_FORMAT_R16_UNORM:
		case DXGI_FORMAT_B5G6R5_UNORM:
		case DXGI_FORMAT_B5G5R5A1_UNORM:
		case DXGI_FORMAT_B4G4R4A4_UNORM: {
			block_bytes = 2;
			break;
		}
		case DXGI_FORMAT_R8_UNORM:
		case DXGI_FORMAT_R8_UINT:
		case DXGI_FORMAT_A8_UNORM: {
			block_bytes = 1;
			break;
		}
	}

	u32 const num_blocks_wide = max<u32>((width + block_dim - 1) / block_dim, 1);
	u32 const num_blocks_high = max<u32>((height + block_dim - 1) / block_dim, 1);

	DDS_SurfaceInfo result = {};
	result.row_pitch = num_blocks_wide * block_bytes;
	result.num_rows = num_blocks_high;
	result.slice_pitch = result.row_pitch * result.num_rows;
	return result;
}

auto dk::dds_alpha_mode_from_parsed(DDS_Parsed const *dds) noexcept -> DDS_AlphaMode {
	DDS_AlphaMode result = DDS_AlphaMode_Null;
	DDS_PixelFormat const *pf = &dds->header->pixel_format;
	if ((pf->flags & DDS_PixelFormatFlag_FOURCC) != 0) {
		if (dds->header_dxt10 != nullptr) {
			u32 const mode = dds->header_dxt10->misc_flags2 & DDS_MiscFlags2_AlphaModeMask;
			switch (mode) {
				case DDS_AlphaMode_Straight:
				case DDS_AlphaMode_Premultiplied:
				case DDS_AlphaMode_Opaque:
				case DDS_AlphaMode_Custom: {
					result = static_cast<DDS_AlphaMode>(mode);
					break;
				}
			}
		}
		else if (pf->four_cc == DDS_FourCC_DXT2 || pf->four_cc == DDS_FourCC_DXT4) {
			result = DDS_AlphaMode_Premultiplied;
		}
	}
	return result;
}

auto dk::dds_resource_dim_from_parsed(DDS_Parsed const *dds) noexcept -> DDS_ResourceDim {
	DDS_ResourceDim result = DDS_ResourceDim_Tex2D;
	if (dds->header_dxt10 != nullptr) {
		result = dds->header_dxt10->resource_dim;
	}
	else if ((dds->header->flags & DDS_HeaderFlag_VOLUME) != 0) {
		result = DDS_ResourceDim_Tex3D;
	}
	return result;
}

auto dk::dds_depth_from_parsed(DDS_Parsed const *dds) noexcept -> u32 {
	u32 depth = 1;
	if (dds_resource_dim_from_parsed(dds) == DDS_ResourceDim_Tex3D) {
		depth = max<u32>(1, dds->header->depth);
	}
	return depth;
}

auto dk::dds_mip_count_from_parsed(DDS_Parsed const *dds) noexcept -> u32 {
	return max<u32>(1, dds->header->mip_count);
}

auto dk::dds_array_size_from_parsed(DDS_Parsed const *dds) noexcept -> u32 {
	u32 array_size = 1;
	b8 const cubemap = dds_is_cubemap_from_parsed(dds);
	if (dds->header_dxt10 != nullptr) {
		array_size = max<u32>(array_size, dds->header_dxt10->array_size);
		if (cubemap) {
			array_size *= 6;
		}
		if (dds->header_dxt10->resource_dim == DDS_ResourceDim_Tex3D) {
			array_size = 1;
		}
	}
	else if (cubemap) {
		array_size = 6;
	}
	return array_size;
}

auto dk::dds_misc_flags_from_parsed(DDS_Parsed const *dds) noexcept -> u32 {
	u32 result = 0;
	if (dds->header_dxt10 != nullptr) {
		result = dds->header_dxt10->misc_flag;
	}
	else if ((dds->header->caps2 & DDS_Caps2Flag_CUBEMAP) != 0) {
		result = DDS_MiscFlags_TEXTURECUBE;
	}
	return result;
}

auto dk::dds_is_cubemap_from_parsed(DDS_Parsed const *dds) noexcept -> b8 {
	return (dds_misc_flags_from_parsed(dds) & DDS_MiscFlags_TEXTURECUBE) != 0;
}

auto dk::dds_subresource_from_parsed(DDS_Parsed const *dds, u32 array_idx, u32 mip_idx) noexcept -> DDS_Subresource {
	DDS_Subresource result = {};
	u32 const mip_count = dds_mip_count_from_parsed(dds);
	u32 const array_size = dds_array_size_from_parsed(dds);
	u32 const depth = dds_depth_from_parsed(dds);
	u32 const clamped_array_idx = min<u32>(array_idx, array_size - 1);
	u32 const clamped_mip_idx = min<u32>(mip_idx, mip_count - 1);

	u32 mip_width = max<u32>(1, dds->header->width);
	u32 mip_height = max<u32>(1, dds->header->height);
	u32 mip_depth = depth;
	u64 target_mip_offset = 0;
	u64 target_mip_size = 0;
	u64 slice_size = 0;
	for (u32 m = 0; m < mip_count; ++m) {
		DDS_SurfaceInfo const info = dds_surface_info(mip_width, mip_height, dds->format);
		u64 const size = static_cast<u64>(info.slice_pitch) * mip_depth;
		if (m == clamped_mip_idx) {
			target_mip_offset  = slice_size;
			target_mip_size    = size;
			result.width       = mip_width;
			result.height      = mip_height;
			result.depth       = mip_depth;
			result.row_pitch   = info.row_pitch;
			result.slice_pitch = info.slice_pitch;
		}
		slice_size += size;
		mip_width  = max<u32>(1, mip_width >> 1);
		mip_height = max<u32>(1, mip_height >> 1);
		mip_depth  = max<u32>(1, mip_depth >> 1);
	}

	u64 const total_offset = static_cast<u64>(clamped_array_idx) * slice_size + target_mip_offset;
	result.bytes = str8(dds->pixel_bytes.data + total_offset, target_mip_size);
	return result;
}

auto dk::dds_subresource_list_from_parsed(Arena *arena, DDS_Parsed const *dds) noexcept -> DDS_SubresourceList {
	DDS_SubresourceList result = {};
	result.mip_count = dds_mip_count_from_parsed(dds);
	result.array_size = dds_array_size_from_parsed(dds);

	u32 const depth = dds_depth_from_parsed(dds);
	u8 const *ptr = dds->pixel_bytes.data;
	for (u32 array_idx = 0; array_idx < result.array_size; ++array_idx) {
		u32 mip_width = max<u32>(1, dds->header->width);
		u32 mip_height = max<u32>(1, dds->header->height);
		u32 mip_depth = depth;
		for (u32 mip_idx = 0; mip_idx < result.mip_count; ++mip_idx) {
			DDS_SurfaceInfo const info = dds_surface_info(mip_width, mip_height, dds->format);
			u64 const size = static_cast<u64>(info.slice_pitch) * mip_depth;

			DDS_SubresourceNode *node = arena_push_array<DDS_SubresourceNode>(arena, 1);
			node->v.bytes       = str8(ptr, size);
			node->v.width       = mip_width;
			node->v.height      = mip_height;
			node->v.depth       = mip_depth;
			node->v.row_pitch   = info.row_pitch;
			node->v.slice_pitch = info.slice_pitch;
			forward_list_queue_push(&result.first, &result.last, node);
			result.count += 1;

			ptr += size;
			mip_width  = max<u32>(1, mip_width >> 1);
			mip_height = max<u32>(1, mip_height >> 1);
			mip_depth  = max<u32>(1, mip_depth >> 1);
		}
	}

	return result;
}

auto dk::dds_subresource_array_from_list(Arena *arena, DDS_SubresourceList const *list) noexcept -> DDS_SubresourceArray {
	DDS_SubresourceArray result = {};
	result.count = list->count;
	result.mip_count = list->mip_count;
	result.array_size = list->array_size;
	result.v = arena_push_array<DDS_Subresource>(arena, result.count);
	u64 idx = 0;
	for (DDS_SubresourceNode const *node = list->first; node != nullptr; node = node->next) {
		result[idx++] = node->v;
	}
	return result;
}

auto dk::dds_subresource_array_from_parsed(Arena *arena, DDS_Parsed const *dds) noexcept -> DDS_SubresourceArray {
	DDS_SubresourceList const list = dds_subresource_list_from_parsed(arena, dds);
	DDS_SubresourceArray const result = dds_subresource_array_from_list(arena, &list);
	return result;
}

auto dk::dds_subresource_from_array(DDS_SubresourceArray array, u32 array_idx, u32 mip_idx) noexcept -> DDS_Subresource {
	u32 const clamped_array_idx = min<u32>(array_idx, array.array_size - 1);
	u32 const clamped_mip_idx = min<u32>(mip_idx, array.mip_count - 1);
	u32 const idx = clamped_array_idx * array.mip_count + clamped_mip_idx;
	return array[idx];
}
