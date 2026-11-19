// Copyright (C) 2026 Koh Swee Teck Dedrick.
// Licensed under the MIT license (https://opensource.org/license/mit/).

#ifndef BASE_HPP__
#define BASE_HPP__

#ifndef DK_BASE_INCLUDED
#	define DK_BASE_INCLUDED
#endif

#include "base_core.hpp"
#include "base_hash.hpp"
#include "base_profile.hpp"
#include "base_memory.hpp"
#include "base_arena.hpp"
#include "base_math.hpp"
#include "base_string.hpp"
#include "base_ring_buffer.hpp"
#include "base_thread.hpp"
#include "base_thread_context.hpp"
#include "base_file.hpp"
#include "base_system.hpp"
#include "base_log.hpp"
#include "base_command_line.hpp"
#include "base_entry_point.hpp"

#if defined(DK_PLATFORM_WIN32)
#	include "win32/win32_base.hpp"
#else
#	error "Base layer platform backend not implemented."
#endif

#endif // BASE_HPP__
