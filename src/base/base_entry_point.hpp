// Copyright (C) 2026 Koh Swee Teck Dedrick.
// Licensed under the MIT license (https://opensource.org/license/mit/).

#ifndef BASE_ENTRY_POINT_HPP__
#define BASE_ENTRY_POINT_HPP__

namespace dk {
	// NOTE(Dedrick): To be defined by application.
	extern auto entry_point(CmdLine *cmd_line) noexcept -> int;

	auto main_thread_entry_point(int argc, char **argv) noexcept -> int;
	auto thread_entry_point(void (*func)(void *params), void *func_params) noexcept -> void;
}

#endif // BASE_ENTRY_POINT_HPP__
