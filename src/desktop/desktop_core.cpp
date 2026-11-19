// Copyright (C) 2026 Koh Swee Teck Dedrick.
// Licensed under the MIT license (https://opensource.org/license/mit/).

dk::DT_Context *dk::dt_context;
dk::DT_EventList dk::dt_event_list;
dk::Arena *dk::dt_event_arena;

auto dk::dt_window_alloc() noexcept -> RGFW_window * {
	RGFW_window *result = dt_context->free_window;
	if (result != nullptr) {
		forward_list_stack_pop(&dt_context->free_window);
		memory_zero(result);
	}
	else {
		result = arena_push<RGFW_window>(dt_context->arena);
	}
	return result;
}

auto dk::dt_window_release(RGFW_window *window) noexcept -> void {
	list_remove(&dt_context->first_window, &dt_context->last_window, window);
	forward_list_stack_push(&dt_context->free_window, window);
}

auto dk::dt_rgfw_event_callback(RGFW_event const *event) noexcept -> void {
	if (dt_event_arena == nullptr) {
		return;
	}
	DT_EventNode *node = arena_push<DT_EventNode>(dt_event_arena);
	memory_copy_struct(&node->event, event);
	switch (event->type) {
		case RGFW_dataDrop: {
			RGFW_dataDropNode *drop = arena_push<RGFW_dataDropNode>(dt_event_arena);
			memory_copy_struct(drop, event->drop.value);
			char *data = arena_push_array<char>(dt_event_arena, drop->length);
			memory_copy(data, drop->data, drop->length);
			drop->data = data;
			node->event.drop.value = drop;
			break;
		}
		case RGFW_monitorConnected: [[fallthrough]];
		case RGFW_monitorDisconnected: {
			RGFW_monitor *monitor = arena_push<RGFW_monitor>(dt_event_arena);
			memory_copy_struct(monitor, event->monitor.monitor);
			node->event.monitor.monitor = monitor;
			break;
		}
	}
	forward_list_queue_push(&dt_event_list.first, &dt_event_list.last, node);
}

auto dk::dt_init() noexcept -> void {
	//~ Dedrick: Do platform specific part of the work.
	dt_platform_init();

	//~ Dedrick: Top level initialization.
	Arena *arena = arena_alloc();
	dt_context = arena_push<DT_Context>(arena);
	dt_context->arena = arena;

	//~ Dedrick: Initialize RGFW state.
	dt_context->rgfw_info = static_cast<RGFW_info *>(arena_push_bytes_no_zero(arena, RGFW_sizeofInfo(), alignof(void *)));
	i32 const error = RGFW_init_ptr(nullptr, 0, dt_context->rgfw_info);
	if (error < 0) {
		TempArena const scratch = scratch_begin(nullptr, 0);
		dt_show_dialog(nullptr, "Fatal Error"_str8, "RGFW failed to initialize."_str8, true);
		abort_self(1);
	}

	//~ Dedrick: Set up for our own window queue.
	RGFW_setQueueEvents(RGFW_FALSE);
	RGFW_setAllEventCallbacks(dt_rgfw_event_callback, nullptr);
}

auto dk::dt_window_open(String8 title, s32 x, s32 y, s32 w, s32 h, RGFW_windowFlags flags) noexcept -> RGFW_window * {
	RGFW_window *window = dt_window_alloc();
	window = RGFW_createWindowPtr(reinterpret_cast<char const *>(title.data), x, y, w, h, flags, window);
	list_push_back(&dt_context->first_window, &dt_context->last_window, window);
	return window;
}

auto dk::dt_window_close(RGFW_window *window) noexcept -> void {
	if (window == nullptr) {
		return;
	}
	RGFW_window_closePtr(window);
	dt_window_release(window);
}

auto dk::dt_get_events(Arena *arena) noexcept -> DT_EventList {
	dt_event_arena = arena;
	memory_zero(&dt_event_list);
	RGFW_pollEvents();
	return dt_event_list;
}
