// Copyright (C) 2026 Koh Swee Teck Dedrick.
// Licensed under the MIT license (https://opensource.org/license/mit/).

dk::D3D11_Context *dk::d3d11_context;

auto dk::d3d11_init(CmdLine *cmd_line) noexcept -> void {
	ZoneScoped;

	HRESULT error = 0;
	Arena *arena = arena_alloc();
	d3d11_context = arena_push<D3D11_Context>(arena);
	d3d11_context->arena = arena;

	//~ Dedrick: Create device.
	{
		ZoneScopedN("create device");
		UINT creation_flags = 0;
#if !defined(NDEBUG)
		if (cmd_line_has_flag(cmd_line, "d3d11_debug"_str8)) {
			creation_flags |= D3D11_CREATE_DEVICE_DEBUG;
		}
#endif
		D3D_FEATURE_LEVEL const feature_levels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
		D3D_DRIVER_TYPE driver_type = D3D_DRIVER_TYPE_HARDWARE;
		if (cmd_line_has_flag(cmd_line, "force_d3d11_software"_str8)) {
			driver_type = D3D_DRIVER_TYPE_WARP;
		}
		error = D3D11CreateDevice(
			nullptr,
			driver_type,
			nullptr,
			creation_flags,
			feature_levels, static_cast<UINT>(array_count(feature_levels)),
			D3D11_SDK_VERSION,
			&d3d11_context->base_device, 0, &d3d11_context->base_device_context
		);
		if (FAILED(error) && driver_type == D3D_DRIVER_TYPE_HARDWARE) {
			// NOTE(Dedrick): Retry with software rendering.
			error = D3D11CreateDevice(
				nullptr,
				D3D_DRIVER_TYPE_WARP,
				nullptr,
				creation_flags,
				feature_levels, static_cast<UINT>(array_count(feature_levels)),
				D3D11_SDK_VERSION,
				&d3d11_context->base_device, 0, &d3d11_context->base_device_context
			);
		}
		if (FAILED(error)) {
			TempArena const scratch = scratch_begin(nullptr, 0);
			String8 const msg = str8f(scratch.arena, "DXGI swap chain creation failure (0x%lx). Terminating.", error);
			dt_show_dialog(nullptr, "Fatal Error"_str8, msg, true);
			abort_self(1);
		}
	}

	//~ Dedrick: Enable break on error.
#if !defined(NDEBUG)
	if (cmd_line_has_flag(cmd_line, "d3d11_debug"_str8)) {
		ZoneScopedN("enable break on error");
		ID3D11InfoQueue *info = nullptr;
		error = d3d11_context->base_device->QueryInterface(IID_PPV_ARGS(&info));
		if (SUCCEEDED(error)) {
			error = info->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, TRUE);
			error = info->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, TRUE);
			info->Release();
		}
	}
