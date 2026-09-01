#pragma once

#include <cstdint>
#include <expected>

namespace snail {

template <typename F>
concept SizeClassifier = std::regular_invocable<const F &> &&
                         std::same_as<std::invoke_result_t<const F &, std::size_t>, std::uint32_t>;

template <typename F>
concept PoolAllocator = std::regular_invocable<const F &> &&
                        std::same_as<std::invoke_result_t<const F &>, std::expected<void *, int>>;

} // namespace snail
