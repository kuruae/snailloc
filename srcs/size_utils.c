#include "size_utils.h"
#include "alloc.h"

size_t g_system_page_size;
size_t g_tiny_max;
size_t g_small_max;

__attribute__((constructor)) static void init_size_globals(void) {
    g_system_page_size = (size_t)sysconf(_SC_PAGESIZE);
    g_tiny_max = g_system_page_size / TINY_RATIO;
    g_small_max = g_system_page_size / SMALL_RATIO;
}

size_t get_zone_size(size_t max_type_size) {
    size_t header_size = sizeof(t_zone_header) + sizeof(t_chunk_header);
    size_t needed = (max_type_size + header_size) * MIN_ALLOC_COUNT;

    return (needed + (g_system_page_size - 1)) & ~(g_system_page_size - 1);
}