#endif

	//~ Dedrick: Get main device.
	{
		ZoneScopedN("get main device");
		error = d3d11_context->base_device->QueryInterface(IID_PPV_ARGS(&d3d11_context->device));
		error = d3d11_context->base_device_context->QueryInterface(IID_PPV_ARGS(&d3d11_context->device_context));
	}

	//~ Dedrick: Get adapter, factory.
	{
		ZoneScopedN("get adapter, factory");
		IDXGIDevice1 *dxgi_device = nullptr;
		error = d3d11_context->device->QueryInterface(IID_PPV_ARGS(&dxgi_device));
		error = dxgi_device->GetAdapter(&d3d11_context->dxgi_adapter);
		error = d3d11_context->dxgi_adapter->GetParent(IID_PPV_ARGS(&d3d11_context->dxgi_factory));
		dxgi_device->Release();
	}

	//~ Dedrick: Create default rasterizer.
	{
		ZoneScopedN("create default rasterizer");
		D3D11_RASTERIZER_DESC desc = {};
		desc.FillMode       = D3D11_FILL_SOLID;
		desc.CullMode       = D3D11_CULL_BACK;
		desc.ScissorEnable  = TRUE;
		error = d3d11_context->device->CreateRasterizerState(&desc, &d3d11_context->default_rasterizer);
	}

	//~ Dedrick: Create default blend state.
	{
		ZoneScopedN("create default blend state");
		D3D11_BLEND_DESC desc = {};
		desc.RenderTarget[0].BlendEnable            = 1;
		desc.RenderTarget[0].SrcBlend               = D3D11_BLEND_SRC_ALPHA;
		desc.RenderTarget[0].DestBlend              = D3D11_BLEND_INV_SRC_ALPHA;
		desc.RenderTarget[0].BlendOp                = D3D11_BLEND_OP_ADD;
		desc.RenderTarget[0].SrcBlendAlpha          = D3D11_BLEND_ONE;
		desc.RenderTarget[0].DestBlendAlpha         = D3D11_BLEND_ZERO;
		desc.RenderTarget[0].BlendOpAlpha           = D3D11_BLEND_OP_ADD;
		desc.RenderTarget[0].RenderTargetWriteMask  = D3D11_COLOR_WRITE_ENABLE_ALL;
		error = d3d11_context->device->CreateBlendState(&desc, &d3d11_context->default_blend_state);
	}

	//~ Dedrick: Create nearest clamp sampler.
	{
		ZoneScopedN("create nearest sampler");
		D3D11_SAMPLER_DESC desc = {};
		desc.Filter          = D3D11_FILTER_MIN_MAG_MIP_POINT;
		desc.AddressU        = D3D11_TEXTURE_ADDRESS_CLAMP;
		desc.AddressV        = D3D11_TEXTURE_ADDRESS_CLAMP;
		desc.AddressW        = D3D11_TEXTURE_ADDRESS_CLAMP;
		desc.ComparisonFunc  = D3D11_COMPARISON_NEVER;
		error = d3d11_context->device->CreateSamplerState(&desc, &d3d11_context->samplers[D3D11_SampleKind_NearestClamp]);
	}

	//~ Dedrick: Create nearest sampler.
	{
		ZoneScopedN("create nearest sampler");
		D3D11_SAMPLER_DESC desc = {};
		desc.Filter          = D3D11_FILTER_MIN_MAG_MIP_POINT;
		desc.AddressU        = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressV        = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressW        = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.ComparisonFunc  = D3D11_COMPARISON_NEVER;
		error = d3d11_context->device->CreateSamplerState(&desc, &d3d11_context->samplers[D3D11_SampleKind_Nearest]);
	}

	//~ Dedrick: Create linear clamp sampler.
	{
		ZoneScopedN("create linear clamp sampler");
		D3D11_SAMPLER_DESC desc = {};
		desc.Filter          = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		desc.AddressU        = D3D11_TEXTURE_ADDRESS_CLAMP;
		desc.AddressV        = D3D11_TEXTURE_ADDRESS_CLAMP;
		desc.AddressW        = D3D11_TEXTURE_ADDRESS_CLAMP;
		desc.ComparisonFunc  = D3D11_COMPARISON_NEVER;
		error = d3d11_context->device->CreateSamplerState(&desc, &d3d11_context->samplers[D3D11_SampleKind_LinearClamp]);
	}

	//~ Dedrick: Create linear sampler.
	{
		ZoneScopedN("create linear sampler");
		D3D11_SAMPLER_DESC desc = {};
		desc.Filter          = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		desc.AddressU        = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressV        = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressW        = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.ComparisonFunc  = D3D11_COMPARISON_NEVER;
		error = d3d11_context->device->CreateSamplerState(&desc, &d3d11_context->samplers[D3D11_SampleKind_Linear]);
	}
}

auto dk::d3d11_device() noexcept -> ID3D11Device1 * {
	return d3d11_context->device;
}

auto dk::d3d11_device_context() noexcept -> ID3D11DeviceContext1 * {
	return d3d11_context->device_context;
}

auto dk::d3d11_sampler(D3D11_SampleKind kind) noexcept -> ID3D11SamplerState * {
	DK_ASSERT(kind < D3D11_SampleKind_COUNT);
	return d3d11_context->samplers[kind];
}

