// Copyright (C) 2026 Koh Swee Teck Dedrick.
// Licensed under the MIT license (https://opensource.org/license/mit/).

#ifndef D3D11_HPP__
#define D3D11_HPP__

#ifndef DK_D3D11_INCLUDED
#	define DK_D3D11_INCLUDED
#endif

#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <DirectXPackedVector.h>

#pragma comment(lib, "user32")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "dxgi")

namespace dk {
	enum D3D11_SampleKind {
		D3D11_SampleKind_NearestClamp,
		D3D11_SampleKind_Nearest,
		D3D11_SampleKind_LinearClamp,
		D3D11_SampleKind_Linear,
		D3D11_SampleKind_COUNT,
	};

	struct D3D11_Window {
		D3D11_Window *next;

		//~ Dedrick: Swapchain.
		IDXGISwapChain1 *swapchain;
		ID3D11RenderTargetView *swapchain_rtv;
		ivec2 last_resolution;
	};

	struct D3D11_Context {
		Arena *arena;
		D3D11_Window *first_free_window;

		//~ Dedrick: Objects.
		ID3D11Device *base_device;
		ID3D11DeviceContext *base_device_context;
		ID3D11Device1 *device;
		ID3D11DeviceContext1 *device_context;
		IDXGIAdapter *dxgi_adapter;
		IDXGIFactory2 *dxgi_factory;
		ID3D11RasterizerState *default_rasterizer;
		ID3D11BlendState *default_blend_state;
		ID3D11SamplerState *samplers[D3D11_SampleKind_COUNT];
	};

	extern D3D11_Context *d3d11_context;

	auto d3d11_init(CmdLine *cmd_line) noexcept -> void;

	auto d3d11_device() noexcept -> ID3D11Device1 *;
	auto d3d11_device_context() noexcept -> ID3D11DeviceContext1 *;
	auto d3d11_sampler(D3D11_SampleKind kind) noexcept -> ID3D11SamplerState *;

#if defined(DK_DESKTOP_INCLUDED)
	auto d3d11_window_equip(RGFW_window *window) noexcept -> D3D11_Window *;
	auto d3d11_window_unequip(RGFW_window *window, D3D11_Window *window_equip) noexcept -> void;

	auto d3d11_window_resize_swapchain_if_needed(RGFW_window *window, D3D11_Window *window_equip) noexcept -> void;
#endif

	extern String8 const d3d11_format_name_table[192];
}

#endif // D3D11_HPP__
