// Copyright (C) 2026 Koh Swee Teck Dedrick.
// Licensed under the MIT license (https://opensource.org/license/mit/).

#ifndef DESKTOP_HPP__
#define DESKTOP_HPP__

#ifndef DK_DESKTOP_INCLUDED
#	define DK_DESKTOP_INCLUDED
#endif

#include "desktop_rgfw.hpp"
#include "desktop_core.hpp"

#if defined(DK_PLATFORM_WIN32)
#	include "win32/win32_desktop.hpp"
#else
#	error "Desktop layer platform backend not implemented."
#endif

#endif // DESKTOP_HPP__