#if defined(DK_DESKTOP_INCLUDED)
auto dk::d3d11_window_equip(RGFW_window *window) noexcept -> D3D11_Window * {
	ZoneScoped;

	//~ Dedrick: Allocate window state.
	D3D11_Window *d3d11_window = d3d11_context->first_free_window;
	if (d3d11_window == nullptr) {
		d3d11_window = arena_push<D3D11_Window>(d3d11_context->arena);
	}
	else {
		forward_list_stack_pop(&d3d11_context->first_free_window);
		memory_zero(d3d11_window);
	}

	//~ Dedrick: Obtain HWND.
	HWND const hwnd = static_cast<HWND>(RGFW_window_getHWND(window));

	//~ Dedrick: Create swapchain.
	DXGI_SWAP_CHAIN_DESC1 swapchain_desc = {};
	swapchain_desc.Width = 0; // NOTE(Dedrick): Inherit from window.
	swapchain_desc.Height = 0; // NOTE(Dedrick): Inherit from window.
	swapchain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapchain_desc.Stereo = FALSE;
	swapchain_desc.SampleDesc.Count = 1;
	swapchain_desc.SampleDesc.Quality = 0;
	swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapchain_desc.BufferCount = 2;
	swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapchain_desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	swapchain_desc.Flags = 0;
	HRESULT const error = d3d11_context->dxgi_factory->CreateSwapChainForHwnd(reinterpret_cast<IUnknown *>(d3d11_context->device), hwnd, &swapchain_desc, 0, 0, &d3d11_window->swapchain);
	if (FAILED(error)) {
		TempArena const scratch = scratch_begin(nullptr, 0);
		String8 const msg = str8f(scratch.arena, "DXGI swap chain creation failure (0x%lx). Terminating.", error);
		dt_show_dialog(window, "Fatal Error"_str8, msg, true);
		abort_self(1);
	}

	//~ Dedrick: Disable alt + enter fullscreen shortcut.
	d3d11_context->dxgi_factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

	//~ Dedrick: Create swapchain view.
	{
		ID3D11Texture2D *backbuffer = nullptr;
		D3D11_RENDER_TARGET_VIEW_DESC swapchain_rtv_desc = {};
		swapchain_rtv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapchain_rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		d3d11_window->swapchain->GetBuffer(0, IID_PPV_ARGS(&backbuffer));
		d3d11_context->device->CreateRenderTargetView(backbuffer, &swapchain_rtv_desc, &d3d11_window->swapchain_rtv);
		backbuffer->Release();
	}

	return d3d11_window;
}

auto dk::d3d11_window_unequip(RGFW_window *window, D3D11_Window *window_equip) noexcept -> void {
	ZoneScoped;
	(void)window;
	window_equip->swapchain_rtv->Release();
	window_equip->swapchain->Release();
	forward_list_stack_push(&d3d11_context->first_free_window, window_equip);
}

auto dk::d3d11_window_resize_swapchain_if_needed(RGFW_window *window, D3D11_Window *window_equip) noexcept -> void {
	ZoneScoped;

	//~ Dedrick: Get resolution in pixels.
	ivec2 resolution = {};
	{
		i32 w = 0, h = 0;
		RGFW_window_getSizeInPixels(window, &w, &h);
		resolution = { w, h };
	}

	//~ Dedrick: Resolution change.
	if (window_equip->last_resolution.x != resolution.x || window_equip->last_resolution.y != resolution.y) {
		window_equip->last_resolution = resolution;

		//~ Dedrick: Resize swapchain.
		window_equip->swapchain_rtv->Release();
		window_equip->swapchain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
		ID3D11Texture2D *backbuffer = nullptr;
		window_equip->swapchain->GetBuffer(0, IID_PPV_ARGS(&backbuffer));
		D3D11_RENDER_TARGET_VIEW_DESC swapchain_rtv_desc = {};
		swapchain_rtv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapchain_rtv_desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		d3d11_context->device->CreateRenderTargetView(backbuffer, &swapchain_rtv_desc, &window_equip->swapchain_rtv);
		backbuffer->Release();
	}
}
#endif

