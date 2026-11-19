// Copyright (C) 2026 Koh Swee Teck Dedrick.
// Licensed under the MIT license (https://opensource.org/license/mit/).

#ifndef BASE_MEMORY_HPP__
#define BASE_MEMORY_HPP__

namespace dk {
	auto memory_reserve(u64 size) noexcept -> void *;
	auto memory_commit(void *ptr, u64 size) noexcept -> b8;
	auto memory_decommit(void *ptr, u64 size) noexcept -> b8;
	auto memory_release(void *ptr, u64 size) noexcept -> void;
}

#endif // BASE_MEMORY_HPP__
