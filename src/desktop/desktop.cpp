// Copyright (C) 2026 Koh Swee Teck Dedrick.
// Licensed under the MIT license (https://opensource.org/license/mit/).

#define RGFW_IMPLEMENTATION
#pragma warning(push, 3)
#include "desktop_rgfw.hpp"
#pragma warning(pop)
#undef RGFW_IMPLEMENTATION

#include "desktop_core.cpp"

#if defined(DK_PLATFORM_WIN32)
#	include "win32/win32_desktop.cpp"
#else
#	error "Desktop layer platform backend not implemented."
#endif