dk::String8 const dk::d3d11_format_name_table[192] = {
	"UNKNOWN"_str8,
	"R32G32B32A32_TYPELESS"_str8,
	"R32G32B32A32_FLOAT"_str8,
	"R32G32B32A32_UINT"_str8,
	"R32G32B32A32_SINT"_str8,
	"R32G32B32_TYPELESS"_str8,
	"R32G32B32_FLOAT"_str8,
	"R32G32B32_UINT"_str8,
	"R32G32B32_SINT"_str8,
	"R16G16B16A16_TYPELESS"_str8,
	"R16G16B16A16_FLOAT"_str8,
	"R16G16B16A16_UNORM"_str8,
	"R16G16B16A16_UINT"_str8,
	"R16G16B16A16_SNORM"_str8,
	"R16G16B16A16_SINT"_str8,
	"R32G32_TYPELESS"_str8,
	"R32G32_FLOAT"_str8,
	"R32G32_UINT"_str8,
	"R32G32_SINT"_str8,
	"R32G8X24_TYPELESS"_str8,
	"D32_FLOAT_S8X24_UINT"_str8,
	"R32_FLOAT_X8X24_TYPELESS"_str8,
	"X32_TYPELESS_G8X24_UINT"_str8,
	"R10G10B10A2_TYPELESS"_str8,
	"R10G10B10A2_UNORM"_str8,
	"R10G10B10A2_UINT"_str8,
	"R11G11B10_FLOAT"_str8,
	"R8G8B8A8_TYPELESS"_str8,
	"R8G8B8A8_UNORM"_str8,
	"R8G8B8A8_UNORM_SRGB"_str8,
	"R8G8B8A8_UINT"_str8,
	"R8G8B8A8_SNORM"_str8,
	"R8G8B8A8_SINT"_str8,
	"R16G16_TYPELESS"_str8,
	"R16G16_FLOAT"_str8,
	"R16G16_UNORM"_str8,
	"R16G16_UINT"_str8,
	"R16G16_SNORM"_str8,
	"R16G16_SINT"_str8,
	"R32_TYPELESS"_str8,
	"D32_FLOAT"_str8,
	"R32_FLOAT"_str8,
	"R32_UINT"_str8,
	"R32_SINT"_str8,
	"R24G8_TYPELESS"_str8,
	"D24_UNORM_S8_UINT"_str8,
	"R24_UNORM_X8_TYPELESS"_str8,
	"X24_TYPELESS_G8_UINT"_str8,
	"R8G8_TYPELESS"_str8,
	"R8G8_UNORM"_str8,
	"R8G8_UINT"_str8,
	"R8G8_SNORM"_str8,
	"R8G8_SINT"_str8,
	"R16_TYPELESS"_str8,
	"R16_FLOAT"_str8,
	"D16_UNORM"_str8,
	"R16_UNORM"_str8,
	"R16_UINT"_str8,
	"R16_SNORM"_str8,
	"R16_SINT"_str8,
	"R8_TYPELESS"_str8,
	"R8_UNORM"_str8,
	"R8_UINT"_str8,
	"R8_SNORM"_str8,
	"R8_SINT"_str8,
	"A8_UNORM"_str8,
	"R1_UNORM"_str8,
	"R9G9B9E5_SHAREDEXP"_str8,
	"R8G8_B8G8_UNORM"_str8,
	"G8R8_G8B8_UNORM"_str8,
	"BC1_TYPELESS"_str8,
	"BC1_UNORM"_str8,
	"BC1_UNORM_SRGB"_str8,
	"BC2_TYPELESS"_str8,
	"BC2_UNORM"_str8,
	"BC2_UNORM_SRGB"_str8,
	"BC3_TYPELESS"_str8,
	"BC3_UNORM"_str8,
	"BC3_UNORM_SRGB"_str8,
	"BC4_TYPELESS"_str8,
	"BC4_UNORM"_str8,
	"BC4_SNORM"_str8,
	"BC5_TYPELESS"_str8,
	"BC5_UNORM"_str8,
	"BC5_SNORM"_str8,
	"B5G6R5_UNORM"_str8,
	"B5G5R5A1_UNORM"_str8,
	"B8G8R8A8_UNORM"_str8,
	"B8G8R8X8_UNORM"_str8,
	"R10G10B10_XR_BIAS_A2_UNORM"_str8,
	"B8G8R8A8_TYPELESS"_str8,
	"B8G8R8A8_UNORM_SRGB"_str8,
	"B8G8R8X8_TYPELESS"_str8,
	"B8G8R8X8_UNORM_SRGB"_str8,
	"BC6H_TYPELESS"_str8,
	"BC6H_UF16"_str8,
	"BC6H_SF16"_str8,
	"BC7_TYPELESS"_str8,
	"BC7_UNORM"_str8,
	"BC7_UNORM_SRGB"_str8,
	"AYUV"_str8,
	"Y410"_str8,
	"Y416"_str8,
	"NV12"_str8,
	"P010"_str8,
	"P016"_str8,
	"420_OPAQUE"_str8,
	"YUY2"_str8,
	"Y210"_str8,
	"Y216"_str8,
	"NV11"_str8,
	"AI44"_str8,
	"IA44"_str8,
	"P8"_str8,
	"A8P8"_str8,
	"B4G4R4A4_UNORM"_str8,
	"P208"_str8,
	"V208"_str8,
	"V408"_str8,
	"SAMPLER_FEEDBACK_MIN_MIP_OPAQUE"_str8,
	"SAMPLER_FEEDBACK_MIP_REGION_USED_OPAQUE"_str8,
	"A4B4G4R4_UNORM"_str8,
};
