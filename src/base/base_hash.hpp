// Copyright (C) 2026 Koh Swee Teck Dedrick.
// Licensed under the MIT license (https://opensource.org/license/mit/).

#ifndef BASE_HASH_HPP__
#define BASE_HASH_HPP__

namespace dk {
	auto u64_hash_from_seed_data(u64 seed, void const *data, u64 size) noexcept -> u64;
	auto u64_hash_from_data(void const *data, u64 size) noexcept -> u64;
}

#endif // BASE_HASH_HPP__
