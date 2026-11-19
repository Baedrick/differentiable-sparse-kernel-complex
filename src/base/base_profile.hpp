// Copyright (C) 2026 Koh Swee Teck Dedrick.
// Licensed under the MIT license (https://opensource.org/license/mit/).

#ifndef BASE_PROFILE_HPP__
#define BASE_PROFILE_HPP__

#if defined(DK_PROFILE_ENABLE)
#	define TRACY_ENABLE
#	if defined(DK_COMPILER_MSVC)
#		define TracyFunction __FUNCSIG__
#	endif
#	if defined(DK_PLATFORM_WIN32)
#		ifndef NOMINMAX
#			define NOMINMAX
#		endif
#	endif
#endif

#include "thirdparty/tracy/tracy/Tracy.hpp"

#endif // BASE_PROFILE_HPP__
