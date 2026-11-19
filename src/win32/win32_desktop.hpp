// Copyright (C) 2026 Koh Swee Teck Dedrick.
// Licensed under the MIT license (https://opensource.org/license/mit/).

#ifndef WIN32_DESKTOP_HPP__
#define WIN32_DESKTOP_HPP__

#include <wrl/client.h>
#include <shellapi.h>

namespace dk {
	FILEOPENDIALOGOPTIONS constexpr W32_DT_FILE_DIALOG_COMMON_FLAGS =
		FOS_PATHMUSTEXIST
		| FOS_FORCEFILESYSTEM
		| FOS_NOCHANGEDIR;

	auto w32_dt_create_filter_specs(Arena *arena, DT_FileDialogFilter const *filters, u64 filter_count, UINT *out_count) -> COMDLG_FILTERSPEC *;
}

#endif // WIN32_DESKTOP_HPP__
