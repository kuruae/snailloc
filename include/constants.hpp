#pragma once

#include <cstdint>
#include <new>

namespace snail {

constexpr auto POOL_SIZE = 2uz * 1024uz * 1024uz;
constexpr auto REGULAR_ALLOC_MAX = 64uz * 1024uz;
constexpr auto MIN_CHUNK_SIZE = 16uz;

constexpr auto SIZE_CLASS_SMALL_INCREMENT = MIN_CHUNK_SIZE;
constexpr auto SIZE_CLASS_SMALL_TRESHOLD = 128uz;
constexpr auto SIZE_CLASS_GROWTH_DIVISOR = 4uz;

constexpr auto MAX_CHUNKS_PER_POOL = POOL_SIZE / MIN_CHUNK_SIZE;
constexpr auto BITMAP_WORDS = MAX_CHUNKS_PER_POOL / 64uz;

constexpr auto CACHELINE_SIZE = std::hardware_destructive_interference_size;

constexpr std::uintptr_t POOL_MASK = ~(POOL_SIZE - 1);

} // namespace snail
