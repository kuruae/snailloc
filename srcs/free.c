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

static t_zone_header *find_large_zone(const void *ptr) {
    pthread_mutex_lock(&g_large_mutex);
    t_zone_header *zone = g_large_zones;
    while (zone) {
        if ((const char *)ptr >= (const char *)zone &&
            (const char *)ptr < (const char *)zone + zone->zone_size) {
            pthread_mutex_unlock(&g_large_mutex);
            return zone;
        }
        zone = zone->next;
    }
    pthread_mutex_unlock(&g_large_mutex);
    return NULL;
}

static void freelist_push(t_zone_header *zone, t_chunk_header *chunk) {
    t_chunk_header *head = atomic_load_explicit(&zone->free_list, memory_order_acquire);
    do {
        CHUNK_NEXT_FREE(chunk) = head;
    } while (!atomic_compare_exchange_weak_explicit(&zone->free_list, &head, chunk,
                                                    memory_order_release, memory_order_acquire));
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

    t_zone_header *zone = find_zone_by_addr(ptr);
    if (UNLIKELY(!zone))
        zone = find_large_zone(ptr);

    if (UNLIKELY(!zone)) {
        handle_error("free(): pointer not in any known zone", check_level);
        return;
    }

    t_chunk_header *chunk = get_chunk_from_ptr(ptr);

    uint8_t was_free = atomic_exchange_explicit(&chunk->free, 1, memory_order_acq_rel);
    if (UNLIKELY(was_free == 1)) {
        handle_error("free(): double free detected", check_level);
        return;
    }

    if (chunk->zone_type == ZONE_LARGE) {
        if (env_is_fill_on_free(flags))
            ft_bzero(ptr, chunk->size);
        remove_large_zone(zone);
        munmap(zone, zone->zone_size);
        return;
    }

    if (env_is_fill_on_free(flags))
        ft_bzero(ptr, chunk->size);

    freelist_push(zone, chunk);
}
