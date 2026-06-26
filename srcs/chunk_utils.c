#include "chunk_utils.h"
#include "alloc.h"
#include "compiler_attrs.h"

t_chunk_header *find_free_chunk(t_zone_header *zone, size_t size) {
	t_chunk_header *head = atomic_load_explicit(&zone->free_list, memory_order_acquire);
	while (head) {
		t_chunk_header *next = CHUNK_NEXT_FREE(head);
		if (head->size >= size) {
			if (atomic_compare_exchange_weak_explicit(&zone->free_list,
			    &head, next, memory_order_acquire, memory_order_relaxed))
				return head;
		} else {
			break;
		}
	}
	return NULL;
}

static inline size_t align_size(size_t size) { return (size + (ALIGNMENT - 1)) & ~(ALIGNMENT - 1); }

void *carve_chunk(t_zone_header *zone, size_t size) {
    size_t aligned_size = align_size(size);
    size_t needed = aligned_size + sizeof(t_chunk_header);

    if (UNLIKELY((char *)zone->break_ptr + needed > (char *)zone + zone->zone_size))
        return NULL;

    t_chunk_header *new_chunk = (t_chunk_header *)zone->break_ptr;

    new_chunk->zone_off = (uint32_t)((char *)new_chunk - (char *)zone);
    atomic_store_explicit(&new_chunk->free, 0, memory_order_release);
    new_chunk->size = aligned_size;
    new_chunk->zone_type = zone->type;

    zone->break_ptr = (char *)zone->break_ptr + needed;

    return get_ptr_from_chunk(new_chunk);
}
