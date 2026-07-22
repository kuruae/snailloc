#include <cstddef>
#include <cstdint>
#include <new>

constexpr size_t POOL_SIZE = 2 * 1024 * 1024;
constexpr size_t REGULAR_ALLOC_MAX = 64 * 1024;
constexpr uintptr_t POOL_MASK = ~(POOL_SIZE - 1);

constexpr size_t MIN_CHUNK_SIZE = 16;
constexpr size_t MAX_CHUNKS_PER_POOL = POOL_SIZE / MIN_CHUNK_SIZE;
constexpr size_t BITMAP_WORDS = MAX_CHUNKS_PER_POOL / 64;

constexpr size_t CACHELINE_SIZE = std::hardware_destructive_interference_size;
