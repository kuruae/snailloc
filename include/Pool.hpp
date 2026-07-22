#pragma once

#include "Mailbox.hpp"
#include "const.hpp"
#include <expected>
#include <sys/mman.h>
#include <thread>

struct alignas(CACHELINE_SIZE) Pool {
    static_assert(std::has_single_bit(POOL_SIZE));

    std::thread::id ownerThread;

    Mailbox mailbox{};

    uint32_t sizeClassIndex;
    uint32_t chunkSize;
    uint32_t chunksAllocated;
    uint32_t maxChunks;

    std::array<uint64_t, BITMAP_WORDS> freeMasks;

    static Pool *fromPtr(void *ptr) noexcept {
        return reinterpret_cast<Pool *>(reinterpret_cast<uintptr_t>(ptr) & POOL_MASK);
    }

    uint8_t *payloadStart() noexcept {
        constexpr size_t headerSize{(sizeof(Pool) + 15) & ~15};
        return reinterpret_cast<uint8_t *>(this) + headerSize;
    }
};

/**
 *  @brief Allocates a 2MB pool
 *
 *  2MB-alignement is needed so we can always find
 *  the owner of our pointer by rounding down the address
 *  to a multiple of 2MB.
 *  `mmap` cannot give the correct alignment so we ask 4MB
 *  and take only the 2MB aligned part, then give back the extra.
 *
 *  @return A pointer to a usable memory pool, or -1.
 */
[[nodiscard]]
static std::expected<void *, int> allocatePool() noexcept {
    constexpr size_t requestSize = POOL_SIZE * 2;
    void *rawMemoryPtr =
        ::mmap(nullptr, requestSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (rawMemoryPtr == MAP_FAILED) {
        return std::unexpected(-1);
    }

    uintptr_t rawAddress = reinterpret_cast<uintptr_t>(rawMemoryPtr);

    // Find the first 2MB aligned boundary
    uintptr_t alignedAddress = (rawAddress + POOL_SIZE - 1) & POOL_MASK;

    // give back extra memory
    size_t leftExtra = alignedAddress - rawAddress;
    if (leftExtra > 0) {
        ::munmap(reinterpret_cast<void *>(rawAddress), leftExtra);
    }
    size_t rightExtra = requestSize - leftExtra - POOL_SIZE;
    if (rightExtra > 0) {
        ::munmap(reinterpret_cast<void *>(alignedAddress + POOL_SIZE), rightExtra);
    }

    return reinterpret_cast<void *>(alignedAddress);
}
