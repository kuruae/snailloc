#include "alloc.h"
#include "chunk_utils.h"
#include "color_utils.h"
#include "env_flags.h"
#include "libft.h"
#include "log_utils.h"
#include "zones.h"
#include <stdlib.h>

static inline void handle_error(const char *msg, uint8_t check_level) {
  ft_putstr_fd((char *)color_error(), 2);
  ft_putstr_fd((char *)msg, 2);
  ft_putendl_fd((char *)color_reset(), 2);
  if (check_level >= 1)
    abort();
}

void free(void *ptr) {
  if (UNLIKELY(!ptr))
    return;

  log_free(ptr);

  t_env_flags *flags = env_flags_singleton();
  uint8_t check_level = env_get_check_level(flags);

  if (UNLIKELY((uintptr_t)ptr % ALIGNMENT) != 0) {
    handle_error("free(): misaligned pointer in free()", check_level);
    return;
  }

  t_chunk_header *chunk = get_chunk_from_ptr(ptr);

  uint8_t was_free = atomic_exchange_explicit(
      &chunk->free, 1, memory_order_acq_rel);
  if (UNLIKELY(was_free == 1)) {
    handle_error("free(): double free detected", check_level);
    return;
  }

  if (chunk->zone_type == ZONE_LARGE) {
    if (env_is_fill_on_free(flags))
      ft_bzero(ptr, chunk->size);
    t_zone_header *large_zone =
        (t_zone_header *)((char *)chunk - sizeof(t_zone_header));
    remove_large_zone(large_zone);
    munmap(large_zone, large_zone->zone_size);
    return;
  }

  if (env_is_fill_on_free(flags))
    ft_bzero(ptr, chunk->size);

  if (env_is_check_wild_ptr(flags)) {
    t_zone_header *zone = get_zone_from_chunk(chunk);
    if (is_zone_in_local_list(zone, chunk->zone_type))
      coalesce_forward(zone, chunk);
  }
}
