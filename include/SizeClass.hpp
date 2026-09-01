#pragma once

#include "constants.hpp"
#include <array>
#include <bit>

namespace snail::detail {

static_assert(std::has_single_bit(MIN_CHUNK_SIZE));

constexpr auto round_up_to_chunk(std::size_t n) noexcept -> std::size_t {
    return (n + MIN_CHUNK_SIZE - 1) & ~(MIN_CHUNK_SIZE - 1);
}

consteval auto count_size_classes() noexcept -> std::size_t {
    auto count = 1uz;
    auto size = MIN_CHUNK_SIZE;

    while (size < REGULAR_ALLOC_MAX) {
        if (size < SIZE_CLASS_SMALL_TRESHOLD)
            size += SIZE_CLASS_SMALL_INCREMENT;
        else
            size += (size / SIZE_CLASS_GROWTH_DIVISOR);

        size = round_up_to_chunk(size);
        ++count;
    }

    return count;
}

inline constexpr auto NUM_SIZE_CLASSES = count_size_classes();

} // namespace snail::detail
