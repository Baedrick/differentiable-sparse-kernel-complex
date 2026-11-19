// Copyright (C) 2026 Koh Swee Teck Dedrick.
// Licensed under the MIT license (https://opensource.org/license/mit/).

#ifndef BASE_RING_BUFFER_HPP__
#define BASE_RING_BUFFER_HPP__

namespace dk {
	struct RingBuffer {
		u8 *base;
		u64 size;
		u64 read_pos;
		u64 write_pos;
	};

	auto ring_write(RingBuffer *ring, void const *src, u64 src_size) noexcept -> void;
	auto ring_read(RingBuffer *ring, void *dst, u64 dst_size) noexcept -> void;
}

#endif // BASE_RING_BUFFER_HPP__
