// Copyright (C) 2026 Koh Swee Teck Dedrick.
// Licensed under the MIT license (https://opensource.org/license/mit/).

//~ Dedrick: Build options.
#define DK_BUILD_GRAPHICAL

//~ Dedrick: [hpp]
#include "base/base.hpp"
#include "desktop/desktop.hpp"
#include "d3d11/d3d11.hpp"
#include "dds/dds.hpp"
#include "ui/ui.hpp"
#include "playground/playground.hpp"

//~ Dedrick: [cpp]
#include "base/base.cpp"
#include "desktop/desktop.cpp"
#include "d3d11/d3d11.cpp"
#include "dds/dds.cpp"
#include "ui/ui.cpp"
#include "playground/playground.cpp"

auto dk::entry_point(CmdLine *cmd_line) noexcept -> int {
	pg_init(cmd_line);
	for (b8 quit = false; !quit; ) {
		quit = pg_frame();
		FrameMark;
	}
	pg_shutdown();
	return 0;
}
