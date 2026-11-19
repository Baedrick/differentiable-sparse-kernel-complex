// Copyright (C) 2026 Koh Swee Teck Dedrick.
// Licensed under the MIT license (https://opensource.org/license/mit/).

#ifndef BASE_LOG_HPP__
#define BASE_LOG_HPP__

namespace dk {
	enum LogKind {
		LogKind_Info = 0,
		LogKind_Error,
		LogKind_COUNT
	};

	struct LogNode {
		LogNode *next;
		String8 string;
		LogKind kind;
	};

	struct LogFrame {
		LogFrame *next;
		u64 arena_start_pos;
		LogNode *first;
		LogNode *last;
		u64 node_count;
		u64 total_string_size;
		u64 kind_counts[LogKind_COUNT];
	};

	struct LogEntry {
		u64 offset;
		u32 size;
		LogKind kind;
	};

	struct LogFrameResult {
		String8 string;
		LogEntry *entries;
		u64 count;
		u64 *kind_indices[LogKind_COUNT];
		u64 kind_counts[LogKind_COUNT];
	};

	struct LogContext {
		Arena *arena;
		LogFrame *stack;
	};

	struct LogScope {
		LogScope(LogKind kind, String8 str) noexcept;
		~LogScope();
		LogScope(LogScope const &) = delete;
		auto operator=(LogScope const &) -> LogScope & = delete;
		LogKind kind_;
	};

	auto log_alloc() noexcept -> LogContext *;
	auto log_release(LogContext *context) noexcept -> void;
	auto log_select(LogContext *context) noexcept -> void;

	auto log_frame_begin() noexcept -> void;
	auto log_frame_end(Arena *arena) noexcept -> LogFrameResult;

	auto log_msg(LogKind kind, String8 str) noexcept -> void;
	auto log_msgfv(LogKind kind, char const *fmt, va_list args) noexcept -> void;
	auto log_msgf(LogKind kind, char const *fmt, ...) noexcept -> void;
}

#define DK_LOG_INFO(s)       dk::log_msg(dk::LogKind_Info, (s))
#define DK_LOG_INFOF(...)    dk::log_msgf(dk::LogKind_Info, __VA_ARGS__)
#define DK_LOG_ERROR(s)      dk::log_msg(dk::LogKind_Error, (s))
#define DK_LOG_ERRORF(...)   dk::log_msgf(dk::LogKind_Error, __VA_ARGS__)

#define DK_LOG_INFO_SCOPE(s) dk::LogScope DK_GLUE(_log_scope_, __LINE__)(dk::LogKind_Info, (s))

#endif // BASE_LOG_HPP__
