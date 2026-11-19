// Copyright (C) 2026 Koh Swee Teck Dedrick.
// Licensed under the MIT license (https://opensource.org/license/mit/).

#ifndef DESKTOP_CORE_HPP__
#define DESKTOP_CORE_HPP__

namespace dk {
	struct DT_EventNode {
		DT_EventNode *next;
		RGFW_event event;
	};

	struct DT_EventList {
		DT_EventNode *first;
		DT_EventNode *last;
		u64 count;
	};

	struct DT_FileDialogFilter {
		String8 display_name; ///< e.g. "Text files"
		String8 extensions; ///< e.g. "txt,text"
	};

	struct DT_Context {
		Arena *arena;
		RGFW_info *rgfw_info;
		RGFW_window *first_window;
		RGFW_window *last_window;
		RGFW_window *free_window;
	};

	extern DT_Context *dt_context;
	extern DT_EventList dt_event_list;
	extern Arena *dt_event_arena;

	auto dt_init() noexcept -> void;
	auto dt_platform_init() noexcept -> void;

	auto dt_window_alloc() noexcept -> RGFW_window *;
	auto dt_window_release(RGFW_window *window) noexcept -> void;
	auto dt_rgfw_event_callback(RGFW_event const *event) noexcept -> void;

	auto dt_window_open(String8 title, s32 x, s32 y, s32 w, s32 h, RGFW_windowFlags flags) noexcept -> RGFW_window *;
	auto dt_window_close(RGFW_window *window) noexcept -> void;

	auto dt_get_events(Arena *arena) noexcept -> DT_EventList;

	auto dt_show_dialog(RGFW_window const *parent, String8 title, String8 message, b8 error) noexcept -> void;

	auto dt_show_in_file_browser(String8 path) noexcept -> void;
	auto dt_file_dialog_pick_file(Arena *arena, RGFW_window const *parent, DT_FileDialogFilter const *filters, u64 filter_count) noexcept -> String8;
	auto dt_file_dialog_pick_multiple_files(Arena *arena, RGFW_window const *parent, DT_FileDialogFilter const *filters, u64 filter_count) noexcept -> String8List;
	auto dt_file_dialog_save(Arena *arena, RGFW_window const *parent, String8 default_name, DT_FileDialogFilter const *filters, u64 filter_count, u64 *out_filter_index) noexcept -> String8;
	auto dt_file_dialog_pick_folder(Arena *arena, RGFW_window const *parent) noexcept -> String8;
}

#endif // DESKTOP_CORE_HPP__
